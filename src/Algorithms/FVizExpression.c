#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Algorithms/FVizExpression.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef enum FVizExpressionNodeKind
{
    FVIZ_EXPR_CONSTANT,
    FVIZ_EXPR_VARIABLE,
    FVIZ_EXPR_COMPONENT,
    FVIZ_EXPR_NEGATE,
    FVIZ_EXPR_ADD,
    FVIZ_EXPR_SUBTRACT,
    FVIZ_EXPR_MULTIPLY,
    FVIZ_EXPR_DIVIDE,
    FVIZ_EXPR_POWER,
    FVIZ_EXPR_EQUAL,
    FVIZ_EXPR_NOT_EQUAL,
    FVIZ_EXPR_LESS,
    FVIZ_EXPR_LESS_EQUAL,
    FVIZ_EXPR_GREATER,
    FVIZ_EXPR_GREATER_EQUAL,
    FVIZ_EXPR_CONDITIONAL,
    FVIZ_EXPR_FUNCTION
} FVizExpressionNodeKind;

typedef enum FVizExpressionFunction
{
    FVIZ_EXPR_ABS,
    FVIZ_EXPR_SQRT,
    FVIZ_EXPR_EXP,
    FVIZ_EXPR_LOG,
    FVIZ_EXPR_LOG10,
    FVIZ_EXPR_SIN,
    FVIZ_EXPR_COS,
    FVIZ_EXPR_TAN,
    FVIZ_EXPR_FLOOR,
    FVIZ_EXPR_CEIL,
    FVIZ_EXPR_ROUND,
    FVIZ_EXPR_MIN,
    FVIZ_EXPR_MAX,
    FVIZ_EXPR_POW,
    FVIZ_EXPR_CLAMP,
    FVIZ_EXPR_MAG,
    FVIZ_EXPR_DOT,
    FVIZ_EXPR_CROSS,
    FVIZ_EXPR_NORMALIZE,
    FVIZ_EXPR_VEC2,
    FVIZ_EXPR_VEC3,
    FVIZ_EXPR_VEC4,
    FVIZ_EXPR_WHERE
} FVizExpressionFunction;

typedef struct FVizExpressionNode
{
    FVizExpressionNodeKind kind;
    FVizExpressionFunction function;
    int child[4];
    uint32_t child_count;
    uint32_t variable;
    uint32_t component;
    double constant;
} FVizExpressionNode;

struct FVizExpression
{
    FVizObject base;
    char* source;
    FVizExpressionNode* nodes;
    FVizSize node_count;
    FVizSize node_capacity;
    char** variable_names;
    FVizSize variable_count;
    FVizSize variable_capacity;
    int root;
    FVizSize error_offset;
};

typedef struct FVizExpressionCacheEntry
{
    char* source;
    FVizExpression* expression;
    uint64_t last_use;
} FVizExpressionCacheEntry;

struct FVizExpressionCache
{
    FVizObject base;
    FVizArray* entries;
    FVizSize capacity;
    uint64_t use_serial;
};

typedef struct FVizExpressionParser
{
    FVizExpression* expression;
    const char* source;
    const char* cursor;
    FVizBool failed;
    const char* message;
} FVizExpressionParser;

typedef struct FVizExpressionResolvedBinding
{
    const unsigned char* data;
    FVizSize stride;
    FVizDataType type;
    uint32_t components;
} FVizExpressionResolvedBinding;

typedef struct FVizExpressionValue
{
    uint32_t components;
    double value[FVIZ_EXPRESSION_MAX_COMPONENTS];
} FVizExpressionValue;

typedef struct FVizExpressionEvaluation
{
    const FVizExpression* expression;
    const FVizExpressionResolvedBinding* bindings;
    double* output;
    uint32_t output_components;
    FVizExpressionOptions options;
    FVizBool non_finite_found;
} FVizExpressionEvaluation;

static void fviz_expression_destroy(FVizObject* object);

static const FVizObjectClass g_fviz_expression_class = {
    FVIZ_TYPE_EXPRESSION,
    "FVizExpression",
    &g_fviz_object_class,
    fviz_expression_destroy,
    NULL
};

static void fviz_expression_cache_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_expression_cache_class = {
    FVIZ_TYPE_EXPRESSION_CACHE,
    "FVizExpressionCache",
    &g_fviz_object_class,
    fviz_expression_cache_destroy,
    NULL
};

static void fviz_expression_destroy(FVizObject* object)
{
    FVizExpression* expression = (FVizExpression*)object;
    FVizSize index;
    for (index = 0u; index < expression->variable_count; ++index)
        fviz_free(expression->variable_names[index]);
    fviz_free(expression->variable_names);
    fviz_free(expression->nodes);
    fviz_free(expression->source);
}

static void fviz_expression_parser_fail(
    FVizExpressionParser* parser, const char* message)
{
    if (parser->failed != FVIZ_FALSE) return;
    parser->failed = FVIZ_TRUE;
    parser->message = message;
    parser->expression->error_offset = (FVizSize)(parser->cursor - parser->source);
}

static void fviz_expression_skip_space(FVizExpressionParser* parser)
{
    while (isspace((unsigned char)*parser->cursor) != 0) ++parser->cursor;
}

static int fviz_expression_add_node(
    FVizExpressionParser* parser, const FVizExpressionNode* node)
{
    FVizExpression* expression = parser->expression;
    if (expression->node_count == expression->node_capacity)
    {
        const FVizSize capacity = expression->node_capacity == 0u
            ? 32u : expression->node_capacity * 2u;
        FVizExpressionNode* nodes;
        if (capacity > 4096u)
        {
            fviz_expression_parser_fail(parser, "expression is too complex");
            return -1;
        }
        nodes = (FVizExpressionNode*)fviz_realloc(
            expression->nodes, capacity * sizeof(*nodes));
        if (nodes == NULL)
        {
            fviz_expression_parser_fail(parser, "expression allocation failed");
            return -1;
        }
        expression->nodes = nodes;
        expression->node_capacity = capacity;
    }
    expression->nodes[expression->node_count] = *node;
    return (int)expression->node_count++;
}

