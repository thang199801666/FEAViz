#include <stdio.h>
#include <stdlib.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static FVizActor* make_actor(void)
{
    FVizActor* actor = NULL;
    FVizPolyData* data = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK || fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) != FVIZ_OK ||
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

int main(void)
{
    FVizActor* a = make_actor();
    FVizActor* b = make_actor();
    FVizSelection* selection = NULL;
    FVizSelection* incoming = NULL;
    FVizSelection* copy = NULL;
    FVizGlyphMapper* glyphs = NULL;
    FVizGlyphInstance instance;
    FVizSelectionRecord record;
    FVizDataArray* mask = NULL;
    FVizSelection* converted = NULL;
    FVizNamedSelectionCollection* named = NULL;
    FVizPolyData* extracted = NULL;

    CHECK(a != NULL && b != NULL);
    CHECK(fviz_selection_create(&selection) == FVIZ_OK);
    CHECK(fviz_selection_create(&incoming) == FVIZ_OK);

    CHECK(fviz_selection_add(selection, a, FVIZ_SELECTION_CELL, 0u) == FVIZ_OK);
    CHECK(fviz_selection_add(selection, a, FVIZ_SELECTION_CELL, 0u) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_contains(selection, a, FVIZ_SELECTION_CELL, 0u) == FVIZ_TRUE);

    CHECK(fviz_selection_add(incoming, a, FVIZ_SELECTION_POINT, 1u) == FVIZ_OK);
    CHECK(fviz_selection_add(incoming, b, FVIZ_SELECTION_EDGE, 7u) == FVIZ_OK);
    CHECK(fviz_selection_apply(selection, incoming, FVIZ_SELECTION_ADD) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 3u);
    CHECK(fviz_selection_apply(selection, incoming, FVIZ_SELECTION_TOGGLE) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_apply(selection, incoming, FVIZ_SELECTION_ADD) == FVIZ_OK);
    CHECK(fviz_selection_apply(selection, incoming, FVIZ_SELECTION_SUBTRACT) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_apply(selection, incoming, FVIZ_SELECTION_REPLACE) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 2u);
    CHECK(fviz_selection_copy(selection, &copy) == FVIZ_OK);
    CHECK(fviz_selection_count(copy) == 2u);
    CHECK(fviz_selection_remove(copy, b, FVIZ_SELECTION_EDGE, 7u) == FVIZ_OK);
    CHECK(fviz_selection_count(copy) == 1u);
    CHECK(fviz_selection_remove(copy, b, FVIZ_SELECTION_EDGE, 7u) == FVIZ_ERROR_NOT_FOUND);

    fviz_selection_clear(selection);
    CHECK(fviz_selection_add(selection, a, FVIZ_SELECTION_POINT, 1u) == FVIZ_OK);
    CHECK(fviz_selection_create_mask(selection, a, FVIZ_SELECTION_POINT,
        3u, FVIZ_FALSE, &mask) == FVIZ_OK);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[0] == 0u);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[1] == 1u);
    fviz_release(mask);
    mask = NULL;
    CHECK(fviz_selection_create_mask(selection, a, FVIZ_SELECTION_POINT,
        3u, FVIZ_TRUE, &mask) == FVIZ_OK);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[0] == 1u);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[1] == 0u);
    CHECK(fviz_selection_convert_association(selection, a,
        FVIZ_SELECTION_CELL, &converted) == FVIZ_OK);
    CHECK(fviz_selection_count(converted) == 1u);
    CHECK(fviz_selection_contains(converted, a, FVIZ_SELECTION_CELL, 0u) == FVIZ_TRUE);
    fviz_release(converted);
    converted = NULL;
    CHECK(fviz_named_selection_collection_create(&named) == FVIZ_OK);
    CHECK(fviz_named_selection_collection_set(named, "hotspot", selection) == FVIZ_OK);
    CHECK(fviz_named_selection_collection_count(named) == 1u);
    CHECK(fviz_named_selection_collection_get(named, "hotspot") == selection);
    CHECK(fviz_named_selection_collection_set(named, "hotspot", copy) == FVIZ_OK);
    CHECK(fviz_named_selection_collection_get(named, "hotspot") == copy);
    CHECK(fviz_named_selection_collection_remove(named, "hotspot") == FVIZ_OK);
    CHECK(fviz_named_selection_collection_count(named) == 0u);
    fviz_selection_clear(selection);
    CHECK(fviz_selection_add(selection, a, FVIZ_SELECTION_CELL, 0u) == FVIZ_OK);
    CHECK(fviz_selection_extract_poly_data(selection, a,
        FVIZ_SELECTION_CELL, &extracted) == FVIZ_OK);
    CHECK(fviz_poly_data_point_count(extracted) == 3u);
    CHECK(fviz_poly_data_triangle_count(extracted) == 1u);
    fviz_release(extracted); extracted = NULL;
    fviz_release(mask);
    mask = NULL;

    CHECK(fviz_glyph_mapper_create(&glyphs) == FVIZ_OK);
    fviz_glyph_instance_initialize(&instance);
    instance.position = fviz_vec3(2.0f, 3.0f, 4.0f);
    CHECK(fviz_glyph_mapper_add_instance(glyphs, &instance) == FVIZ_OK);
    CHECK(fviz_actor_set_glyph_mapper(a, glyphs) == FVIZ_OK);
    fviz_actor_set_position(a, fviz_vec3(10.0f, 0.0f, 0.0f));
    fviz_selection_clear(selection);
    CHECK(fviz_selection_add(selection, a, FVIZ_SELECTION_GLYPH_INSTANCE, 0u) == FVIZ_OK);
    CHECK(fviz_selection_probe(selection, 0u, "ignored") == FVIZ_OK);
    CHECK(fviz_selection_get_record(selection, 0u, &record) == FVIZ_OK);
    CHECK(record.has_world_position == FVIZ_TRUE);
    CHECK(record.world_position.x > 11.999f && record.world_position.x < 12.001f);
    CHECK(record.world_position.y > 2.999f && record.world_position.y < 3.001f);
    CHECK(record.world_position.z > 3.999f && record.world_position.z < 4.001f);

    /* Self-application must be safe. */
    CHECK(fviz_selection_apply(selection, selection, FVIZ_SELECTION_TOGGLE) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 0u);

    fviz_release(glyphs);
    fviz_release(named);
    fviz_release(extracted);
    fviz_release(converted);
    fviz_release(mask);
    fviz_release(copy);
    fviz_release(incoming);
    fviz_release(selection);
    fviz_release(b);
    fviz_release(a);
    return 0;
}
