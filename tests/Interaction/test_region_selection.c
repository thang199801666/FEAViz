#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static FVizActor* make_triangle_actor(void)
{
    FVizActor* actor = NULL;
    FVizPolyData* data = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK || fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(-0.75f, -0.6f, 0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.75f, -0.6f, 0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.0f,   0.75f, 0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data, 0u, 1u, 2u) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, data) != FVIZ_OK)
    {
        fviz_release(data);
        fviz_release(actor);
        return NULL;
    }
    fviz_release(data);
    return actor;
}

static FVizBool cancel_selection(void* user_data)
{
    FVizBool* cancel = (FVizBool*)user_data;
    return *cancel;
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizCamera* camera;
    FVizActor* mesh_actor = NULL;
    FVizActor* glyph_actor = NULL;
    FVizGlyphMapper* glyphs = NULL;
    FVizGlyphInstance instance;
    FVizSelection* selection = NULL;
    const int lasso[8] = {60, 60, 140, 60, 140, 140, 60, 140};

    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    camera = fviz_renderer_camera(renderer);
    fviz_camera_set_position(camera, fviz_vec3(0.0f, 0.0f, 5.0f));
    fviz_camera_set_target(camera, fviz_vec3(0.0f, 0.0f, 0.0f));
    fviz_camera_set_up(camera, fviz_vec3(0.0f, 1.0f, 0.0f));
    fviz_camera_set_perspective(camera, 45.0f, 0.1f, 100.0f);

    mesh_actor = make_triangle_actor();
    CHECK(mesh_actor != NULL);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), mesh_actor) == FVIZ_OK);

    CHECK(fviz_selection_select_rectangle(
        renderer, 200, 200, 0, 0, 199, 199, FVIZ_SELECTION_ACTOR, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_association(selection, 0u) == FVIZ_SELECTION_ACTOR);
    fviz_release(selection); selection = NULL;

    CHECK(fviz_selection_select_rectangle(
        renderer, 200, 200, 0, 0, 199, 199, FVIZ_SELECTION_POINT, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 3u);
    fviz_release(selection); selection = NULL;

    {
        const int full_region[8] = {0, 0, 199, 0, 199, 199, 0, 199};
        FVizSelectionRegionOptions options;
        FVizSelectionRegionStatistics statistics;
        FVizBool cancel = FVIZ_FALSE;
        fviz_selection_region_options_initialize(&options);
        options.maximum_results = 2u;
        CHECK(fviz_selection_select_polygon_with_options(
            renderer, 200, 200, full_region, 4u, FVIZ_SELECTION_POINT,
            &options, &statistics, &selection) == FVIZ_OK);
        CHECK(selection != NULL && fviz_selection_count(selection) == 2u);
        CHECK(statistics.results_returned == 2u);
        CHECK(statistics.overflow == FVIZ_TRUE);
        CHECK(statistics.candidates_tested == 3u);
        fviz_release(selection); selection = NULL;
        options.maximum_results = 0u;
        options.cancellation_check_interval = 1u;
        options.cancel = cancel_selection;
        options.cancel_user_data = &cancel;
        cancel = FVIZ_TRUE;
        CHECK(fviz_selection_select_polygon_with_options(
            renderer, 200, 200, full_region, 4u, FVIZ_SELECTION_POINT,
            &options, &statistics, &selection) == FVIZ_ERROR_CANCELLED);
        CHECK(selection == NULL);
        CHECK(statistics.cancelled == FVIZ_TRUE);
        options.cancel = NULL;
        options.visibility_policy = FVIZ_SELECTION_VISIBLE_ONLY;
        CHECK(fviz_selection_select_polygon_with_options(
            renderer, 200, 200, full_region, 4u, FVIZ_SELECTION_POINT,
            &options, &statistics, &selection) == FVIZ_ERROR_NOT_SUPPORTED);
        CHECK(selection == NULL);
    }

    CHECK(fviz_selection_select_rectangle(
        renderer, 200, 200, 0, 0, 199, 199, FVIZ_SELECTION_CELL, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    fviz_release(selection); selection = NULL;

    CHECK(fviz_selection_select_rectangle(
        renderer, 200, 200, 0, 0, 199, 199, FVIZ_SELECTION_EDGE, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 3u);
    fviz_release(selection); selection = NULL;

    {
        FVizFrustum frustum;
        CHECK(fviz_renderer_get_frustum(renderer,1.0f,&frustum) == FVIZ_OK);
        CHECK(fviz_selection_select_frustum(renderer,&frustum,FVIZ_SELECTION_CELL,&selection) == FVIZ_OK);
        CHECK(fviz_selection_count(selection) == 1u);
        fviz_release(selection); selection = NULL;
    }

    CHECK(fviz_selection_select_polygon(
        renderer, 200, 200, lasso, 4u, FVIZ_SELECTION_CELL, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    fviz_release(selection); selection = NULL;

    fviz_actor_set_pickable(mesh_actor, FVIZ_FALSE);
    CHECK(fviz_selection_select_polygon(
        renderer, 200, 200, lasso, 4u, FVIZ_SELECTION_CELL, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 0u);
    fviz_release(selection); selection = NULL;
    fviz_actor_set_pickable(mesh_actor, FVIZ_TRUE);

    CHECK(fviz_actor_create(&glyph_actor) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_create(&glyphs) == FVIZ_OK);
    fviz_glyph_instance_initialize(&instance);
    instance.position = fviz_vec3(0.0f, 0.0f, 0.0f);
    CHECK(fviz_glyph_mapper_add_instance(glyphs, &instance) == FVIZ_OK);
    instance.position = fviz_vec3(10.0f, 0.0f, 0.0f);
    CHECK(fviz_glyph_mapper_add_instance(glyphs, &instance) == FVIZ_OK);
    CHECK(fviz_actor_set_glyph_mapper(glyph_actor, glyphs) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), glyph_actor) == FVIZ_OK);

    CHECK(fviz_selection_select_polygon(
        renderer, 200, 200, lasso, 4u, FVIZ_SELECTION_GLYPH_INSTANCE, &selection) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_actor(selection, 0u) == glyph_actor);
    CHECK(fviz_selection_id(selection, 0u) == 0u);

    fviz_release(selection);
    fviz_release(glyphs);
    fviz_release(glyph_actor);
    fviz_release(mesh_actor);
    fviz_release(renderer);
    return 0;
}