static int fviz_expression_variable(
    FVizExpressionParser* parser, const char* name, FVizSize length)
{
    FVizExpression* expression = parser->expression;
    FVizSize index;
    for (index = 0u; index < expression->variable_count; ++index)
    {
        if (strlen(expression->variable_names[index]) == length &&
            strncmp(expression->variable_names[index], name, length) == 0)
            return (int)index;
    }
    if (expression->variable_count == expression->variable_capacity)
    {
        const FVizSize capacity = expression->variable_capacity == 0u
            ? 8u : expression->variable_capacity * 2u;
        char** names;
        if (capacity > 256u)
        {
            fviz_expression_parser_fail(parser, "expression has too many variables");
            return -1;
        }
        names = (char**)fviz_realloc(
            expression->variable_names, capacity * sizeof(*names));
        if (names == NULL)
        {
            fviz_expression_parser_fail(parser, "expression allocation failed");
            return -1;
        }
        expression->variable_names = names;
        expression->variable_capacity = capacity;
    }
    expression->variable_names[expression->variable_count] =
        (char*)fviz_alloc(length + 1u);
    if (expression->variable_names[expression->variable_count] == NULL)
    {
        fviz_expression_parser_fail(parser, "expression allocation failed");
        return -1;
    }
    (void)memcpy(expression->variable_names[expression->variable_count], name, length);
    expression->variable_names[expression->variable_count][length] = '\0';
    return (int)expression->variable_count++;
}

static int fviz_expression_parse_conditional(FVizExpressionParser* parser);

static FVizBool fviz_expression_function(
    const char* name,
    FVizSize length,
    FVizExpressionFunction* out_function,
    uint32_t* out_arity)
{
#define FVIZ_FUNCTION(text, value, arity) \
    if (length == sizeof(text) - 1u && strncmp(name, text, length) == 0) \
    { *out_function = value; *out_arity = arity; return FVIZ_TRUE; }
    FVIZ_FUNCTION("abs", FVIZ_EXPR_ABS, 1u)
    FVIZ_FUNCTION("sqrt", FVIZ_EXPR_SQRT, 1u)
    FVIZ_FUNCTION("exp", FVIZ_EXPR_EXP, 1u)
    FVIZ_FUNCTION("log", FVIZ_EXPR_LOG, 1u)
    FVIZ_FUNCTION("log10", FVIZ_EXPR_LOG10, 1u)
    FVIZ_FUNCTION("sin", FVIZ_EXPR_SIN, 1u)
    FVIZ_FUNCTION("cos", FVIZ_EXPR_COS, 1u)
    FVIZ_FUNCTION("tan", FVIZ_EXPR_TAN, 1u)
    FVIZ_FUNCTION("floor", FVIZ_EXPR_FLOOR, 1u)
    FVIZ_FUNCTION("ceil", FVIZ_EXPR_CEIL, 1u)
    FVIZ_FUNCTION("round", FVIZ_EXPR_ROUND, 1u)
    FVIZ_FUNCTION("min", FVIZ_EXPR_MIN, 2u)
    FVIZ_FUNCTION("max", FVIZ_EXPR_MAX, 2u)
    FVIZ_FUNCTION("pow", FVIZ_EXPR_POW, 2u)
    FVIZ_FUNCTION("clamp", FVIZ_EXPR_CLAMP, 3u)
    FVIZ_FUNCTION("mag", FVIZ_EXPR_MAG, 1u)
    FVIZ_FUNCTION("magnitude", FVIZ_EXPR_MAG, 1u)
    FVIZ_FUNCTION("length", FVIZ_EXPR_MAG, 1u)
    FVIZ_FUNCTION("dot", FVIZ_EXPR_DOT, 2u)
    FVIZ_FUNCTION("cross", FVIZ_EXPR_CROSS, 2u)
    FVIZ_FUNCTION("normalize", FVIZ_EXPR_NORMALIZE, 1u)
    FVIZ_FUNCTION("vec2", FVIZ_EXPR_VEC2, 2u)
    FVIZ_FUNCTION("vec3", FVIZ_EXPR_VEC3, 3u)
    FVIZ_FUNCTION("vec4", FVIZ_EXPR_VEC4, 4u)
    FVIZ_FUNCTION("where", FVIZ_EXPR_WHERE, 3u)
#undef FVIZ_FUNCTION
    return FVIZ_FALSE;
}

