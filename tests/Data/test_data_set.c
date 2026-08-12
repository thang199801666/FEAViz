#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizDataSet* data_set = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* replacement = NULL;
    float value = 42.0f;
    FVizMTime data_set_mtime;

    CHECK(fviz_data_set_create(&data_set) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(temperature, &value) == FVIZ_OK);
    CHECK(fviz_data_set_set_point_count(data_set, 1u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "temperature", temperature) == FVIZ_OK);
    CHECK(fviz_data_set_validate(data_set) == FVIZ_OK);
    CHECK(fviz_attribute_set_count(fviz_data_set_point_data(data_set)) == 1u);
    CHECK(fviz_attribute_set_const_get(fviz_data_set_point_data(data_set), "temperature") == temperature);
    data_set_mtime = fviz_object_mtime((const FVizObject*)data_set);
    value = 43.0f;
    CHECK(fviz_data_array_set_tuple(temperature, 0u, &value) == FVIZ_OK);
    CHECK(fviz_object_mtime((const FVizObject*)data_set) > data_set_mtime);
    data_set_mtime = fviz_object_mtime((const FVizObject*)data_set);
    *(float*)fviz_data_array_data(temperature) = 44.0f;
    fviz_object_modified((FVizObject*)temperature);
    CHECK(fviz_object_mtime((const FVizObject*)data_set) > data_set_mtime);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &replacement) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "temperature", replacement) == FVIZ_OK);
    CHECK(fviz_attribute_set_const_get(fviz_data_set_point_data(data_set), "temperature") == replacement);
    CHECK(fviz_attribute_set_remove(fviz_data_set_point_data(data_set), "temperature") == FVIZ_OK);
    CHECK(fviz_attribute_set_count(fviz_data_set_point_data(data_set)) == 0u);

    fviz_release(replacement);
    fviz_release(temperature);
    fviz_release(data_set);
    return 0;
}
