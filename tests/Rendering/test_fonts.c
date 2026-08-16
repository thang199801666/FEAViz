#include <stdio.h>
#include <string.h>

#include <FViz/Rendering/FVizFont.h>
#include <FViz/Rendering/FVizFontAtlas.h>
#include <FViz/Core/FVizMemory.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    FVizFontAtlas* atlas = NULL;
    FVizFont* font = NULL;
    CHECK(fviz_font_default_family_count() == 4u);
    CHECK(strcmp(fviz_font_default_family(0u), "Arial") == 0);
    CHECK(strcmp(fviz_font_default_family(1u), "Times New Roman") == 0);
    CHECK(strcmp(fviz_font_default_family(2u), "Segoe UI") == 0);
    CHECK(strcmp(fviz_font_default_family(3u), "Consolas") == 0);
    CHECK(fviz_font_default_family(4u) == NULL);
    CHECK(fviz_font_atlas_create_builtin(&atlas) == FVIZ_OK);
    CHECK(fviz_font_atlas_glyph_count(atlas) > 0u);
    fviz_release(atlas);
    atlas = NULL;
    if (fviz_font_atlas_create_system("Arial", 14.0f, &atlas) == FVIZ_OK)
    {
        CHECK(fviz_font_atlas_glyph_count(atlas) >= 90u);
        CHECK(fviz_font_atlas_find_glyph(atlas, (uint32_t)'A') != NULL);
        fviz_release(atlas);
        atlas = NULL;
        CHECK(fviz_font_create_system("Consolas", 14.0f, &font) == FVIZ_OK);
        CHECK(strcmp(fviz_font_family(font), "Consolas") == 0);
        fviz_release(font);
    }
    return 0;
}