static int fviz_expression_parse_primary(FVizExpressionParser* parser)
{
    FVizExpressionNode node;
    fviz_expression_skip_space(parser);
    (void)memset(&node, 0, sizeof(node));
    if (*parser->cursor == '(')
    {
        int result;
        ++parser->cursor;
        result = fviz_expression_parse_conditional(parser);
        fviz_expression_skip_space(parser);
        if (*parser->cursor != ')')
            fviz_expression_parser_fail(parser, "expected ')'");
        else
            ++parser->cursor;
        return result;
    }
    if (isdigit((unsigned char)*parser->cursor) != 0 || *parser->cursor == '.')
    {
        char* end = NULL;
        node.kind = FVIZ_EXPR_CONSTANT;
        node.constant = strtod(parser->cursor, &end);
        if (end == parser->cursor)
        {
            fviz_expression_parser_fail(parser, "invalid numeric literal");
            return -1;
        }
        parser->cursor = end;
        return fviz_expression_add_node(parser, &node);
    }
    if (isalpha((unsigned char)*parser->cursor) != 0 || *parser->cursor == '_')
    {
        const char* name = parser->cursor;
        FVizSize length;
        int result;
        while (isalnum((unsigned char)*parser->cursor) != 0 || *parser->cursor == '_')
            ++parser->cursor;
        length = (FVizSize)(parser->cursor - name);
        fviz_expression_skip_space(parser);
        if (*parser->cursor == '(')
        {
            FVizExpressionFunction function;
            uint32_t arity = 0u;
            uint32_t count = 0u;
            if (fviz_expression_function(name, length, &function, &arity) == FVIZ_FALSE)
            {
                fviz_expression_parser_fail(parser, "unknown expression function");
                return -1;
            }
            ++parser->cursor;
            fviz_expression_skip_space(parser);
            if (*parser->cursor != ')')
            {
                for (;;)
                {
                    if (count >= 4u)
                    {
                        fviz_expression_parser_fail(parser, "function has too many arguments");
                        return -1;
                    }
                    node.child[count++] = fviz_expression_parse_conditional(parser);
                    fviz_expression_skip_space(parser);
                    if (*parser->cursor != ',') break;
                    ++parser->cursor;
                }
            }
            if (*parser->cursor != ')')
                fviz_expression_parser_fail(parser, "expected ')' after function arguments");
            else
                ++parser->cursor;
            if (count != arity)
                fviz_expression_parser_fail(parser, "expression function arity mismatch");
            node.kind = FVIZ_EXPR_FUNCTION;
            node.function = function;
            node.child_count = count;
            return parser->failed == FVIZ_FALSE
                ? fviz_expression_add_node(parser, &node) : -1;
        }
        if ((length == 2u && strncmp(name, "pi", 2u) == 0) ||
            (length == 1u && *name == 'e'))
        {
            node.kind = FVIZ_EXPR_CONSTANT;
            node.constant = length == 2u ? 3.14159265358979323846 : 2.71828182845904523536;
            return fviz_expression_add_node(parser, &node);
        }
        node.kind = FVIZ_EXPR_VARIABLE;
        result = fviz_expression_variable(parser, name, length);
        node.variable = result >= 0 ? (uint32_t)result : 0u;
        result = parser->failed == FVIZ_FALSE
            ? fviz_expression_add_node(parser, &node) : -1;
        fviz_expression_skip_space(parser);
        if (*parser->cursor == '.')
        {
            uint32_t component;
            ++parser->cursor;
            if (*parser->cursor == 'x') component = 0u;
            else if (*parser->cursor == 'y') component = 1u;
            else if (*parser->cursor == 'z') component = 2u;
            else if (*parser->cursor == 'w') component = 3u;
            else
            {
                fviz_expression_parser_fail(parser, "unknown vector component");
                return -1;
            }
            ++parser->cursor;
            (void)memset(&node, 0, sizeof(node));
            node.kind = FVIZ_EXPR_COMPONENT;
            node.child[0] = result;
            node.component = component;
            result = fviz_expression_add_node(parser, &node);
        }
        else if (*parser->cursor == '[')
        {
            char* end = NULL;
            unsigned long component;
            ++parser->cursor;
            fviz_expression_skip_space(parser);
            component = strtoul(parser->cursor, &end, 10);
            if (end == parser->cursor || component >= FVIZ_EXPRESSION_MAX_COMPONENTS)
            {
                fviz_expression_parser_fail(parser, "invalid component index");
                return -1;
            }
            parser->cursor = end;
            fviz_expression_skip_space(parser);
            if (*parser->cursor != ']')
            {
                fviz_expression_parser_fail(parser, "expected ']' after component index");
                return -1;
            }
            ++parser->cursor;
            (void)memset(&node, 0, sizeof(node));
            node.kind = FVIZ_EXPR_COMPONENT;
            node.child[0] = result;
            node.component = (uint32_t)component;
            result = fviz_expression_add_node(parser, &node);
        }
        return result;
    }
    fviz_expression_parser_fail(parser, "expected expression value");
    return -1;
}

static int fviz_expression_parse_unary(FVizExpressionParser* parser)
{
    FVizExpressionNode node;
    fviz_expression_skip_space(parser);
    if (*parser->cursor == '+')
    {
        ++parser->cursor;
        return fviz_expression_parse_unary(parser);
    }
    if (*parser->cursor == '-')
    {
        ++parser->cursor;
        (void)memset(&node, 0, sizeof(node));
        node.kind = FVIZ_EXPR_NEGATE;
        node.child[0] = fviz_expression_parse_unary(parser);
        return fviz_expression_add_node(parser, &node);
    }
    return fviz_expression_parse_primary(parser);
}

static int fviz_expression_parse_power(FVizExpressionParser* parser)
{
    FVizExpressionNode node;
    int left = fviz_expression_parse_unary(parser);
    fviz_expression_skip_space(parser);
    if (*parser->cursor != '^') return left;
    ++parser->cursor;
    (void)memset(&node, 0, sizeof(node));
    node.kind = FVIZ_EXPR_POWER;
    node.child[0] = left;
    node.child[1] = fviz_expression_parse_power(parser);
    return fviz_expression_add_node(parser, &node);
}

static int fviz_expression_parse_multiply(FVizExpressionParser* parser)
{
    int left = fviz_expression_parse_power(parser);
    for (;;)
    {
        FVizExpressionNode node;
        char operation;
        fviz_expression_skip_space(parser);
        operation = *parser->cursor;
        if (operation != '*' && operation != '/') break;
        ++parser->cursor;
        (void)memset(&node, 0, sizeof(node));
        node.kind = operation == '*' ? FVIZ_EXPR_MULTIPLY : FVIZ_EXPR_DIVIDE;
        node.child[0] = left;
        node.child[1] = fviz_expression_parse_power(parser);
        left = fviz_expression_add_node(parser, &node);
    }
    return left;
}

static int fviz_expression_parse_add(FVizExpressionParser* parser)
{
    int left = fviz_expression_parse_multiply(parser);
    for (;;)
    {
        FVizExpressionNode node;
        char operation;
        fviz_expression_skip_space(parser);
        operation = *parser->cursor;
        if (operation != '+' && operation != '-') break;
        ++parser->cursor;
        (void)memset(&node, 0, sizeof(node));
        node.kind = operation == '+' ? FVIZ_EXPR_ADD : FVIZ_EXPR_SUBTRACT;
        node.child[0] = left;
        node.child[1] = fviz_expression_parse_multiply(parser);
        left = fviz_expression_add_node(parser, &node);
    }
    return left;
}

