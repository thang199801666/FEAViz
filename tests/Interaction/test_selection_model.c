#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static FVizActor* make_actor(void)
{
    FVizActor* actor = NULL;
    FVizPolyData* data = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK || fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(0,0,0), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(1,0,0), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(0,1,0), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data, 0u,1u,2u) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, data) != FVIZ_OK)
    { fviz_release(data); fviz_release(actor); return NULL; }
    fviz_release(data);
    return actor;
}

int main(void)
{
    FVizSelectionModel* model = NULL;
    FVizSelection* incoming = NULL;
    FVizActor* actor = make_actor();
    FVizInteractionEvent event = {0};
    FVizMTime before;
    CHECK(actor != NULL);
    CHECK(fviz_selection_model_create(&model) == FVIZ_OK);
    CHECK(fviz_selection_model_association(model) == FVIZ_SELECTION_CELL);
    CHECK(fviz_selection_model_modifier(model) == FVIZ_SELECTION_REPLACE);
    CHECK(fviz_selection_model_hover_update_rate(model) == 30.0f);
    fviz_selection_model_set_hover_update_rate(model, 60.0f);
    CHECK(fviz_selection_model_hover_update_rate(model) == 60.0f);
    before = fviz_object_mtime((FVizObject*)model);
    fviz_selection_model_set_association(model, FVIZ_SELECTION_EDGE);
    CHECK(fviz_selection_model_association(model) == FVIZ_SELECTION_EDGE);
    CHECK(fviz_object_mtime((FVizObject*)model) > before);

    CHECK(fviz_selection_create(&incoming) == FVIZ_OK);
    CHECK(fviz_selection_add(incoming, actor, FVIZ_SELECTION_CELL, 0u) == FVIZ_OK);
    CHECK(fviz_selection_model_apply(model, incoming, FVIZ_SELECTION_REPLACE) == FVIZ_OK);
    CHECK(fviz_selection_count(fviz_selection_model_selection(model)) == 1u);
    CHECK(fviz_selection_model_apply(model, incoming, FVIZ_SELECTION_TOGGLE) == FVIZ_OK);
    CHECK(fviz_selection_count(fviz_selection_model_selection(model)) == 0u);

    event.shift = FVIZ_TRUE;
    CHECK(fviz_selection_modifier_from_event(&event) == FVIZ_SELECTION_ADD);
    event.control = FVIZ_TRUE;
    CHECK(fviz_selection_modifier_from_event(&event) == FVIZ_SELECTION_TOGGLE);
    event.alt = FVIZ_TRUE;
    CHECK(fviz_selection_modifier_from_event(&event) == FVIZ_SELECTION_SUBTRACT);
    event.alt = event.control = event.shift = FVIZ_FALSE;
    CHECK(fviz_selection_modifier_from_event(&event) == FVIZ_SELECTION_REPLACE);

    fviz_selection_model_clear(model);
    fviz_selection_model_clear_hover(model);
    fviz_release(incoming);
    fviz_release(model);
    fviz_release(actor);
    return 0;
}
