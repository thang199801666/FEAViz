#include <FViz/FViz.h>

#include <stdio.h>

int main(void)
{
    FVizImageData* image = NULL;
    FVizImageDataGeometryFilter* geometry = NULL;
    FVizDataArray* scalars = NULL;
    FVizPolyData* surface;
    const int64_t extent[6] = {0,20,0,15,0,10};
    const double origin[3] = {-10.0,-7.5,-5.0};
    const double spacing[3] = {1.0,1.0,1.0};
    const double sample_point[3] = {0.0,0.0,0.0};
    FVizSize point_count;
    FVizSize i;
    double sampled = 0.0;

    if (fviz_image_data_create(&image) != FVIZ_OK ||
        fviz_image_data_set_extent(image, extent) != FVIZ_OK ||
        fviz_image_data_set_origin(image, origin) != FVIZ_OK ||
        fviz_image_data_set_spacing(image, spacing) != FVIZ_OK ||
        fviz_image_data_allocate_point_scalars(image, "Distance2", FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK)
        goto fail;

    point_count = fviz_image_data_point_count(image);
    for (i = 0u; i < point_count; ++i)
    {
        FVizVec3 point;
        const float value = (fviz_image_data_point(image, (FVizId)i, &point) == FVIZ_OK)
            ? point.x*point.x + point.y*point.y + point.z*point.z : 0.0f;
        if (fviz_data_array_set_tuple(scalars, i, &value) != FVIZ_OK) goto fail;
    }
    if (fviz_image_data_sample_active_scalars(image, sample_point, 0u, &sampled) != FVIZ_OK ||
        fviz_image_data_geometry_filter_create(&geometry) != FVIZ_OK ||
        fviz_image_data_geometry_filter_set_input_data(geometry, image) != FVIZ_OK ||
        fviz_image_data_geometry_filter_update(geometry) != FVIZ_OK)
        goto fail;

    surface = fviz_image_data_geometry_filter_output(geometry);
    if (surface == NULL || fviz_poly_data_validate(surface) != FVIZ_OK) goto fail;

    printf("FEAViz 0.26 ImageData\n");
    printf("points=%llu cells=%llu surface-points=%llu surface-triangles=%llu sample=%.3f\n",
        (unsigned long long)fviz_image_data_point_count(image),
        (unsigned long long)fviz_image_data_cell_count(image),
        (unsigned long long)fviz_poly_data_point_count(surface),
        (unsigned long long)fviz_poly_data_triangle_count(surface),
        sampled);
    if (fviz_image_data_point_count(image) != 3696u ||
        fviz_image_data_cell_count(image) != 3000u ||
        fviz_poly_data_triangle_count(surface) != 2600u || sampled < 0.249999 || sampled > 0.250001)
        goto fail;

    fviz_release(geometry);
    fviz_release(scalars);
    fviz_release(image);
    return 0;
fail:
    fviz_release(geometry);
    fviz_release(scalars);
    fviz_release(image);
    return 1;
}