static int fviz_expression_parse_compare(FVizExpressionParser* parser)
{
    int left = fviz_expression_parse_add(parser);
    for (;;)
    {
        FVizExpressionNode node;
        FVizExpressionNodeKind kind;
        fviz_expression_skip_space(parser);
        if (strncmp(parser->cursor, "==", 2u) == 0) kind = FVIZ_EXPR_EQUAL;
        else if (strncmp(parser->cursor, "!=", 2u) == 0) kind = FVIZ_EXPR_NOT_EQUAL;
        else if (strncmp(parser->cursor, "<=", 2u) == 0) kind = FVIZ_EXPR_LESS_EQUAL;
        else if (strncmp(parser->cursor, ">=", 2u) == 0) kind = FVIZ_EXPR_GREATER_EQUAL;
        else if (*parser->cursor == '<') kind = FVIZ_EXPR_LESS;
        else if (*parser->cursor == '>') kind = FVIZ_EXPR_GREATER;
        else break;
        parser->cursor += (kind == FVIZ_EXPR_LESS || kind == FVIZ_EXPR_GREATER) ? 1 : 2;
        (void)memset(&node, 0, sizeof(node));
        node.kind = kind;
        node.child[0] = left;
        node.child[1] = fviz_expression_parse_add(parser);
        left = fviz_expression_add_node(parser, &node);
    }
    return left;
}

static int fviz_expression_parse_conditional(FVizExpressionParser* parser)
{
    int condition = fviz_expression_parse_compare(parser);
    FVizExpressionNode node;
    fviz_expression_skip_space(parser);
    if (*parser->cursor != '?') return condition;
    ++parser->cursor;
    (void)memset(&node, 0, sizeof(node));
    node.kind = FVIZ_EXPR_CONDITIONAL;
    node.child[0] = condition;
    node.child[1] = fviz_expression_parse_conditional(parser);
    fviz_expression_skip_space(parser);
    if (*parser->cursor != ':')
    {
        fviz_expression_parser_fail(parser, "expected ':' in conditional expression");
        return -1;
    }
    ++parser->cursor;
    node.child[2] = fviz_expression_parse_conditional(parser);
    node.child_count = 3u;
    return fviz_expression_add_node(parser, &node);
}

void fviz_expression_options_initialize(FVizExpressionOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->parallel_threshold = 16384u;
    options->non_finite_policy = FVIZ_EXPRESSION_NON_FINITE_PROPAGATE;
}

void fviz_expression_binding_initialize(FVizExpressionBinding* binding)
{
    if (binding == NULL) return;
    (void)memset(binding, 0, sizeof(*binding));
    binding->struct_size = (uint32_t)sizeof(*binding);
    binding->association = FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED;
}

FVizResult fviz_expression_compile_with_diagnostic(
    const char* source, FVizExpression** out_expression, FVizSize* out_error_offset)
{
    FVizExpression* expression;
    FVizExpressionParser parser;
    FVizSize length;
    if (out_error_offset != NULL) *out_error_offset = 0u;
    if (out_expression == NULL || source == NULL || *source == '\0')
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_expression = NULL;
    length = (FVizSize)strlen(source);
    if (length > 65536u) return FVIZ_ERROR_INVALID_ARGUMENT;
    expression = (FVizExpression*)fviz_internal_object_allocate(
        sizeof(*expression), &g_fviz_expression_class, NULL);
    if (expression == NULL) return fviz_last_error_code();
    expression->source = (char*)fviz_alloc(length + 1u);
    if (expression->source == NULL)
    {
        fviz_release(expression);
        return fviz_last_error_code();
    }
    (void)memcpy(expression->source, source, length + 1u);
    (void)memset(&parser, 0, sizeof(parser));
    parser.expression = expression;
    parser.source = expression->source;
    parser.cursor = expression->source;
    expression->root = fviz_expression_parse_conditional(&parser);
    fviz_expression_skip_space(&parser);
    if (parser.failed == FVIZ_FALSE && *parser.cursor != '\0')
        fviz_expression_parser_fail(&parser, "unexpected token after expression");
    if (parser.failed != FVIZ_FALSE || expression->root < 0)
    {
        if (out_error_offset != NULL) *out_error_offset = expression->error_offset;
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_ARGUMENT,
            parser.message != NULL ? parser.message : "invalid expression");
        fviz_release(expression);
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_expression = expression;
    return FVIZ_OK;
}

FVizResult fviz_expression_compile(
    const char* source, FVizExpression** out_expression)
{
    return fviz_expression_compile_with_diagnostic(source, out_expression, NULL);
}

const char* fviz_expression_source(const FVizExpression* expression)
{
    return expression != NULL ? expression->source : NULL;
}

FVizSize fviz_expression_error_offset(const FVizExpression* expression)
{
    return expression != NULL ? expression->error_offset : 0u;
}

FVizSize fviz_expression_variable_count(const FVizExpression* expression)
{
    return expression != NULL ? expression->variable_count : 0u;
}

const char* fviz_expression_variable_name(
    const FVizExpression* expression, FVizSize index)
{
    return expression != NULL && index < expression->variable_count
        ? expression->variable_names[index] : NULL;
}

static double fviz_expression_read(
    const FVizExpressionResolvedBinding* binding,
    FVizSize tuple,
    uint32_t component)
{
    const unsigned char* pointer = binding->data + tuple * binding->stride +
        (FVizSize)component * fviz_data_type_size(binding->type);
    switch (binding->type)
    {
#define FVIZ_EXPR_READ(type_value, c_type) \
        case type_value: { c_type value; (void)memcpy(&value, pointer, sizeof(value)); return (double)value; }
        FVIZ_EXPR_READ(FVIZ_DATA_INT8, int8_t)
        FVIZ_EXPR_READ(FVIZ_DATA_UINT8, uint8_t)
        FVIZ_EXPR_READ(FVIZ_DATA_INT16, int16_t)
        FVIZ_EXPR_READ(FVIZ_DATA_UINT16, uint16_t)
        FVIZ_EXPR_READ(FVIZ_DATA_INT32, int32_t)
        FVIZ_EXPR_READ(FVIZ_DATA_UINT32, uint32_t)
        FVIZ_EXPR_READ(FVIZ_DATA_INT64, int64_t)
        FVIZ_EXPR_READ(FVIZ_DATA_UINT64, uint64_t)
        FVIZ_EXPR_READ(FVIZ_DATA_FLOAT32, float)
        FVIZ_EXPR_READ(FVIZ_DATA_FLOAT64, double)
#undef FVIZ_EXPR_READ
        default: return 0.0;
    }
}

