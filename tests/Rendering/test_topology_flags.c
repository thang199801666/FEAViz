#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "topology flags check failed at line %d: %s\n", __LINE__, #expr); \
    return __LINE__; \
} } while (0)

static int test_actor_topology_options(void)
{
    FVizActor* actor = NULL;
    FVizTopologyRenderOptions options;
    FVizTopologyRenderOptions read_back;
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);

    fviz_topology_render_options_initialize(&options);
    CHECK(options.coincident_mode == FVIZ_COINCIDENT_TOPOLOGY_DEFAULT);
    CHECK(options.depth_test == FVIZ_TRUE);
    CHECK(options.depth_function == FVIZ_DEPTH_FUNCTION_LEQUAL);
    CHECK(options.pass_order == FVIZ_RENDER_PASS_OPAQUE);

    options.coincident_mode = FVIZ_COINCIDENT_TOPOLOGY_POLYGON_OFFSET;
    options.offset_faces = FVIZ_TRUE;
    options.polygon_offset_factor = 2.5f;
    options.polygon_offset_units = 4.0f;
    options.line_offset_factor = 1.5f;
    options.line_offset_units = 2.0f;
    options.point_offset_units = 0.25f;
    options.z_shift = 0.001f;
    options.depth_test = FVIZ_TRUE;
    options.depth_write = FVIZ_FALSE;
    options.depth_function = FVIZ_DEPTH_FUNCTION_LESS;
    options.depth_range_minimum = 0.0f;
    options.depth_range_maximum = 0.95f;
    options.render_layer = 2;
    options.render_priority = 5;
    options.pass_order = FVIZ_RENDER_PASS_EDGE;
    options.overlay_mode = FVIZ_OVERLAY_TOPOLOGY_SURFACE_EDGES;
    options.topology_data_flags = FVIZ_TOPOLOGY_DATA_CONNECTIVITY | FVIZ_TOPOLOGY_DATA_ADJACENCY;
    fviz_actor_set_topology_render_options(actor, &options);

    fviz_actor_topology_render_options(actor, &read_back);
    CHECK(read_back.coincident_mode == FVIZ_COINCIDENT_TOPOLOGY_POLYGON_OFFSET);
    CHECK(fabs(read_back.polygon_offset_factor - 2.5f) < 1.0e-6f);
    CHECK(fabs(read_back.polygon_offset_units - 4.0f) < 1.0e-6f);
    CHECK(fabs(read_back.z_shift - 0.001f) < 1.0e-6f);
    CHECK(read_back.depth_write == FVIZ_FALSE);
    CHECK(read_back.depth_function == FVIZ_DEPTH_FUNCTION_LESS);
    CHECK(read_back.render_layer == 2);
    CHECK(read_back.pass_order == FVIZ_RENDER_PASS_EDGE);

    /* Individual accessors. */
    fviz_actor_set_coincident_topology_mode(actor, FVIZ_COINCIDENT_TOPOLOGY_SHIFT_Z_BUFFER);
    CHECK(fviz_actor_coincident_topology_mode(actor) == FVIZ_COINCIDENT_TOPOLOGY_SHIFT_Z_BUFFER);
    fviz_actor_set_polygon_offset(actor, 3.0f, 5.0f);
    {
        float factor = 0.0f;
        float units = 0.0f;
        fviz_actor_polygon_offset(actor, &factor, &units);
        CHECK(fabs(factor - 3.0f) < 1.0e-6f && fabs(units - 5.0f) < 1.0e-6f);
    }
    fviz_actor_set_depth_function(actor, FVIZ_DEPTH_FUNCTION_GEQUAL);
    CHECK(fviz_actor_depth_function(actor) == FVIZ_DEPTH_FUNCTION_GEQUAL);
    fviz_actor_set_depth_range(actor, 0.1f, 0.9f);
    {
        float minimum = 0.0f;
        float maximum = 0.0f;
        fviz_actor_get_depth_range(actor, &minimum, &maximum);
        CHECK(fabs(minimum - 0.1f) < 1.0e-6f && fabs(maximum - 0.9f) < 1.0e-6f);
    }
    fviz_actor_set_overlay_topology_mode(actor, FVIZ_OVERLAY_TOPOLOGY_SURFACE_POINTS);
    CHECK(fviz_actor_overlay_topology_mode(actor) == FVIZ_OVERLAY_TOPOLOGY_SURFACE_POINTS);
    fviz_actor_set_topology_data_flags(actor, FVIZ_TOPOLOGY_DATA_GLOBAL_IDS);
    CHECK(fviz_actor_topology_data_flags(actor) == FVIZ_TOPOLOGY_DATA_GLOBAL_IDS);

    fviz_release(actor);
    return 0;
}

static int test_topology_summary(void)
{
    FVizPolyData* poly = NULL;
    FVizDataArray* global_ids = NULL;
    FVizTopologySummary summary;
    const FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    const uint32_t triangles[6] = {0u, 1u, 2u, 0u, 2u, 3u};
    const uint64_t gids[4] = {100u, 101u, 102u, 103u};

    CHECK(fviz_poly_data_create(&poly) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(poly, points, 4u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangles(poly, triangles, 2u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &global_ids) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(global_ids, gids, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(poly), "GlobalNodeIds", global_ids) == FVIZ_OK);
    fviz_release(global_ids);

    CHECK(fviz_poly_data_topology_summary(poly, &summary) == FVIZ_OK);
    CHECK(summary.point_count == 4u);
    CHECK(summary.triangle_count == 2u);
    CHECK(summary.cell_count == 2u);
    CHECK(summary.connectivity_valid == FVIZ_TRUE);
    CHECK(summary.has_global_ids == FVIZ_TRUE);
    CHECK(summary.cell_classification_valid == FVIZ_TRUE);

    fviz_release(poly);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_actor_topology_options()) != 0)
    {
        fprintf(stderr, "test_actor_topology_options failed at %d\n", result);
        return result;
    }
    if ((result = test_topology_summary()) != 0)
    {
        fprintf(stderr, "test_topology_summary failed at %d\n", result);
        return result;
    }
    return 0;
}
