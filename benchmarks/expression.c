#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <FViz/FViz.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

int main(void)
{
    const FVizSize tuple_count = 1000000u;
    FVizDataArray* scalar = NULL;
    FVizDataArray* vector = NULL;
    FVizDataArray* output = NULL;
    FVizExpression* expression = NULL;
    FVizExpressionBinding bindings[2];
    float* scalar_data = NULL;
    float* vector_data = NULL;
    FVizSize tuple;
    double compile_started;
    double compile_seconds;
    double evaluate_started;
    double evaluate_seconds;
    double checksum = 0.0;
    scalar_data = (float*)malloc((size_t)tuple_count * sizeof(*scalar_data));
    vector_data = (float*)malloc((size_t)tuple_count * 3u * sizeof(*vector_data));
    if (scalar_data == NULL || vector_data == NULL) return 1;
    for (tuple = 0u; tuple < tuple_count; ++tuple)
    {
        scalar_data[tuple] = (float)(tuple % 1000u) * 0.001f;
        vector_data[tuple * 3u + 0u] = scalar_data[tuple] + 1.0f;
        vector_data[tuple * 3u + 1u] = scalar_data[tuple] + 2.0f;
        vector_data[tuple * 3u + 2u] = scalar_data[tuple] + 3.0f;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalar) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &vector) != FVIZ_OK ||
        fviz_data_array_append_tuples(scalar, scalar_data, tuple_count) != FVIZ_OK ||
        fviz_data_array_append_tuples(vector, vector_data, tuple_count) != FVIZ_OK)
        return 2;
    free(vector_data); free(scalar_data);
    fviz_expression_binding_initialize(&bindings[0]);
    fviz_expression_binding_initialize(&bindings[1]);
    bindings[0].name = "A"; bindings[0].array = scalar;
    bindings[1].name = "V"; bindings[1].array = vector;
    bindings[0].association = bindings[1].association =
        FVIZ_EXPRESSION_ASSOCIATION_POINTS;
    compile_started = wall_seconds();
    if (fviz_expression_compile(
            "sqrt(A*A + dot(V,V)) + clamp(V.x, 0, 2)", &expression) != FVIZ_OK)
        return 3;
    compile_seconds = wall_seconds() - compile_started;
    evaluate_started = wall_seconds();
    if (fviz_expression_evaluate(expression, bindings, 2u, NULL, &output) != FVIZ_OK)
        return 4;
    evaluate_seconds = wall_seconds() - evaluate_started;
    for (tuple = 0u; tuple < tuple_count; tuple += 997u)
    {
        double value;
        if (fviz_data_array_get_component(output, tuple, 0u, &value) != FVIZ_OK) return 5;
        checksum += value;
    }
    puts("tuples,compile_seconds,evaluate_seconds,ns_per_tuple,checksum");
    printf("%llu,%.9f,%.9f,%.3f,%.9f\n",
        (unsigned long long)tuple_count,
        compile_seconds,
        evaluate_seconds,
        evaluate_seconds * 1.0e9 / (double)tuple_count,
        checksum);
    fviz_release(output); fviz_release(expression);
    fviz_release(vector); fviz_release(scalar);
    return checksum > 0.0 ? 0 : 6;
}