static FVizResult fviz_expression_broadcast_components(
    uint32_t left, uint32_t right, uint32_t* out_components)
{
    if (left == right) *out_components = left;
    else if (left == 1u) *out_components = right;
    else if (right == 1u) *out_components = left;
    else return FVIZ_ERROR_INVALID_ARGUMENT;
    return FVIZ_OK;
}

static FVizResult fviz_expression_node_shape(
    const FVizExpression* expression,
    const FVizExpressionResolvedBinding* bindings,
    int node_index,
    uint32_t* out_components)
{
    const FVizExpressionNode* node = &expression->nodes[node_index];
    uint32_t a = 1u;
    uint32_t b = 1u;
    uint32_t c = 1u;
    uint32_t d = 1u;
    uint32_t index;
    if (node->kind == FVIZ_EXPR_CONSTANT) *out_components = 1u;
    else if (node->kind == FVIZ_EXPR_VARIABLE)
        *out_components = bindings[node->variable].components;
    else if (node->kind == FVIZ_EXPR_COMPONENT)
    {
        if (fviz_expression_node_shape(expression, bindings, node->child[0], &a) != FVIZ_OK ||
            node->component >= a)
            return FVIZ_ERROR_INVALID_ARGUMENT;
        *out_components = 1u;
    }
    else if (node->kind == FVIZ_EXPR_NEGATE)
        return fviz_expression_node_shape(expression, bindings, node->child[0], out_components);
    else if (node->kind >= FVIZ_EXPR_ADD && node->kind <= FVIZ_EXPR_GREATER_EQUAL)
    {
        if (fviz_expression_node_shape(expression, bindings, node->child[0], &a) != FVIZ_OK ||
            fviz_expression_node_shape(expression, bindings, node->child[1], &b) != FVIZ_OK)
            return FVIZ_ERROR_INVALID_ARGUMENT;
        return fviz_expression_broadcast_components(a, b, out_components);
    }
    else if (node->kind == FVIZ_EXPR_CONDITIONAL)
    {
        if (fviz_expression_node_shape(expression, bindings, node->child[0], &a) != FVIZ_OK ||
            fviz_expression_node_shape(expression, bindings, node->child[1], &b) != FVIZ_OK ||
            fviz_expression_node_shape(expression, bindings, node->child[2], &c) != FVIZ_OK ||
            fviz_expression_broadcast_components(b, c, out_components) != FVIZ_OK ||
            (a != 1u && a != *out_components))
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        for (index = 0u; index < node->child_count; ++index)
        {
            uint32_t shape;
            if (fviz_expression_node_shape(
                    expression, bindings, node->child[index], &shape) != FVIZ_OK)
                return FVIZ_ERROR_INVALID_ARGUMENT;
            if (index == 0u) a = shape;
            else if (index == 1u) b = shape;
            else if (index == 2u) c = shape;
            else if (index == 3u) d = shape;
        }
        switch (node->function)
        {
            case FVIZ_EXPR_MAG: *out_components = 1u; break;
            case FVIZ_EXPR_CROSS:
                if (a != 3u || b != 3u) return FVIZ_ERROR_INVALID_ARGUMENT;
                *out_components = 3u; break;
            case FVIZ_EXPR_VEC2: case FVIZ_EXPR_VEC3: case FVIZ_EXPR_VEC4:
                if (a != 1u || b != 1u ||
                    (node->child_count > 2u && c != 1u) ||
                    (node->child_count > 3u && d != 1u))
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                *out_components = node->child_count; break;
            case FVIZ_EXPR_DOT:
                if (a != b) return FVIZ_ERROR_INVALID_ARGUMENT;
                *out_components = 1u; break;
            case FVIZ_EXPR_CLAMP:
                if (fviz_expression_broadcast_components(a, b, out_components) != FVIZ_OK ||
                    fviz_expression_broadcast_components(*out_components, c, out_components) != FVIZ_OK)
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                break;
            case FVIZ_EXPR_WHERE:
                if (fviz_expression_broadcast_components(b, c, out_components) != FVIZ_OK ||
                    (a != 1u && a != *out_components))
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                break;
            case FVIZ_EXPR_MIN: case FVIZ_EXPR_MAX: case FVIZ_EXPR_POW:
                return fviz_expression_broadcast_components(a, b, out_components);
            default: *out_components = a; break;
        }
    }
    return *out_components <= FVIZ_EXPRESSION_MAX_COMPONENTS
        ? FVIZ_OK : FVIZ_ERROR_NOT_SUPPORTED;
}

static double fviz_expression_unary(
    FVizExpressionFunction function, double value)
{
    switch (function)
    {
        case FVIZ_EXPR_ABS: return fabs(value);
        case FVIZ_EXPR_SQRT: return sqrt(value);
        case FVIZ_EXPR_EXP: return exp(value);
        case FVIZ_EXPR_LOG: return log(value);
        case FVIZ_EXPR_LOG10: return log10(value);
        case FVIZ_EXPR_SIN: return sin(value);
        case FVIZ_EXPR_COS: return cos(value);
        case FVIZ_EXPR_TAN: return tan(value);
        case FVIZ_EXPR_FLOOR: return floor(value);
        case FVIZ_EXPR_CEIL: return ceil(value);
        case FVIZ_EXPR_ROUND: return round(value);
        default: return value;
    }
}

