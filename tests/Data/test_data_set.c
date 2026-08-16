#include <FViz/FViz.h>

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizBool count_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    int* count = (int*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    ++(*count);
    return FVIZ_FALSE;
}

int main(void)
{
    FVizDataSet* data_set = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* replacement = NULL;
    float value = 42.0f;
    FVizMTime data_set_mtime;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    int modified_count = 0;

    CHECK(fviz_data_set_create(&data_set) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(temperature, &value) == FVIZ_OK);
    CHECK(fviz_data_set_set_point_count(data_set, 1u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "temperature", temperature) == FVIZ_OK);
    CHECK(fviz_data_set_validate(data_set) == FVIZ_OK);
    CHECK(fviz_attribute_set_count(fviz_data_set_point_data(data_set)) == 1u);
    CHECK(fviz_attribute_set_const_get(fviz_data_set_point_data(data_set), "temperature") == temperature);
    CHECK(fviz_attribute_set_set_active(
        fviz_data_set_point_data(data_set), FVIZ_ATTRIBUTE_SCALARS, "temperature") == FVIZ_OK);
    CHECK(fviz_attribute_set_const_active(
        fviz_data_set_point_data(data_set), FVIZ_ATTRIBUTE_SCALARS) == temperature);
    CHECK(fviz_object_add_observer(
        (FVizObject*)data_set, FVIZ_EVENT_MODIFIED, 0.0f, count_modified, &modified_count, &modified_tag) == FVIZ_OK);
    modified_count = 0;
    data_set_mtime = fviz_object_mtime((const FVizObject*)data_set);
    value = 43.0f;
    CHECK(fviz_data_array_set_tuple(temperature, 0u, &value) == FVIZ_OK);
    CHECK(fviz_object_mtime((const FVizObject*)data_set) > data_set_mtime);
    CHECK(modified_count == 1);
    modified_count = 0;
    data_set_mtime = fviz_object_mtime((const FVizObject*)data_set);
    *(float*)fviz_data_array_data(temperature) = 44.0f;
    fviz_object_modified((FVizObject*)temperature);
    CHECK(fviz_object_mtime((const FVizObject*)data_set) > data_set_mtime);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &replacement) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_data_set_point_data(data_set), "temperature", replacement) == FVIZ_OK);
    CHECK(fviz_attribute_set_const_get(fviz_data_set_point_data(data_set), "temperature") == replacement);
    CHECK(fviz_attribute_set_const_active(
        fviz_data_set_point_data(data_set), FVIZ_ATTRIBUTE_SCALARS) == replacement);
    CHECK(fviz_attribute_set_remove(fviz_data_set_point_data(data_set), "temperature") == FVIZ_OK);
    CHECK(fviz_attribute_set_active_name(
        fviz_data_set_point_data(data_set), FVIZ_ATTRIBUTE_SCALARS) == NULL);
    CHECK(fviz_attribute_set_count(fviz_data_set_point_data(data_set)) == 0u);

    /* Exercise the adaptive name index, including index refresh after erase. */
    {
        char name[32];
        FVizSize i;
        for (i = 0u; i < 32u; ++i)
        {
            (void)snprintf(name, sizeof(name), "field_%04llu", (unsigned long long)i);
            CHECK(fviz_attribute_set_add(
                fviz_data_set_point_data(data_set), name, replacement) == FVIZ_OK);
        }
        CHECK(fviz_attribute_set_get(
            fviz_data_set_point_data(data_set), "field_0031") == replacement);
        CHECK(fviz_attribute_set_remove(
            fviz_data_set_point_data(data_set), "field_0010") == FVIZ_OK);
        CHECK(fviz_attribute_set_get(
            fviz_data_set_point_data(data_set), "field_0010") == NULL);
        CHECK(fviz_attribute_set_get(
            fviz_data_set_point_data(data_set), "field_0031") == replacement);
        CHECK(fviz_attribute_set_get(
            fviz_data_set_point_data(data_set), "field_0030") == replacement);
        fviz_attribute_set_clear(fviz_data_set_point_data(data_set));
        CHECK(fviz_attribute_set_count(fviz_data_set_point_data(data_set)) == 0u);
    }

    CHECK(fviz_object_remove_observer((FVizObject*)data_set, modified_tag) == FVIZ_OK);
    fviz_release(replacement);
    fviz_release(temperature);
    fviz_release(data_set);
    return 0;
}
