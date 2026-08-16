#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/FViz.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FVizExpression* expression = NULL;
    FVizExpressionBinding* bindings = NULL;
    FVizDataArray** arrays = NULL;
    FVizDataArray* output = NULL;
    FVizExpressionOptions options;
    char* source;
    FVizSize variable_count;
    FVizSize variable;
    if (data == NULL || size == 0u || size > 65536u) return 0;
    source = (char*)malloc(size + 1u);
    if (source == NULL) return 0;
    (void)memcpy(source, data, size);
    source[size] = '\0';
    if (fviz_expression_compile(source, &expression) != FVIZ_OK)
    {
        free(source);
        return 0;
    }
    variable_count = fviz_expression_variable_count(expression);
    if (variable_count > 0u)
    {
        bindings = (FVizExpressionBinding*)calloc(
            (size_t)variable_count, sizeof(*bindings));
        arrays = (FVizDataArray**)calloc((size_t)variable_count, sizeof(*arrays));
        if (bindings == NULL || arrays == NULL) goto done;
    }
    for (variable = 0u; variable < variable_count; ++variable)
    {
        const uint32_t components = (uint32_t)(1u +
            ((size > 0u ? data[variable % size] : 0u) % 4u));
        double tuple[4][4];
        FVizSize i;
        uint32_t component;
        for (i = 0u; i < 4u; ++i)
            for (component = 0u; component < components; ++component)
                tuple[i][component] = (double)((int)(i * 7u + component * 3u) - 5);
        if (fviz_data_array_create(FVIZ_DATA_FLOAT64, components, &arrays[variable]) != FVIZ_OK ||
            fviz_data_array_append_tuples(arrays[variable], tuple, 4u) != FVIZ_OK)
            goto done;
        fviz_expression_binding_initialize(&bindings[variable]);
        bindings[variable].name = fviz_expression_variable_name(expression, variable);
        bindings[variable].array = arrays[variable];
        bindings[variable].association = FVIZ_EXPRESSION_ASSOCIATION_POINTS;
    }
    fviz_expression_options_initialize(&options);
    options.parallel_threshold = 0u;
    (void)fviz_expression_evaluate(
        expression, bindings, variable_count, &options, &output);

done:
    fviz_release(output);
    for (variable = 0u; variable < variable_count; ++variable)
        fviz_release(arrays != NULL ? arrays[variable] : NULL);
    free(arrays);
    free(bindings);
    fviz_release(expression);
    free(source);
    return 0;
}