static FVizExpressionValue fviz_expression_eval_node(
    const FVizExpressionEvaluation* evaluation,
    int node_index,
    FVizSize tuple)
{
    const FVizExpressionNode* node = &evaluation->expression->nodes[node_index];
    FVizExpressionValue result;
    FVizExpressionValue a;
    FVizExpressionValue b;
    FVizExpressionValue c;
    FVizExpressionValue d;
    uint32_t index;
    (void)memset(&result, 0, sizeof(result));
    if (node->kind == FVIZ_EXPR_CONSTANT)
    {
        result.components = 1u;
        result.value[0] = node->constant;
        return result;
    }
    if (node->kind == FVIZ_EXPR_VARIABLE)
    {
        const FVizExpressionResolvedBinding* binding =
            &evaluation->bindings[node->variable];
        result.components = binding->components;
        for (index = 0u; index < result.components; ++index)
            result.value[index] = fviz_expression_read(binding, tuple, index);
        return result;
    }
    a = fviz_expression_eval_node(evaluation, node->child[0], tuple);
    if (node->kind == FVIZ_EXPR_COMPONENT)
    {
        result.components = 1u;
        result.value[0] = a.value[node->component];
        return result;
    }
    if (node->kind == FVIZ_EXPR_NEGATE)
    {
        for (index = 0u; index < a.components; ++index) a.value[index] = -a.value[index];
        return a;
    }
    if (node->kind >= FVIZ_EXPR_ADD && node->kind <= FVIZ_EXPR_GREATER_EQUAL)
    {
        b = fviz_expression_eval_node(evaluation, node->child[1], tuple);
        (void)fviz_expression_broadcast_components(a.components, b.components, &result.components);
        for (index = 0u; index < result.components; ++index)
        {
            const double av = a.value[a.components == 1u ? 0u : index];
            const double bv = b.value[b.components == 1u ? 0u : index];
            if (node->kind == FVIZ_EXPR_ADD) result.value[index] = av + bv;
            else if (node->kind == FVIZ_EXPR_SUBTRACT) result.value[index] = av - bv;
            else if (node->kind == FVIZ_EXPR_MULTIPLY) result.value[index] = av * bv;
            else if (node->kind == FVIZ_EXPR_DIVIDE) result.value[index] = av / bv;
            else if (node->kind == FVIZ_EXPR_POWER) result.value[index] = pow(av, bv);
            else if (node->kind == FVIZ_EXPR_EQUAL) result.value[index] = av == bv ? 1.0 : 0.0;
            else if (node->kind == FVIZ_EXPR_NOT_EQUAL) result.value[index] = av != bv ? 1.0 : 0.0;
            else if (node->kind == FVIZ_EXPR_LESS) result.value[index] = av < bv ? 1.0 : 0.0;
            else if (node->kind == FVIZ_EXPR_LESS_EQUAL) result.value[index] = av <= bv ? 1.0 : 0.0;
            else if (node->kind == FVIZ_EXPR_GREATER) result.value[index] = av > bv ? 1.0 : 0.0;
            else result.value[index] = av >= bv ? 1.0 : 0.0;
        }
        return result;
    }
    if (node->kind == FVIZ_EXPR_CONDITIONAL)
    {
        b = fviz_expression_eval_node(evaluation, node->child[1], tuple);
        c = fviz_expression_eval_node(evaluation, node->child[2], tuple);
        (void)fviz_expression_broadcast_components(b.components, c.components, &result.components);
        for (index = 0u; index < result.components; ++index)
        {
            const double condition = a.value[a.components == 1u ? 0u : index];
            result.value[index] = condition != 0.0
                ? b.value[b.components == 1u ? 0u : index]
                : c.value[c.components == 1u ? 0u : index];
        }
        return result;
    }
    if (node->child_count > 1u)
        b = fviz_expression_eval_node(evaluation, node->child[1], tuple);
    else (void)memset(&b, 0, sizeof(b));
    if (node->child_count > 2u)
        c = fviz_expression_eval_node(evaluation, node->child[2], tuple);
    else (void)memset(&c, 0, sizeof(c));
    if (node->child_count > 3u)
        d = fviz_expression_eval_node(evaluation, node->child[3], tuple);
    else (void)memset(&d, 0, sizeof(d));
    if (node->function == FVIZ_EXPR_MAG || node->function == FVIZ_EXPR_NORMALIZE)
    {
        double magnitude = 0.0;
        for (index = 0u; index < a.components; ++index)
            magnitude += a.value[index] * a.value[index];
        magnitude = sqrt(magnitude);
        if (node->function == FVIZ_EXPR_MAG)
        {
            result.components = 1u;
            result.value[0] = magnitude;
        }
        else
        {
            result.components = a.components;
            for (index = 0u; index < a.components; ++index)
                result.value[index] = a.value[index] / magnitude;
        }
    }
    else if (node->function == FVIZ_EXPR_DOT)
    {
        result.components = 1u;
        for (index = 0u; index < a.components; ++index)
            result.value[0] += a.value[index] * b.value[index];
    }
    else if (node->function == FVIZ_EXPR_CROSS)
    {
        result.components = 3u;
        result.value[0] = a.value[1] * b.value[2] - a.value[2] * b.value[1];
        result.value[1] = a.value[2] * b.value[0] - a.value[0] * b.value[2];
        result.value[2] = a.value[0] * b.value[1] - a.value[1] * b.value[0];
    }
    else if (node->function >= FVIZ_EXPR_VEC2 && node->function <= FVIZ_EXPR_VEC4)
    {
        result.components = node->child_count;
        result.value[0] = a.value[0]; result.value[1] = b.value[0];
        if (result.components > 2u) result.value[2] = c.value[0];
        if (result.components > 3u) result.value[3] = d.value[0];
    }
    else if (node->function == FVIZ_EXPR_CLAMP || node->function == FVIZ_EXPR_WHERE)
    {
        if (node->function == FVIZ_EXPR_CLAMP)
        {
            (void)fviz_expression_broadcast_components(a.components, b.components, &result.components);
            (void)fviz_expression_broadcast_components(result.components, c.components, &result.components);
            for (index = 0u; index < result.components; ++index)
            {
                const double value = a.value[a.components == 1u ? 0u : index];
                const double minimum = b.value[b.components == 1u ? 0u : index];
                const double maximum = c.value[c.components == 1u ? 0u : index];
                result.value[index] = value < minimum ? minimum : (value > maximum ? maximum : value);
            }
        }
        else
        {
            (void)fviz_expression_broadcast_components(b.components, c.components, &result.components);
            for (index = 0u; index < result.components; ++index)
            {
                const double condition = a.value[a.components == 1u ? 0u : index];
                result.value[index] = condition != 0.0
                    ? b.value[b.components == 1u ? 0u : index]
                    : c.value[c.components == 1u ? 0u : index];
            }
        }
    }
    else if (node->function == FVIZ_EXPR_MIN || node->function == FVIZ_EXPR_MAX ||
             node->function == FVIZ_EXPR_POW)
    {
        (void)fviz_expression_broadcast_components(a.components, b.components, &result.components);
        for (index = 0u; index < result.components; ++index)
        {
            const double av = a.value[a.components == 1u ? 0u : index];
            const double bv = b.value[b.components == 1u ? 0u : index];
            result.value[index] = node->function == FVIZ_EXPR_MIN ? (av < bv ? av : bv) :
                (node->function == FVIZ_EXPR_MAX ? (av > bv ? av : bv) : pow(av, bv));
        }
    }
    else
    {
        result.components = a.components;
        for (index = 0u; index < a.components; ++index)
            result.value[index] = fviz_expression_unary(node->function, a.value[index]);
    }
    return result;
}

