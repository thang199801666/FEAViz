#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_height_field(FVizPolyData** out_data, FVizDataArray** out_scalars)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizSize x;
    FVizSize y;
    const FVizSize n = 4u;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(scalars, n * n) != FVIZ_OK) return fviz_last_error_code();
    for (y = 0u; y < n; ++y)
    {
        for (x = 0u; x < n; ++x)
        {
            const float value = (float)(x + y);
            FVizSize index = y * n + x;
            if (fviz_poly_data_add_point(data, fviz_vec3((float)x, (float)y, 0.0f), NULL) != FVIZ_OK) return fviz_last_error_code();
            if (fviz_data_array_set_tuple(scalars, index, &value) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    for (y = 0u; y + 1u < n; ++y)
    {
        for (x = 0u; x + 1u < n; ++x)
        {
            const uint32_t a = (uint32_t)(y * n + x);
            const uint32_t b = (uint32_t)(a + 1u);
            const uint32_t c = (uint32_t)((y + 1u) * n + x);
            const uint32_t d = (uint32_t)(c + 1u);
            if (fviz_poly_data_add_triangle(data, a, c, b) != FVIZ_OK ||
                fviz_poly_data_add_triangle(data, b, c, d) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_poly_data_set_scalars(data, scalars) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_attribute_set_add(fviz_poly_data_point_data(data), "height", scalars) != FVIZ_OK) return fviz_last_error_code();
    *out_data = data;
    *out_scalars = scalars;
    return FVIZ_OK;
}

static int test_contour_lines(void)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizContourFilter* filter = NULL;
    FVizPolyData* output = NULL;
    FVizPolyData* updated_output;
    const float levels[1] = {3.0f};
    CHECK(build_height_field(&data, &scalars) == FVIZ_OK);
    CHECK(fviz_contour_filter_create("height", levels, 1u, &filter) == FVIZ_OK);
    CHECK(filter != NULL);
    CHECK(fviz_contour_filter_level_count(filter) == 1u);
    CHECK(fviz_contour_filter_set_input(filter, data) == FVIZ_OK);
    CHECK(fviz_contour_filter_update(filter) == FVIZ_OK);
    output = fviz_contour_filter_output(filter);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_line_count(output) > 0u);
    {
        const FVizVec3* points = fviz_poly_data_points(output);
        FVizSize i;
        FVizBool found_line = FVIZ_FALSE;
        const uint32_t* line_indices = fviz_poly_data_line_indices(output);
        const FVizSize line_count = fviz_poly_data_line_count(output);
        for (i = 0u; i < line_count && !found_line; ++i)
        {
            const uint32_t a = line_indices[i * 2u + 0u];
            const uint32_t b = line_indices[i * 2u + 1u];
            FVizVec3 midpoint = fviz_vec3_scale(fviz_vec3_add(points[a], points[b]), 0.5f);
            if (fabsf(midpoint.x + midpoint.y - 3.0f) < 0.5f) found_line = FVIZ_TRUE;
        }
        CHECK(found_line == FVIZ_TRUE);
    }
    CHECK(fviz_contour_filter_update(filter) == FVIZ_OK);
    CHECK(fviz_contour_filter_output(filter) == output);
    CHECK(fviz_retain(output) == output);
    {
        FVizSize i;
        const float zero = 0.0f;
        for (i = 0u; i < fviz_data_array_tuple_count(scalars); ++i)
            CHECK(fviz_data_array_set_tuple(scalars, i, &zero) == FVIZ_OK);
    }
    CHECK(fviz_contour_filter_update(filter) == FVIZ_OK);
    updated_output = fviz_contour_filter_output(filter);
    CHECK(updated_output != NULL && updated_output != output);
    CHECK(fviz_poly_data_line_count(updated_output) == 0u);
    fviz_release(output);
    fviz_release(filter);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}

static int test_contour_no_scalar(void)
{
    FVizPolyData* data = NULL;
    FVizContourFilter* filter = NULL;
    const float levels[1] = {1.0f};
    uint32_t a, b, c;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return 1;
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,0,0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,0,0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,1,0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, b, c) == FVIZ_OK);
    CHECK(fviz_contour_filter_create("missing", levels, 1u, &filter) == FVIZ_OK);
    CHECK(fviz_contour_filter_set_input(filter, data) == FVIZ_OK);
    CHECK(fviz_contour_filter_update(filter) == FVIZ_ERROR_NOT_FOUND);
    fviz_release(filter);
    fviz_release(data);
    return 0;
}

static int test_contour_thread_equivalence(void)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizContourFilter* filter = NULL;
    FVizContourFilter* parallel_filter = NULL;
    FVizPolyData* serial;
    FVizPolyData* parallel;
    const float levels[2] = {2.0f, 4.0f};
    CHECK(build_height_field(&data, &scalars) == FVIZ_OK);
    CHECK(fviz_contour_filter_create("height", levels, 2u, &filter) == FVIZ_OK);
    CHECK(fviz_contour_filter_create("height", levels, 2u, &parallel_filter) == FVIZ_OK);
    CHECK(fviz_contour_filter_set_input(filter, data) == FVIZ_OK);
    fviz_parallel_set_thread_limit(1u);
    CHECK(fviz_contour_filter_update(filter) == FVIZ_OK);
    serial = fviz_contour_filter_output(filter);
    CHECK(serial != NULL);
    fviz_parallel_set_thread_limit(4u);
    CHECK(fviz_contour_filter_set_input(parallel_filter, data) == FVIZ_OK);
    CHECK(fviz_contour_filter_update(parallel_filter) == FVIZ_OK);
    parallel = fviz_contour_filter_output(parallel_filter);
    CHECK(parallel != NULL);
    CHECK(fviz_poly_data_line_count(serial) == fviz_poly_data_line_count(parallel));
    CHECK(fviz_poly_data_point_count(serial) == fviz_poly_data_point_count(parallel));
    CHECK(memcmp(fviz_poly_data_line_indices(serial), fviz_poly_data_line_indices(parallel),
        fviz_poly_data_line_count(serial) * 2u * sizeof(uint32_t)) == 0);
    fviz_parallel_set_thread_limit(0u);
    fviz_release(parallel_filter);
    fviz_release(filter);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}

static int test_contour_numeric_types(void)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizContourFilter* filter = NULL;
    const FVizVec3 points[3] = {{0,0,0},{1,0,0},{0,1,0}};
    const double values[3] = {0.0, 2.0, 2.0};
    const float level[1] = {1.0f};
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(data, points, 3u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(scalars, values, 3u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "height64", scalars) == FVIZ_OK);
    CHECK(fviz_contour_filter_create("height64", level, 1u, &filter) == FVIZ_OK);
    CHECK(fviz_contour_filter_set_input(filter, data) == FVIZ_OK);
    CHECK(fviz_contour_filter_update(filter) == FVIZ_OK);
    CHECK(fviz_poly_data_line_count(fviz_contour_filter_output(filter)) == 1u);
    fviz_release(filter);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}


int main(void)
{
    CHECK(test_contour_lines() == 0);
    CHECK(test_contour_no_scalar() == 0);
    CHECK(test_contour_numeric_types() == 0);
    return test_contour_thread_equivalence();
}
