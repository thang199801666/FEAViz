#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) \
    do \
    { \
        if (!(expr)) \
        { \
            fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    FVizCacheKey first;
    FVizCacheKey second;
    FVizPolyData* poly_data = NULL;
    FVizDataArray* array = NULL;
    FVizDataSet* data_set = NULL;
    FVizDataArray* values = NULL;
    FVizMTime before;

    first = fviz_cache_key_append_string(
        fviz_cache_key_append_u64(fviz_cache_key_initialize(), 42u), "field");
    second = fviz_cache_key_append_string(
        fviz_cache_key_append_u64(fviz_cache_key_initialize(), 42u), "field");
    CHECK(first != 0u && first == second);

    fviz_release(NULL);
    CHECK(fviz_poly_data_create(NULL) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, NULL) == FVIZ_ERROR_INVALID_ARGUMENT);

    poly_data = (FVizPolyData*)(uintptr_t)1u;
    CHECK(fviz_poly_data_create(&poly_data) == FVIZ_OK);
    CHECK(poly_data != NULL);
    before = fviz_object_mtime((const FVizObject*)poly_data);
    CHECK(fviz_retain(poly_data) == poly_data);
    CHECK(fviz_object_ref_count((const FVizObject*)poly_data) == 2u);
    fviz_release(poly_data);
    CHECK(fviz_object_ref_count((const FVizObject*)poly_data) == 1u);
    CHECK(fviz_object_mtime((const FVizObject*)poly_data) == before);

    array = (FVizDataArray*)(uintptr_t)1u;
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &array) == FVIZ_OK);
    CHECK(array != NULL);
    CHECK(fviz_data_array_resize(array, 1u) == FVIZ_OK);
    CHECK(fviz_data_array_set_tuple(array, 0u, &(const float){3.0f}) == FVIZ_OK);

    CHECK(fviz_data_set_create(&data_set) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "value", array) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "value", array) == FVIZ_OK);
    values = fviz_attribute_set_get(fviz_data_set_point_data(data_set), "value");
    CHECK(values == array);
    CHECK(fviz_data_array_tuple_count(values) == 1u);

    fviz_release(data_set);
    fviz_release(array);
    fviz_release(poly_data);
    puts("public API contract tests passed");
    return 0;
}