static void fviz_expression_evaluate_range(
    FVizSize begin, FVizSize end, void* user_data)
{
    FVizExpressionEvaluation* evaluation = (FVizExpressionEvaluation*)user_data;
    FVizSize tuple;
    for (tuple = begin; tuple < end; ++tuple)
    {
        FVizExpressionValue value = fviz_expression_eval_node(
            evaluation, evaluation->expression->root, tuple);
        uint32_t component;
        for (component = 0u; component < value.components; ++component)
        {
            double item = value.value[component];
            if (isfinite(item) == 0)
            {
                if (evaluation->options.non_finite_policy == FVIZ_EXPRESSION_NON_FINITE_REPLACE)
                    item = evaluation->options.non_finite_replacement;
                else if (evaluation->options.non_finite_policy == FVIZ_EXPRESSION_NON_FINITE_ERROR)
                    evaluation->non_finite_found = FVIZ_TRUE;
            }
            evaluation->output[tuple * evaluation->output_components + component] = item;
        }
    }
}

FVizResult fviz_expression_evaluate(
    const FVizExpression* expression,
    const FVizExpressionBinding* bindings,
    FVizSize binding_count,
    const FVizExpressionOptions* options,
    FVizDataArray** out_array)
{
    FVizExpressionOptions defaults;
    FVizExpressionResolvedBinding* resolved = NULL;
    FVizExpressionEvaluation evaluation;
    FVizDataArray* output = NULL;
    FVizSize tuple_count = 0u;
    FVizSize variable;
    FVizExpressionAssociation common_association =
        FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED;
    uint32_t output_components = 0u;
    FVizResult result;
    if (out_array == NULL || expression == NULL ||
        (expression->variable_count > 0u && bindings == NULL))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_array = NULL;
    fviz_expression_options_initialize(&defaults);
    if (options == NULL) options = &defaults;
    if ((options->struct_size != 0u && options->struct_size < sizeof(*options)) ||
        options->non_finite_policy < FVIZ_EXPRESSION_NON_FINITE_PROPAGATE ||
        options->non_finite_policy > FVIZ_EXPRESSION_NON_FINITE_ERROR ||
        !isfinite(options->non_finite_replacement))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (expression->variable_count > 0u)
    {
        resolved = (FVizExpressionResolvedBinding*)fviz_alloc(
            expression->variable_count * sizeof(*resolved));
        if (resolved == NULL) return fviz_last_error_code();
    }
    for (variable = 0u; variable < expression->variable_count; ++variable)
    {
        FVizSize binding;
        const FVizDataArray* array = NULL;
        for (binding = 0u; binding < binding_count; ++binding)
        {
            if (bindings[binding].name != NULL &&
                strcmp(bindings[binding].name, expression->variable_names[variable]) == 0)
            {
                const FVizExpressionAssociation association =
                    bindings[binding].struct_size == 0u
                    ? FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED
                    : bindings[binding].association;
                array = bindings[binding].array;
                if (bindings[binding].struct_size != 0u &&
                    bindings[binding].struct_size < sizeof(FVizExpressionBinding))
                {
                    fviz_free(resolved);
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                }
                if (association !=
                    FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED)
                {
                    if (association <
                            FVIZ_EXPRESSION_ASSOCIATION_POINTS ||
                        association >
                            FVIZ_EXPRESSION_ASSOCIATION_FIELD ||
                        (common_association !=
                            FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED &&
                         common_association != association))
                    {
                        fviz_free(resolved);
                        fviz_internal_set_error(
                            FVIZ_ERROR_INVALID_ARGUMENT,
                            "expression bindings have incompatible associations");
                        return FVIZ_ERROR_INVALID_ARGUMENT;
                    }
                    common_association = association;
                }
                break;
            }
        }
        if (array == NULL || fviz_data_array_components(array) == 0u ||
            fviz_data_array_components(array) > FVIZ_EXPRESSION_MAX_COMPONENTS)
        {
            fviz_free(resolved);
            fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "expression binding is missing or unsupported");
            return FVIZ_ERROR_NOT_FOUND;
        }
        if (variable == 0u) tuple_count = fviz_data_array_tuple_count(array);
        else if (fviz_data_array_tuple_count(array) != tuple_count)
        {
            fviz_free(resolved);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "expression bindings have different tuple counts");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        resolved[variable].data = (const unsigned char*)fviz_data_array_const_data(array);
        resolved[variable].stride = fviz_data_array_tuple_stride(array);
        resolved[variable].type = fviz_data_array_type(array);
        resolved[variable].components = fviz_data_array_components(array);
    }
    /* Constant expressions produce one tuple so they remain directly useful. */
    if (expression->variable_count == 0u) tuple_count = 1u;
    result = fviz_expression_node_shape(
        expression, resolved, expression->root, &output_components);
    if (result != FVIZ_OK)
    {
        fviz_free(resolved);
        fviz_internal_set_error(result, "expression operand component counts are incompatible");
        return result;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, output_components, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, tuple_count) != FVIZ_OK)
    {
        fviz_free(resolved);
        fviz_release(output);
        return fviz_last_error_code();
    }
    (void)memset(&evaluation, 0, sizeof(evaluation));
    evaluation.expression = expression;
    evaluation.bindings = resolved;
    evaluation.output = (double*)fviz_data_array_data(output);
    evaluation.output_components = output_components;
    evaluation.options = *options;
    if (tuple_count >= options->parallel_threshold && options->parallel_threshold != 0u &&
        options->non_finite_policy != FVIZ_EXPRESSION_NON_FINITE_ERROR)
        result = fviz_parallel_for(
            0u, tuple_count, 4096u, fviz_expression_evaluate_range, &evaluation);
    else
    {
        fviz_expression_evaluate_range(0u, tuple_count, &evaluation);
        result = FVIZ_OK;
    }
    fviz_free(resolved);
    if (result != FVIZ_OK || evaluation.non_finite_found != FVIZ_FALSE)
    {
        fviz_release(output);
        if (result == FVIZ_OK)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "expression produced a non-finite result");
            return FVIZ_ERROR_INVALID_STATE;
        }
        return result;
    }
    *out_array = output;
    return FVIZ_OK;
}

