#include <FViz/FViz.h>

#include <math.h>
#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

int main(void)
{
    const FVizSize count = 100000u;
    FVizArrowSource* arrow = NULL;
    FVizGlyphMapper* mapper = NULL;
    FVizPolyData* input = NULL;
    FVizDataArray* vectors = NULL;
    FVizGlyphInstance* instances = NULL;
    FVizVec3* points = NULL;
    float* vector_tuples = NULL;
    FVizBounds bounds;
    FVizVectorGlyphOptions vector_options;
    FVizSize i;
    double start;
    double append_seconds;
    double bounds_seconds;
    double vector_build_seconds;

    instances = (FVizGlyphInstance*)fviz_alloc(count * sizeof(*instances));
    points = (FVizVec3*)fviz_alloc(count * sizeof(*points));
    vector_tuples = (float*)fviz_alloc(count * 3u * sizeof(*vector_tuples));
    if (instances == NULL || points == NULL || vector_tuples == NULL) return 1;
    for (i = 0u; i < count; ++i)
    {
        const float u = (float)i / (float)(count - 1u);
        const float angle = u * 62.8318530718f;
        points[i] = fviz_vec3(cosf(angle) * 100.0f, sinf(angle) * 100.0f, u * 50.0f);
        vector_tuples[i * 3u + 0u] = cosf(angle) * (0.25f + u);
        vector_tuples[i * 3u + 1u] = sinf(angle) * (0.25f + u);
        vector_tuples[i * 3u + 2u] = 0.1f + 0.2f * u;
        fviz_glyph_instance_initialize(&instances[i]);
        instances[i].position = points[i];
        instances[i].orientation = fviz_quat_from_axis_angle(fviz_vec3(0.0f, 0.0f, 1.0f), angle);
        instances[i].scale = fviz_vec3(0.15f, 0.15f, 0.15f);
        instances[i].color[0] = u;
        instances[i].color[2] = 1.0f - u;
    }
    if (fviz_arrow_source_create(&arrow) != FVIZ_OK ||
        fviz_arrow_source_update(arrow) != FVIZ_OK ||
        fviz_glyph_mapper_create(&mapper) != FVIZ_OK ||
        fviz_glyph_mapper_set_source_poly_data(mapper, fviz_arrow_source_output(arrow)) != FVIZ_OK)
        return 2;

    start = wall_seconds();
    if (fviz_glyph_mapper_add_instances(mapper, instances, count) != FVIZ_OK) return 3;
    append_seconds = wall_seconds() - start;
    start = wall_seconds();
    bounds = fviz_glyph_mapper_bounds(mapper);
    bounds_seconds = wall_seconds() - start;
    if (bounds.valid == FVIZ_FALSE || fviz_glyph_mapper_instance_count(mapper) != count) return 4;

    if (fviz_poly_data_create(&input) != FVIZ_OK ||
        fviz_poly_data_add_points(input, points, count, NULL) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &vectors) != FVIZ_OK ||
        fviz_data_array_append_tuples(vectors, vector_tuples, count) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_point_data(input), "Vectors", vectors) != FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_poly_data_point_data(input), FVIZ_ATTRIBUTE_VECTORS, "Vectors") != FVIZ_OK)
        return 5;
    fviz_vector_glyph_options_initialize(&vector_options);
    vector_options.scale_factor = 0.2f;
    start = wall_seconds();
    if (fviz_glyph_mapper_build_from_point_vectors(mapper, input, NULL, &vector_options) != FVIZ_OK)
        return 6;
    vector_build_seconds = wall_seconds() - start;
    if (fviz_glyph_mapper_instance_count(mapper) != count) return 7;

    puts("instances,bulk_append_seconds,bounds_seconds,vector_build_seconds,bulk_instances_per_second");
    printf("%llu,%.9f,%.9f,%.9f,%.0f\n",
        (unsigned long long)count, append_seconds, bounds_seconds, vector_build_seconds,
        append_seconds > 0.0 ? (double)count / append_seconds : 0.0);

    fviz_release(vectors);
    fviz_release(input);
    fviz_free(vector_tuples);
    fviz_free(points);
    fviz_free(instances);
    fviz_release(mapper);
    fviz_release(arrow);
    return 0;
}
