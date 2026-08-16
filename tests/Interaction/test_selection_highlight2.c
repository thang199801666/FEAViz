#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static FVizActor* make_triangle_actor(void)
{
    FVizActor* actor = NULL;
    FVizPolyData* data = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK || fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(-1.0f,-1.0f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 1.0f,-1.0f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.0f, 1.0f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data,0u,1u,2u) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor,data) != FVIZ_OK)
    { fviz_release(data); fviz_release(actor); return NULL; }
    fviz_release(data);
    return actor;
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizActor* actor = make_triangle_actor();
    FVizActor* glyph_actor = NULL;
    FVizGlyphMapper* glyphs = NULL;
    FVizGlyphInstance instance;
    FVizSelection* selection = NULL;
    FVizSelectionHighlight* highlight = NULL;
    const FVizPolyData* geometry;
    const FVizPolyData* points;

    CHECK(actor != NULL);
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
    CHECK(fviz_actor_create(&glyph_actor) == FVIZ_OK);
    CHECK(fviz_glyph_mapper_create(&glyphs) == FVIZ_OK);
    fviz_glyph_instance_initialize(&instance);
    instance.position = fviz_vec3(2.0f,0.0f,0.0f);
    CHECK(fviz_glyph_mapper_add_instance(glyphs,&instance) == FVIZ_OK);
    CHECK(fviz_actor_set_glyph_mapper(glyph_actor,glyphs) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer),glyph_actor) == FVIZ_OK);

    CHECK(fviz_selection_create(&selection) == FVIZ_OK);
    CHECK(fviz_selection_add(selection,actor,FVIZ_SELECTION_CELL,0u) == FVIZ_OK);
    CHECK(fviz_selection_add(selection,actor,FVIZ_SELECTION_POINT,1u) == FVIZ_OK);
    CHECK(fviz_selection_add(selection,actor,FVIZ_SELECTION_EDGE,2u) == FVIZ_OK);
    CHECK(fviz_selection_add(selection,actor,FVIZ_SELECTION_ACTOR,0u) == FVIZ_OK);
    CHECK(fviz_selection_add(selection,glyph_actor,FVIZ_SELECTION_GLYPH_INSTANCE,0u) == FVIZ_OK);
    CHECK(fviz_selection_highlight_create(renderer,selection,&highlight) == FVIZ_OK);
    CHECK(fviz_actor_pickable(fviz_selection_highlight_actor(highlight)) == FVIZ_FALSE);
    CHECK(fviz_actor_pickable(fviz_selection_highlight_point_actor(highlight)) == FVIZ_FALSE);
    geometry = fviz_actor_const_poly_data(fviz_selection_highlight_actor(highlight));
    points = fviz_actor_const_poly_data(fviz_selection_highlight_point_actor(highlight));
    CHECK(geometry != NULL && fviz_poly_data_triangle_count(geometry) == 1u);
    CHECK(fviz_poly_data_line_count(geometry) == 13u);
    CHECK(points != NULL && fviz_poly_data_point_count(points) == 2u);

    fviz_selection_highlight_set_enabled(highlight,FVIZ_FALSE);
    CHECK(fviz_actor_is_visible(fviz_selection_highlight_actor(highlight)) == FVIZ_FALSE);
    CHECK(fviz_actor_is_visible(fviz_selection_highlight_point_actor(highlight)) == FVIZ_FALSE);
    fviz_selection_highlight_set_enabled(highlight,FVIZ_TRUE);
    CHECK(fviz_actor_is_visible(fviz_selection_highlight_actor(highlight)) == FVIZ_TRUE);
    CHECK(fviz_actor_is_visible(fviz_selection_highlight_point_actor(highlight)) == FVIZ_TRUE);

    fviz_release(highlight);
    fviz_release(selection);
    fviz_release(glyphs);
    fviz_release(glyph_actor);
    fviz_release(actor);
    fviz_release(renderer);
    return 0;
}