void fviz_expression_cache_clear(FVizExpressionCache* cache)
{
    FVizSize index;
    if (cache == NULL || cache->entries == NULL) return;
    for (index = 0u; index < fviz_array_count(cache->entries); ++index)
    {
        FVizExpressionCacheEntry* entry = (FVizExpressionCacheEntry*)
            fviz_array_at(cache->entries, index);
        fviz_free(entry->source);
        fviz_release(entry->expression);
    }
    fviz_array_clear(cache->entries);
    fviz_object_modified((FVizObject*)cache);
}

static void fviz_expression_cache_destroy(FVizObject* object)
{
    FVizExpressionCache* cache = (FVizExpressionCache*)object;
    fviz_expression_cache_clear(cache);
    fviz_release(cache->entries);
}

FVizResult fviz_expression_cache_create(
    FVizSize capacity, FVizExpressionCache** out_cache)
{
    FVizExpressionCache* cache;
    if (out_cache == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_cache = NULL;
    cache = (FVizExpressionCache*)fviz_internal_object_allocate(
        sizeof(*cache), &g_fviz_expression_cache_class, NULL);
    if (cache == NULL) return fviz_last_error_code();
    cache->capacity = capacity;
    if (fviz_array_create(sizeof(FVizExpressionCacheEntry), &cache->entries) != FVIZ_OK)
    {
        fviz_release(cache);
        return fviz_last_error_code();
    }
    *out_cache = cache;
    return FVIZ_OK;
}

FVizResult fviz_expression_cache_get(
    FVizExpressionCache* cache,
    const char* source,
    FVizExpression** out_expression)
{
    FVizSize index;
    FVizExpression* compiled = NULL;
    FVizExpressionCacheEntry entry;
    if (cache == NULL || source == NULL || out_expression == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_expression = NULL;
    for (index = 0u; index < fviz_array_count(cache->entries); ++index)
    {
        FVizExpressionCacheEntry* current = (FVizExpressionCacheEntry*)
            fviz_array_at(cache->entries, index);
        if (strcmp(current->source, source) == 0)
        {
            current->last_use = ++cache->use_serial;
            *out_expression = (FVizExpression*)fviz_retain(current->expression);
            return *out_expression != NULL ? FVIZ_OK : fviz_last_error_code();
        }
    }
    if (fviz_expression_compile(source, &compiled) != FVIZ_OK) return fviz_last_error_code();
    if (cache->capacity == 0u)
    {
        *out_expression = compiled;
        return FVIZ_OK;
    }
    if (fviz_array_count(cache->entries) >= cache->capacity)
    {
        FVizSize oldest = 0u;
        uint64_t oldest_use = UINT64_MAX;
        FVizExpressionCacheEntry* entries = (FVizExpressionCacheEntry*)
            fviz_array_data(cache->entries);
        const FVizSize count = fviz_array_count(cache->entries);
        for (index = 0u; index < count; ++index)
            if (entries[index].last_use < oldest_use)
            { oldest = index; oldest_use = entries[index].last_use; }
        fviz_free(entries[oldest].source);
        fviz_release(entries[oldest].expression);
        if (oldest + 1u < count)
            (void)memmove(&entries[oldest], &entries[oldest + 1u],
                (size_t)(count - oldest - 1u) * sizeof(*entries));
        (void)fviz_array_resize(cache->entries, count - 1u);
    }
    entry.source = (char*)fviz_alloc((FVizSize)strlen(source) + 1u);
    entry.expression = (FVizExpression*)fviz_retain(compiled);
    entry.last_use = ++cache->use_serial;
    if (entry.source == NULL || entry.expression == NULL)
    {
        fviz_free(entry.source);
        fviz_release(entry.expression);
        fviz_release(compiled);
        return fviz_last_error_code();
    }
    (void)strcpy(entry.source, source);
    if (fviz_array_push(cache->entries, &entry) != FVIZ_OK)
    {
        fviz_free(entry.source);
        fviz_release(entry.expression);
        fviz_release(compiled);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)cache);
    *out_expression = compiled;
    return FVIZ_OK;
}

FVizSize fviz_expression_cache_count(const FVizExpressionCache* cache)
{
    return cache != NULL ? fviz_array_count(cache->entries) : 0u;
}

FVizSize fviz_expression_cache_capacity(const FVizExpressionCache* cache)
{
    return cache != NULL ? cache->capacity : 0u;
}
