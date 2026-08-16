#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int nearf32(float a, float b)
{
    return fabsf(a - b) <= 1.0e-4f;
}

static int test_actor_primitives(void)
{
    FVizActor* actor = NULL;
    float dash = 0.0f, gap = 0.0f, phase = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);

    fviz_actor_set_line_width(actor, 4.5f);
    fviz_actor_set_line_depth_bias(actor, 0.0002f);
    fviz_actor_set_line_cap(actor, FVIZ_LINE_CAP_ROUND);
    fviz_actor_set_line_join(actor, FVIZ_LINE_JOIN_MITER);
    fviz_actor_set_line_miter_limit(actor, 6.0f);
    fviz_actor_set_line_dash(actor, 12.0f, 6.0f, 3.0f);
    fviz_actor_set_line_scalar_coloring(actor, FVIZ_TRUE);
    CHECK(nearf32(fviz_actor_line_width(actor), 4.5f));
    CHECK(nearf32(fviz_actor_line_depth_bias(actor), 0.0002f));
    CHECK(fviz_actor_line_cap(actor) == FVIZ_LINE_CAP_ROUND);
    CHECK(fviz_actor_line_join(actor) == FVIZ_LINE_JOIN_MITER);
    CHECK(nearf32(fviz_actor_line_miter_limit(actor), 6.0f));
    fviz_actor_get_line_dash(actor, &dash, &gap, &phase);
    CHECK(nearf32(dash, 12.0f));
    CHECK(nearf32(gap, 6.0f));
    CHECK(nearf32(phase, 3.0f));
    CHECK(fviz_actor_line_scalar_coloring(actor) == FVIZ_TRUE);

    fviz_actor_set_point_visibility(actor, FVIZ_TRUE);
    fviz_actor_set_point_size(actor, 13.0f);
    fviz_actor_set_point_shape(actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_set_point_color(actor, 0.2f, 0.4f, 0.8f);
    fviz_actor_set_point_scalar_coloring(actor, FVIZ_TRUE);
    CHECK(fviz_actor_point_visibility(actor) == FVIZ_TRUE);
    CHECK(nearf32(fviz_actor_point_size(actor), 13.0f));
    CHECK(fviz_actor_point_shape(actor) == FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_get_point_color(actor, &r, &g, &b);
    CHECK(nearf32(r, 0.2f) && nearf32(g, 0.4f) && nearf32(b, 0.8f));
    CHECK(fviz_actor_point_scalar_coloring(actor) == FVIZ_TRUE);

    /* Invalid/oversized values are normalized rather than leaking invalid GL state. */
    fviz_actor_set_point_size(actor, 1000.0f);
    CHECK(nearf32(fviz_actor_point_size(actor), 256.0f));
    fviz_actor_set_line_dash(actor, -2.0f, -1.0f, -3.0f);
    fviz_actor_get_line_dash(actor, &dash, &gap, &phase);
    CHECK(dash >= 0.0f && gap >= 0.0f && phase >= 0.0f);

    fviz_release(actor);
    return 0;
}

static int test_glyph_mapper_and_scene_bounds(void)
{
    FVizPolyData* source = NULL;
    FVizGlyphMapper* glyphs = NULL;
    FVizActor* actor = NULL;
    FVizScene* scene = NULL;
    FVizGlyphInstance instances[2];
    FVizGlyphInstance readback;
    FVizBounds bounds;
    FVizVec3 p[3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    uint32_t tri[3] = {0u, 1u, 2u};

    CHECK(fviz_poly_data_create(&source) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(source, p, 3u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangles(source, tri, 1u) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_create(&glyphs) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_set_source_poly_data(glyphs, source) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_reserve_instances(glyphs, 64u) == FVIZ_OK);

    fviz_glyph_instance_initialize(&instances[0]);
    instances[0].position = fviz_vec3(2.0f, 3.0f, 4.0f);
    instances[0].scale = fviz_vec3(2.0f, 1.0f, 1.0f);
    instances[0].color[0] = 0.1f;
    instances[0].color[1] = 0.2f;
    instances[0].color[2] = 0.3f;

    fviz_glyph_instance_initialize(&instances[1]);
    instances[1].position = fviz_vec3(-4.0f, -2.0f, 1.0f);
    instances[1].orientation = fviz_quat_from_axis_angle(fviz_vec3(0.0f, 0.0f, 1.0f), 1.57079632679f);
    instances[1].color[3] = 0.5f;

    CHECK(fviz_glyph_mapper_add_instances(glyphs, instances, 2u) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_instance_count(glyphs) == 2u);
    CHECK(fviz_glyph_mapper_has_translucent_instances(glyphs) == FVIZ_TRUE);
    CHECK(fviz_glyph_mapper_get_instance(glyphs, 1u, &readback) == FVIZ_OK);
    CHECK(nearf32(readback.position.x, -4.0f));
    CHECK(nearf32(readback.color[3], 0.5f));
    instances[1].position.x = -3.5f;
    instances[1].color[3] = 1.0f;
    CHECK(fviz_glyph_mapper_set_instance(glyphs, 1u, &instances[1]) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_get_instance(glyphs, 1u, &readback) == FVIZ_OK);
    CHECK(nearf32(readback.position.x, -3.5f));
    CHECK(fviz_glyph_mapper_has_translucent_instances(glyphs) == FVIZ_FALSE);

    bounds = fviz_glyph_mapper_bounds(glyphs);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(bounds.min.x <= -4.5f + 1.0e-3f);
    CHECK(bounds.max.x >= 4.0f - 1.0e-3f);
    CHECK(bounds.min.y <= -2.0f + 1.0e-3f);
    CHECK(bounds.max.y >= 4.0f - 1.0e-3f);

    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_glyph_mapper(actor, glyphs) == FVIZ_OK);
    fviz_actor_set_position(actor, fviz_vec3(10.0f, 20.0f, 30.0f));
    CHECK(fviz_scene_create(&scene) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    bounds = fviz_scene_bounds(scene);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(bounds.min.x > 4.9f && bounds.max.x > 13.9f);
    CHECK(bounds.min.y > 17.9f && bounds.max.y > 23.9f);
    CHECK(bounds.min.z >= 31.0f - 1.0e-3f && bounds.max.z >= 34.0f - 1.0e-3f);

    /* Standard geometry assignment intentionally leaves glyph mode. */
    CHECK(fviz_actor_set_poly_data(actor, source) == FVIZ_OK);
    CHECK(fviz_actor_glyph_mapper(actor) == NULL);

    fviz_glyph_mapper_clear_instances(glyphs);
    CHECK(fviz_glyph_mapper_instance_count(glyphs) == 0u);
    CHECK(fviz_glyph_mapper_has_translucent_instances(glyphs) == FVIZ_FALSE);
    CHECK(fviz_glyph_mapper_bounds(glyphs).valid == FVIZ_FALSE);

    {
        FVizDataArray* vectors = NULL;
        FVizVectorGlyphOptions options;
        const float tuples[9] = {1.0f,0.0f,0.0f, 0.0f,2.0f,0.0f, 0.0f,0.0f,0.0f};
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &vectors) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(vectors, tuples, 3u) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(source), "Velocity", vectors) == FVIZ_OK);
        CHECK(fviz_attribute_set_set_active(fviz_poly_data_point_data(source), FVIZ_ATTRIBUTE_VECTORS, "Velocity") == FVIZ_OK);
        fviz_vector_glyph_options_initialize(&options);
        options.scale_factor = 0.5f;
        options.opacity = 0.75f;
        CHECK(fviz_glyph_mapper_build_from_point_vectors(glyphs, source, NULL, &options) == FVIZ_OK);
        CHECK(fviz_glyph_mapper_instance_count(glyphs) == 2u);
        CHECK(fviz_glyph_mapper_has_translucent_instances(glyphs) == FVIZ_TRUE);
        CHECK(fviz_glyph_mapper_get_instance(glyphs, 0u, &readback) == FVIZ_OK);
        CHECK(nearf32(readback.scale.x, 0.5f));
        CHECK(fviz_glyph_mapper_get_instance(glyphs, 1u, &readback) == FVIZ_OK);
        CHECK(nearf32(readback.scale.x, 1.0f));
        CHECK(nearf32(readback.color[3], 0.75f));
        {
            FVizVec3 rotated = fviz_quat_rotate_vec3(readback.orientation, fviz_vec3(1.0f, 0.0f, 0.0f));
            CHECK(fabsf(rotated.x) < 1.0e-4f);
            CHECK(rotated.y > 0.999f);
        }
        fviz_release(vectors);
    }

    fviz_release(scene);
    fviz_release(actor);
    fviz_release(glyphs);
    fviz_release(source);
    return 0;
}

int main(void)
{
    if (test_actor_primitives() != 0) return 1;
    if (test_glyph_mapper_and_scene_bounds() != 0) return 1;
    return 0;
}
