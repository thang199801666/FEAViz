#include <math.h>
#include <stdio.h>

#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizImageData.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

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
    FVizImageData* image = NULL;
    FVizDataArray* scalars = NULL;
    const int64_t extent[6] = {-1, 2, 3, 5, 0, 1};
    const double origin[3] = {10.0, -2.0, 5.0};
    const double spacing[3] = {0.5, 2.0, 3.0};
    const double direction[9] = {
        0.0, -1.0, 0.0,
        1.0,  0.0, 0.0,
        0.0,  0.0, 1.0
    };
    const double index[3] = {2.0, 4.0, 1.0};
    double physical[3];
    double recovered[3];
    FVizSize dims[3];
    FVizId id = FVIZ_INVALID_ID;
    int64_t ijk[3];
    FVizVec3 point;
    FVizBounds bounds;
    FVizId cell_id = FVIZ_INVALID_ID;
    FVizId cell_points[8];
    uint32_t cell_point_count = 0u;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    int modified_count = 0;

    CHECK(fviz_image_data_create(&image) == FVIZ_OK);
    CHECK(fviz_image_data_point_count(image) == 0u);
    CHECK(fviz_image_data_set_extent(image, extent) == FVIZ_OK);
    fviz_image_data_dimensions(image, dims);
    CHECK(dims[0] == 4u && dims[1] == 3u && dims[2] == 2u);
    CHECK(fviz_image_data_dimension(image) == 3u);
    CHECK(fviz_image_data_point_count(image) == 24u);
    CHECK(fviz_image_data_cell_count(image) == 6u);
    CHECK(fviz_image_data_cell_type(image) == FVIZ_CELL_HEXAHEDRON);

    CHECK(fviz_image_data_set_origin(image, origin) == FVIZ_OK);
    CHECK(fviz_image_data_set_spacing(image, spacing) == FVIZ_OK);
    CHECK(fviz_image_data_set_direction(image, direction) == FVIZ_OK);
    CHECK(fviz_image_data_index_to_physical(image, index, physical) == FVIZ_OK);
    CHECK(fabs(physical[0] - 2.0) < 1.0e-12);
    CHECK(fabs(physical[1] - -1.0) < 1.0e-12);
    CHECK(fabs(physical[2] - 8.0) < 1.0e-12);
    CHECK(fviz_image_data_physical_to_continuous_index(image, physical, recovered) == FVIZ_OK);
    CHECK(fabs(recovered[0] - index[0]) < 1.0e-12);
    CHECK(fabs(recovered[1] - index[1]) < 1.0e-12);
    CHECK(fabs(recovered[2] - index[2]) < 1.0e-12);

    CHECK(fviz_image_data_point_id(image, 2, 4, 1, &id) == FVIZ_OK);
    CHECK(id == 19u);
    CHECK(fviz_image_data_point_ijk(image, id, ijk) == FVIZ_OK);
    CHECK(ijk[0] == 2 && ijk[1] == 4 && ijk[2] == 1);
    CHECK(fviz_image_data_point(image, id, &point) == FVIZ_OK);
    CHECK(fabs((double)point.x - physical[0]) < 1.0e-6);
    CHECK(fabs((double)point.y - physical[1]) < 1.0e-6);
    CHECK(fabs((double)point.z - physical[2]) < 1.0e-6);

    CHECK(fviz_image_data_allocate_point_scalars(image, "Density", FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(scalars != NULL);
    CHECK(fviz_data_array_tuple_count(scalars) == 24u);
    CHECK(fviz_object_add_observer(
        (FVizObject*)image, FVIZ_EVENT_MODIFIED, 0.0f, count_modified, &modified_count, &modified_tag) == FVIZ_OK);
    modified_count = 0;
    CHECK(fviz_data_array_set_component(scalars, 0u, 0u, 4.0) == FVIZ_OK);
    CHECK(modified_count == 1);
    {
        float* values = (float*)fviz_data_array_data(scalars);
        FVizSize n;
        for (n = 0u; n < fviz_data_array_tuple_count(scalars); ++n)
        {
            int64_t pijk[3];
            CHECK(fviz_image_data_point_ijk(image, (FVizId)n, pijk) == FVIZ_OK);
            values[n] = (float)(pijk[0] + 2 * pijk[1] + 3 * pijk[2]);
        }
    }
    CHECK(fviz_image_data_validate(image) == FVIZ_OK);
    CHECK(fviz_image_data_cell_id(image, -1, 3, 0, &cell_id) == FVIZ_OK && cell_id == 0u);
    CHECK(fviz_image_data_cell_point_ids(image, cell_id, cell_points, &cell_point_count) == FVIZ_OK);
    CHECK(cell_point_count == 8u);
    {
        const double sample_index[3] = {-0.5, 3.5, 0.5};
        double sample_physical[3];
        double sampled = 0.0;
        CHECK(fviz_image_data_index_to_physical(image, sample_index, sample_physical) == FVIZ_OK);
        CHECK(fviz_image_data_sample_active_scalars(image, sample_physical, 0u, &sampled) == FVIZ_OK);
        CHECK(fabs(sampled - 8.0) < 1.0e-5);
    }
    fviz_release(scalars);

    bounds = fviz_image_data_bounds(image);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(bounds.min.x <= bounds.max.x);
    CHECK(bounds.min.y <= bounds.max.y);
    CHECK(bounds.min.z <= bounds.max.z);

    {
        const int64_t incompatible_extent[6] = {0, 1, 0, 1, 0, 1};
        CHECK(fviz_image_data_set_extent(image, incompatible_extent) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_image_data_point_count(image) == 24u);
    }

    CHECK(fviz_object_remove_observer((FVizObject*)image, modified_tag) == FVIZ_OK);
    fviz_release(image);
    return 0;
}
