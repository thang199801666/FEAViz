#include <math.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizDataArray* vectors = NULL;
    FVizDataArray* component = NULL;
    FVizDataArray* magnitude = NULL;
    FVizDataArray* finite = NULL;
    FVizDataArray* source_ids = NULL;
    FVizDataArray* target_ids = NULL;
    FVizDataArray* gathered = NULL;
    FVizDataArray* gather_mask = NULL;
    FVizDataArray* indices = NULL;
    FVizDataArray* weights = NULL;
    FVizDataArray* averaged = NULL;
    FVizDataArray* average_mask = NULL;
    FVizDataArray* transformed = NULL;
    FVizDataArray* discontinuity = NULL;
    FVizFieldGatherOptions gather_options;
    FVizIndexedAverageOptions average_options;
    const double vector_values[3][2] = {{3.0, 4.0}, {NAN, 1.0}, {0.0, 5.0}};
    const uint64_t source_id_values[3] = {20u, 10u, 30u};
    const uint64_t target_id_values[3] = {10u, 40u, 20u};
    const double fill[2] = {-1.0, -2.0};
    const uint32_t index_values[3] = {0u, 0u, 1u};
    const double weight_values[3] = {1.0, 3.0, 2.0};
    double value;
    const double tuple_matrix[2][3] = {{1.0, 0.0, 0.0}, {0.0, 0.5, 0.5}};
    const double basis[3][2] = {{1.0, 0.0}, {1.0, 1.0}, {1.0, 2.0}};
    double least_squares[2][3];

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 2u, &vectors) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(vectors, vector_values, 3u) == FVIZ_OK);
    CHECK(fviz_field_extract_component(vectors, 1u, &component) == FVIZ_OK);
    CHECK(fviz_field_compute_magnitude(vectors, &magnitude) == FVIZ_OK);
    CHECK(fviz_field_compute_finite_mask(vectors, &finite) == FVIZ_OK);
    CHECK(fviz_data_array_get_component(component, 2u, 0u, &value) == FVIZ_OK && value == 5.0);
    CHECK(fviz_data_array_get_component(magnitude, 0u, 0u, &value) == FVIZ_OK && value == 5.0);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(finite, 0u) == 1u);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(finite, 1u) == 0u);

    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &source_ids) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(source_ids, source_id_values, 3u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &target_ids) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(target_ids, target_id_values, 3u) == FVIZ_OK);
    fviz_field_gather_options_initialize(&gather_options);
    gather_options.missing_id_policy = FVIZ_MISSING_ID_FILL;
    gather_options.fill_tuple = fill;
    CHECK(fviz_field_gather_by_ids(vectors, source_ids, target_ids, &gather_options,
        &gathered, &gather_mask) == FVIZ_OK);
    CHECK(fviz_data_array_get_component(gathered, 0u, 0u, &value) == FVIZ_OK && isnan(value));
    CHECK(fviz_data_array_get_component(gathered, 1u, 1u, &value) == FVIZ_OK && value == -2.0);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(gather_mask, 1u) == 0u);

    CHECK(fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &indices) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(indices, index_values, 3u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &weights) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(weights, weight_values, 3u) == FVIZ_OK);
    fviz_indexed_average_options_initialize(&average_options);
    average_options.destination_tuple_count = 2u;
    CHECK(fviz_field_indexed_weighted_average(component, indices, weights,
        &average_options, &averaged, &average_mask) == FVIZ_OK);
    CHECK(fviz_data_array_get_component(averaged, 0u, 0u, &value) == FVIZ_OK &&
        fabs(value - 1.75) < 1.0e-12);
    CHECK(fviz_data_array_get_component(averaged, 1u, 0u, &value) == FVIZ_OK && value == 5.0);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(average_mask, 0u) == 1u);

    CHECK(fviz_field_apply_tuple_matrix(component, &tuple_matrix[0][0],
        2u, &transformed) == FVIZ_OK);
    CHECK(fviz_data_array_get_component(transformed, 0u, 0u, &value) == FVIZ_OK && value == 4.0);
    CHECK(fviz_data_array_get_component(transformed, 1u, 0u, &value) == FVIZ_OK && value == 3.0);
    CHECK(fviz_field_least_squares_operator(
        &basis[0][0], 3u, 2u, &least_squares[0][0]) == FVIZ_OK);
    CHECK(fabs(least_squares[0][0] * 2.0 + least_squares[0][1] * 5.0 +
        least_squares[0][2] * 8.0 - 2.0) < 1.0e-12);
    CHECK(fviz_field_compute_indexed_discontinuity_mask(component, indices,
        2u, 0.5, &discontinuity) == FVIZ_OK);
    CHECK(((const uint8_t*)fviz_data_array_const_data(discontinuity))[0] == 1u);
    CHECK(((const uint8_t*)fviz_data_array_const_data(discontinuity))[1] == 0u);

    fviz_release(average_mask);
    fviz_release(discontinuity);
    fviz_release(transformed);
    fviz_release(averaged);
    fviz_release(weights);
    fviz_release(indices);
    fviz_release(gather_mask);
    fviz_release(gathered);
    fviz_release(target_ids);
    fviz_release(source_ids);
    fviz_release(finite);
    fviz_release(magnitude);
    fviz_release(component);
    fviz_release(vectors);
    return 0;
}
