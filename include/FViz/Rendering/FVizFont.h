#ifndef FVIZ_RENDERING_FONT_H
#define FVIZ_RENDERING_FONT_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Rendering/FVizFontAtlas.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFont FVizFont;
#define FVIZ_TYPE_FONT UINT64_C(0xE42391AD7B50A102)

FVIZ_API FVizResult fviz_font_create_builtin(FVizFont** out_font);
FVIZ_API FVizResult fviz_font_create_system(const char* family, float pixel_size, FVizFont** out_font);
FVIZ_API FVizResult fviz_font_create_from_file(const char* file_path, const char* family, float pixel_size,
                                               FVizFont** out_font);
FVIZ_API void fviz_font_cache_clear(void);
FVIZ_API FVizResult fviz_font_create_from_atlas(const char* family, FVizFontAtlas* atlas, FVizFont** out_font);
FVIZ_API const char* fviz_font_family(const FVizFont* font);
FVIZ_API FVizFontAtlas* fviz_font_atlas(FVizFont* font);
FVIZ_API const FVizFontAtlas* fviz_font_const_atlas(const FVizFont* font);
FVIZ_API float fviz_font_nominal_pixel_size(const FVizFont* font);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_FONT_H */
