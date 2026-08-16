#ifndef FVIZ_ALGORITHMS_EXPRESSION_H
#define FVIZ_ALGORITHMS_EXPRESSION_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizExpression FVizExpression;
typedef struct FVizExpressionCache FVizExpressionCache;
#define FVIZ_TYPE_EXPRESSION UINT64_C(0x29DFA6330BF49C17)
#define FVIZ_TYPE_EXPRESSION_CACHE UINT64_C(0xD5E186AF942B703C)
#define FVIZ_EXPRESSION_MAX_COMPONENTS 16u

typedef enum FVizExpressionNonFinitePolicy
{
    FVIZ_EXPRESSION_NON_FINITE_PROPAGATE = 0,
    FVIZ_EXPRESSION_NON_FINITE_REPLACE = 1,
    FVIZ_EXPRESSION_NON_FINITE_ERROR = 2
} FVizExpressionNonFinitePolicy;

typedef enum FVizExpressionAssociation
{
    FVIZ_EXPRESSION_ASSOCIATION_UNSPECIFIED = 0,
    FVIZ_EXPRESSION_ASSOCIATION_POINTS = 1,
    FVIZ_EXPRESSION_ASSOCIATION_CELLS = 2,
    FVIZ_EXPRESSION_ASSOCIATION_FIELD = 3
} FVizExpressionAssociation;

typedef struct FVizExpressionBinding
{
    uint32_t struct_size;
    const char* name;
    const FVizDataArray* array;
    FVizExpressionAssociation association;
} FVizExpressionBinding;

typedef struct FVizExpressionOptions
{
    uint32_t struct_size;
    FVizSize parallel_threshold;
    FVizExpressionNonFinitePolicy non_finite_policy;
    double non_finite_replacement;
} FVizExpressionOptions;

FVIZ_API void fviz_expression_options_initialize(FVizExpressionOptions* options);
FVIZ_API void fviz_expression_binding_initialize(FVizExpressionBinding* binding);
/* Compiles an immutable expression that can be evaluated repeatedly with new
 * bindings. Supported operators are +, -, *, /, ^, comparisons and ?: . Functions include
 * abs, sqrt, exp, log, log10, sin, cos, tan, floor, ceil, round, min, max,
 * pow, clamp, mag, dot, cross, normalize, vec2/vec3/vec4 and where.
 * Vector components can be selected with .x/.y/.z/.w or [constant-index]. */
FVIZ_API FVizResult fviz_expression_compile(const char* source, FVizExpression** out_expression);
FVIZ_API FVizResult fviz_expression_compile_with_diagnostic(const char* source, FVizExpression** out_expression,
                                                            FVizSize* out_error_offset);
FVIZ_API const char* fviz_expression_source(const FVizExpression* expression);
FVIZ_API FVizSize fviz_expression_error_offset(const FVizExpression* expression);
FVIZ_API FVizSize fviz_expression_variable_count(const FVizExpression* expression);
FVIZ_API const char* fviz_expression_variable_name(const FVizExpression* expression, FVizSize index);
/* All bound arrays must have the same tuple count and all specified
 * associations must match. Scalar values broadcast over vectors; otherwise
 * vector operands must have equal component counts.
 * The result is a Float64 array. */
FVIZ_API FVizResult fviz_expression_evaluate(const FVizExpression* expression, const FVizExpressionBinding* bindings,
                                             FVizSize binding_count, const FVizExpressionOptions* options,
                                             FVizDataArray** out_array);

FVIZ_API FVizResult fviz_expression_cache_create(FVizSize capacity, FVizExpressionCache** out_cache);
/* Returns a retained immutable expression, compiling and caching on a miss. */
FVIZ_API FVizResult fviz_expression_cache_get(FVizExpressionCache* cache, const char* source,
                                              FVizExpression** out_expression);
FVIZ_API void fviz_expression_cache_clear(FVizExpressionCache* cache);
FVIZ_API FVizSize fviz_expression_cache_count(const FVizExpressionCache* cache);
FVIZ_API FVizSize fviz_expression_cache_capacity(const FVizExpressionCache* cache);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_EXPRESSION_H */
