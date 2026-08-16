#include <math.h>
#include <string.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int nearf32(float a, float b) { return fabsf(a-b) <= 1.0e-4f; }

int main(void)
{
    FVizFont* font = NULL;
    FVizFontAtlas* atlas;
    FVizFontAtlas* custom_atlas = NULL;
    FVizFont* custom_font = NULL;
    FVizTextProperty* property = NULL;
    FVizTextActor2D* text2d = NULL;
    FVizBillboardTextActor3D* text3d = NULL;
    FVizLabelSet3D* labels = NULL;
    FVizRenderer* renderer = NULL;
    FVizScalarLegend* legend = NULL;
    FVizTextMetrics metrics;
    const FVizFontGlyph* glyph;
    float x = 0.0f, y = 0.0f;

    CHECK(fviz_font_create_builtin(&font) == FVIZ_OK);
    CHECK(strcmp(fviz_font_family(font), "FEAViz Builtin Mono") == 0);
    atlas = fviz_font_atlas(font);
    CHECK(atlas != NULL);
    CHECK(fviz_font_atlas_width(atlas) == 128u);
    CHECK(fviz_font_atlas_height(atlas) == 48u);
    CHECK(fviz_font_atlas_pixels(atlas) != NULL);
    CHECK(fviz_font_atlas_glyph_count(atlas) == 95u);
    glyph = fviz_font_atlas_find_glyph(atlas, 'A');
    CHECK(glyph != NULL && glyph->advance_x > 0.0f);
    CHECK(fviz_font_atlas_find_glyph(atlas, 0x20ACu) != NULL); /* fallback '?' */

    {
        const uint8_t pixels[16] = {255u,0u,0u,0u, 0u,255u,0u,0u, 0u,0u,255u,0u, 0u,0u,0u,255u};
        const FVizFontGlyph custom_glyphs[2] = {
            {0x03A9u, 2u, 0u, 2u, 2u, 2.5f, 0.0f, 2.0f},
            {'?', 0u, 0u, 2u, 2u, 2.0f, 0.0f, 2.0f}
        };
        CHECK(fviz_font_atlas_create_from_coverage(4u, 4u, pixels, custom_glyphs, 2u, 2.0f, '?', &custom_atlas) == FVIZ_OK);
        CHECK(fviz_font_atlas_glyph_count(custom_atlas) == 2u);
        CHECK(fviz_font_atlas_find_glyph(custom_atlas, 0x03A9u)->codepoint == 0x03A9u);
        CHECK(fviz_font_atlas_find_glyph(custom_atlas, 0x2603u)->codepoint == '?');
        CHECK(fviz_font_create_from_atlas("Test Unicode Atlas", custom_atlas, &custom_font) == FVIZ_OK);
        CHECK(strcmp(fviz_font_family(custom_font), "Test Unicode Atlas") == 0);
    }

    CHECK(fviz_text_property_create(&property) == FVIZ_OK);
    CHECK(fviz_text_property_set_font(property, font) == FVIZ_OK);
    fviz_text_property_set_font_size(property, 16.0f);
    fviz_text_property_set_line_spacing(property, 1.25f);
    fviz_text_property_set_horizontal_alignment(property, FVIZ_TEXT_ALIGN_CENTER);
    fviz_text_property_set_vertical_alignment(property, FVIZ_TEXT_ALIGN_MIDDLE);
    fviz_text_property_set_shadow(property, FVIZ_TRUE, 1.0f, -1.0f, 0.4f);
    CHECK(fviz_text_measure_utf8(property, "Stress\nS11", &metrics) == FVIZ_OK);
    CHECK(metrics.glyph_count == 9u);
    CHECK(metrics.line_count == 2u);
    CHECK(nearf32(metrics.line_height, 20.0f));
    CHECK(metrics.width > 0.0f && nearf32(metrics.height, 40.0f));
    CHECK(fviz_text_measure_utf8(property, "UTF8: \xE2\x82\xAC", &metrics) == FVIZ_OK);
    CHECK(metrics.glyph_count > 0u);

    CHECK(fviz_text_actor_2d_create(&text2d) == FVIZ_OK);
    CHECK(fviz_text_actor_2d_set_text_property(text2d, property) == FVIZ_OK);
    CHECK(fviz_text_actor_2d_set_text(text2d, "Node 42") == FVIZ_OK);
    fviz_text_actor_2d_set_position(text2d, 0.25f, 0.75f);
    fviz_text_actor_2d_set_coordinate_system(text2d, FVIZ_TEXT_COORDINATE_NORMALIZED_VIEWPORT);
    fviz_text_actor_2d_get_position(text2d, &x, &y);
    CHECK(nearf32(x, 0.25f) && nearf32(y, 0.75f));
    CHECK(fviz_text_actor_2d_measure(text2d, &metrics) == FVIZ_OK && metrics.glyph_count == 7u);

    CHECK(fviz_billboard_text_actor_3d_create(&text3d) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_set_text_property(text3d, property) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_set_text(text3d, "Element 1001") == FVIZ_OK);
    fviz_billboard_text_actor_3d_set_world_position(text3d, fviz_vec3(1.0f, 2.0f, 3.0f));
    fviz_billboard_text_actor_3d_set_pixel_offset(text3d, 4.0f, 5.0f);
    CHECK(fviz_billboard_text_actor_3d_depth_test(text3d) == FVIZ_TRUE);

    CHECK(fviz_label_set_3d_create(&labels) == FVIZ_OK);
    CHECK(fviz_label_set_3d_reserve(labels, 128u) == FVIZ_OK);
    CHECK(fviz_label_set_3d_add(labels, fviz_vec3(0.0f, 0.0f, 0.0f), "N1", NULL) == FVIZ_OK);
    CHECK(fviz_label_set_3d_add(labels, fviz_vec3(1.0f, 0.0f, 0.0f), "N2", NULL) == FVIZ_OK);
    CHECK(fviz_label_set_3d_count(labels) == 2u);
    CHECK(strcmp(fviz_label_set_3d_text_at(labels, 1u), "N2") == 0);
    CHECK(nearf32(fviz_label_set_3d_position_at(labels, 1u).x, 1.0f));
    CHECK(fviz_label_set_3d_set_text(labels, 1u, "Node 2") == FVIZ_OK);
    CHECK(fviz_label_set_3d_set_position(labels, 1u, fviz_vec3(2.0f, 0.0f, 0.0f)) == FVIZ_OK);
    fviz_label_set_3d_set_pixel_offset(labels, 3.0f, 4.0f);
    fviz_label_set_3d_get_pixel_offset(labels, &x, &y);
    CHECK(nearf32(x, 3.0f) && nearf32(y, 4.0f));
    CHECK(fviz_label_set_3d_const_text_property(labels) != NULL);

    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_renderer_add_text_actor_2d(renderer, text2d) == FVIZ_OK);
    CHECK(fviz_renderer_add_text_actor_2d(renderer, text2d) == FVIZ_OK); /* no duplicate */
    CHECK(fviz_renderer_text_actor_2d_count(renderer) == 1u);
    CHECK(fviz_renderer_add_billboard_text_actor_3d(renderer, text3d) == FVIZ_OK);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 1u);
    CHECK(fviz_renderer_text_actor_2d_at(renderer, 0u) == text2d);
    CHECK(fviz_renderer_billboard_text_actor_3d_at(renderer, 0u) == text3d);

    CHECK(fviz_renderer_add_label_set_3d(renderer, labels) == FVIZ_OK);
    CHECK(fviz_renderer_add_label_set_3d(renderer, labels) == FVIZ_OK);
    CHECK(fviz_renderer_label_set_3d_count(renderer) == 1u);
    CHECK(fviz_renderer_label_set_3d_at(renderer, 0u) == labels);

    CHECK(fviz_scalar_legend_create(&legend) == FVIZ_OK);
    fviz_scalar_legend_set_title(legend, "Von Mises");
    fviz_scalar_legend_set_units(legend, "MPa");
    fviz_scalar_legend_set_tick_count(legend, 7u);
    CHECK(fviz_scalar_legend_tick_count(legend) == 7u);
    CHECK(strcmp(fviz_scalar_legend_units(legend), "MPa") == 0);
    CHECK(fviz_scalar_legend_set_label_format(legend, "%+.3e") == FVIZ_OK);
    CHECK(strcmp(fviz_scalar_legend_label_format(legend), "%+.3e") == 0);
    CHECK(fviz_scalar_legend_set_label_format(legend, "%s") == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_scalar_legend_title_text_property(legend) != NULL);
    CHECK(fviz_scalar_legend_label_text_property(legend) != NULL);

    CHECK(fviz_renderer_remove_text_actor_2d(renderer, text2d) == FVIZ_OK);
    CHECK(fviz_renderer_remove_billboard_text_actor_3d(renderer, text3d) == FVIZ_OK);
    CHECK(fviz_renderer_text_actor_2d_count(renderer) == 0u);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 0u);

    CHECK(fviz_renderer_remove_label_set_3d(renderer, labels) == FVIZ_OK);
    CHECK(fviz_renderer_label_set_3d_count(renderer) == 0u);

    fviz_release(legend);
    fviz_release(labels);
    fviz_release(renderer);
    fviz_release(text3d);
    fviz_release(text2d);
    fviz_release(property);
    fviz_release(custom_font);
    fviz_release(custom_atlas);
    fviz_release(font);
    return 0;
}
