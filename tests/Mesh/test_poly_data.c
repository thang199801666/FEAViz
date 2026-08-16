#include <math.h>
#include <FViz/FViz.h>

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
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    uint32_t a,b,c;
    const FVizVec3* normals;
    float scalar = 1.0f;
    FVizMTime poly_mtime;
    FVizMTime geometry_mtime;
    FVizMTime topology_mtime;
    FVizMTime attribute_mtime;
    FVizPolyData* shallow = NULL;
    FVizPolyData* deep = NULL;
    FVizPolyData* structure = NULL;
    FVizDataArray* attribute_only = NULL;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizDirtyRange dirty_range;
    int modified_count = 0;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,0,0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,0,0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,1,0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a,b,c) == FVIZ_OK);
    CHECK(fviz_poly_data_validate(data) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(data) == FVIZ_OK);
    normals = fviz_poly_data_normals(data);
    CHECK(normals != NULL);
    CHECK(fabsf(normals[0].z - 1.0f) < 1.0e-6f);
    CHECK(fviz_poly_data_triangle_count(data) == 1u);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(scalars, &scalar) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(scalars, &scalar) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(scalars, &scalar) == FVIZ_OK);
    CHECK(fviz_poly_data_set_scalars(data, scalars) == FVIZ_OK);
    poly_mtime = fviz_object_mtime((const FVizObject*)data);
    scalar = 2.0f;
    CHECK(fviz_data_array_set_tuple(scalars, 1u, &scalar) == FVIZ_OK);
    CHECK(fviz_object_mtime((const FVizObject*)data) > poly_mtime);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "result", scalars) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &attribute_only) == FVIZ_OK);
    CHECK(fviz_data_array_resize(attribute_only, 3u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "attribute_only", attribute_only) == FVIZ_OK);
    CHECK(fviz_object_add_observer(
        (FVizObject*)data, FVIZ_EVENT_MODIFIED, 0.0f, count_modified, &modified_count, &modified_tag) == FVIZ_OK);
    modified_count = 0;
    geometry_mtime = fviz_poly_data_geometry_mtime(data);
    topology_mtime = fviz_poly_data_topology_mtime(data);
    attribute_mtime = fviz_poly_data_attribute_mtime(data);
    scalar = 3.0f;
    CHECK(fviz_data_array_set_tuple(attribute_only, 2u, &scalar) == FVIZ_OK);
    CHECK(modified_count == 1);
    CHECK(fviz_poly_data_geometry_mtime(data) == geometry_mtime);
    CHECK(fviz_poly_data_topology_mtime(data) == topology_mtime);
    CHECK(fviz_poly_data_attribute_mtime(data) > attribute_mtime);
    attribute_mtime = fviz_poly_data_attribute_mtime(data);
    CHECK(fviz_poly_data_set_point(data, 0u, fviz_vec3(0.0f, 0.0f, 0.0f)) == FVIZ_OK);
    CHECK(fviz_poly_data_geometry_mtime(data) == geometry_mtime);
    CHECK(fviz_poly_data_attribute_mtime(data) == attribute_mtime);
    CHECK(fviz_poly_data_set_point(data, 0u, fviz_vec3(0.0f, 0.0f, 0.25f)) == FVIZ_OK);
    CHECK(fviz_poly_data_geometry_mtime(data) > geometry_mtime);
    CHECK(fviz_poly_data_geometry_dirty_range_since(data, geometry_mtime, &dirty_range) == FVIZ_OK);
    CHECK(dirty_range.full == FVIZ_FALSE && dirty_range.first == 0u && dirty_range.count == 1u);
    CHECK(fviz_poly_data_topology_mtime(data) == topology_mtime);
    CHECK(fviz_poly_data_attribute_mtime(data) == attribute_mtime);
    CHECK(fviz_poly_data_shallow_copy(data, &shallow) == FVIZ_OK);
    CHECK(fviz_poly_data_deep_copy(data, &deep) == FVIZ_OK);
    CHECK(fviz_poly_data_copy_structure(data, &structure) == FVIZ_OK);
    CHECK(fviz_poly_data_points(shallow) == fviz_poly_data_points(data));
    CHECK(fviz_attribute_set_const_get(
        fviz_poly_data_const_point_data(shallow), "result") == scalars);
    CHECK(fviz_poly_data_points(deep) != fviz_poly_data_points(data));
    CHECK(fviz_attribute_set_const_get(
        fviz_poly_data_const_point_data(deep), "result") != scalars);
    CHECK(fviz_attribute_set_count(fviz_poly_data_const_point_data(structure)) == 0u);
    CHECK(fviz_poly_data_triangle_count(structure) == 1u);
    CHECK(fviz_poly_data_memory_size(data) >= 3u * sizeof(FVizVec3));
    scalar = 7.0f;
    CHECK(fviz_data_array_set_tuple(scalars, 0u, &scalar) == FVIZ_OK);
    CHECK(*(const float*)fviz_data_array_const_tuple(
        fviz_attribute_set_const_get(fviz_poly_data_const_point_data(shallow), "result"), 0u) == 7.0f);
    CHECK(*(const float*)fviz_data_array_const_tuple(
        fviz_attribute_set_const_get(fviz_poly_data_const_point_data(deep), "result"), 0u) != 7.0f);
    CHECK(fviz_object_remove_observer((FVizObject*)data, modified_tag) == FVIZ_OK);
    fviz_release(attribute_only);
    fviz_release(structure);
    fviz_release(deep);
    fviz_release(shallow);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}
