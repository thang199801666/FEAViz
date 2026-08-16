#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "CHECK failed at %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

static int value_at(
    const FVizDataArray* array,
    FVizSize tuple,
    uint32_t component,
    double expected)
{
    double value = 0.0;
    return fviz_data_array_get_component(array, tuple, component, &value) == FVIZ_OK &&
        fabs(value - expected) < 1.0e-10;
}

int main(void)
{
    FVizDataArray* scalar = NULL;
    FVizDataArray* vector = NULL;
    FVizDataArray* short_array = NULL;
    FVizDataArray* output = NULL;
    FVizExpression* expression = NULL;
    FVizExpressionBinding bindings[2];
    FVizExpressionOptions options;
    const float scalar_values[3] = {1.0f, 2.0f, 3.0f};
    const double vector_values[9] = {
        3.0, 4.0, 0.0,
        0.0, 0.0, 5.0,
        1.0, 2.0, 2.0
    };
    const double short_values[2] = {1.0, 2.0};
    FVizSize error_offset = 0u;
    FVizExpressionCache* cache = NULL;
    FVizExpression* cached = NULL;

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalar) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(scalar, scalar_values, 3u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &vector) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(vector, vector_values, 3u) == FVIZ_OK);
    fviz_expression_binding_initialize(&bindings[0]);
    fviz_expression_binding_initialize(&bindings[1]);
    bindings[0].name = "A"; bindings[0].array = scalar;
    bindings[1].name = "V"; bindings[1].array = vector;
    bindings[0].association = FVIZ_EXPRESSION_ASSOCIATION_POINTS;
    bindings[1].association = FVIZ_EXPRESSION_ASSOCIATION_POINTS;

    CHECK(fviz_expression_compile("sqrt(A^2) + mag(V)", &expression) == FVIZ_OK);
    CHECK(strcmp(fviz_expression_source(expression), "sqrt(A^2) + mag(V)") == 0);
    CHECK(fviz_expression_variable_count(expression) == 2u);
    CHECK(strcmp(fviz_expression_variable_name(expression, 0u), "A") == 0);
    CHECK(strcmp(fviz_expression_variable_name(expression, 1u), "V") == 0);
    fviz_expression_options_initialize(&options);
    options.parallel_threshold = 1u;
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, &options, &output) == FVIZ_OK);
    CHECK(fviz_data_array_type(output) == FVIZ_DATA_FLOAT64);
    CHECK(fviz_data_array_components(output) == 1u);
    CHECK(fviz_data_array_tuple_count(output) == 3u);
    CHECK(value_at(output, 0u, 0u, 6.0));
    CHECK(value_at(output, 1u, 0u, 7.0));
    CHECK(value_at(output, 2u, 0u, 6.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile(
        "A >= 2 ? V : vec3(-1,-1,-1)", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) == FVIZ_OK);
    CHECK(value_at(output, 0u, 0u, -1.0));
    CHECK(value_at(output, 1u, 2u, 5.0));
    CHECK(value_at(output, 2u, 1u, 2.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile(
        "normalize(V) * A + vec3(1, 2, 3)", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) == FVIZ_OK);
    CHECK(fviz_data_array_components(output) == 3u);
    CHECK(value_at(output, 0u, 0u, 1.6));
    CHECK(value_at(output, 0u, 1u, 2.8));
    CHECK(value_at(output, 0u, 2u, 3.0));
    CHECK(value_at(output, 1u, 2u, 5.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile(
        "dot(V, vec3(1, 0, 0)) + V.y",
        &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) == FVIZ_OK);
    CHECK(value_at(output, 0u, 0u, 7.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile("cross(V, vec3(0,0,1))", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) == FVIZ_OK);
    CHECK(value_at(output, 0u, 0u, 4.0));
    CHECK(value_at(output, 0u, 1u, -3.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile("where(A, V, vec3(9,9,9))", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) == FVIZ_OK);
    CHECK(value_at(output, 2u, 1u, 2.0));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile("1 / (A - A)", &expression) == FVIZ_OK);
    options.non_finite_policy = FVIZ_EXPRESSION_NON_FINITE_REPLACE;
    options.non_finite_replacement = -7.0;
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, &options, &output) == FVIZ_OK);
    CHECK(value_at(output, 1u, 0u, -7.0));
    fviz_release(output); output = NULL;
    options.non_finite_policy = FVIZ_EXPRESSION_NON_FINITE_ERROR;
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, &options, &output) ==
        FVIZ_ERROR_INVALID_STATE);
    CHECK(output == NULL);
    fviz_release(expression); expression = NULL;

    CHECK(fviz_expression_compile("2*pi", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, NULL, 0u, NULL, &output) == FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(output) == 1u);
    CHECK(value_at(output, 0u, 0u, 2.0 * 3.14159265358979323846));
    fviz_release(output); output = NULL;
    fviz_release(expression); expression = NULL;

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &short_array) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(short_array, short_values, 2u) == FVIZ_OK);
    bindings[1].array = short_array;
    CHECK(fviz_expression_compile("A + V", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) ==
        FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(expression); expression = NULL;
    bindings[1].array = vector;

    bindings[1].association = FVIZ_EXPRESSION_ASSOCIATION_CELLS;
    CHECK(fviz_expression_compile("A + V.x", &expression) == FVIZ_OK);
    CHECK(fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) ==
        FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(expression); expression = NULL;
    bindings[1].association = FVIZ_EXPRESSION_ASSOCIATION_POINTS;

    CHECK(fviz_expression_compile("A +", &expression) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(expression == NULL);
    CHECK(fviz_expression_compile_with_diagnostic(
        "A > 1 ? 2", &expression, &error_offset) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(expression == NULL && error_offset > 0u);

    CHECK(fviz_expression_cache_create(2u, &cache) == FVIZ_OK);
    CHECK(fviz_expression_cache_get(cache, "A + 1", &expression) == FVIZ_OK);
    CHECK(fviz_expression_cache_get(cache, "A + 1", &cached) == FVIZ_OK);
    CHECK(expression == cached);
    CHECK(fviz_expression_cache_count(cache) == 1u);
    fviz_release(cached); cached = NULL;
    fviz_release(expression); expression = NULL;
    CHECK(fviz_expression_cache_get(cache, "A + 2", &expression) == FVIZ_OK);
    fviz_release(expression); expression = NULL;
    CHECK(fviz_expression_cache_get(cache, "A + 3", &expression) == FVIZ_OK);
    CHECK(fviz_expression_cache_count(cache) == 2u);
    fviz_release(expression); expression = NULL;
    CHECK(fviz_expression_compile("unknown(A)", &expression) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(expression == NULL);

    fviz_release(short_array);
    fviz_release(cached);
    fviz_release(cache);
    fviz_release(vector);
    fviz_release(scalar);
    return 0;
}
