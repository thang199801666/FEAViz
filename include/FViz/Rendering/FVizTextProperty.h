#ifndef FVIZ_RENDERING_TEXT_PROPERTY_H
#define FVIZ_RENDERING_TEXT_PROPERTY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizFont.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTextProperty FVizTextProperty;
#define FVIZ_TYPE_TEXT_PROPERTY UINT64_C(0xE42391AD7B50A103)

typedef enum FVizTextHorizontalAlignment
{
    FVIZ_TEXT_ALIGN_LEFT = 0,
    FVIZ_TEXT_ALIGN_CENTER = 1,
    FVIZ_TEXT_ALIGN_RIGHT = 2
} FVizTextHorizontalAlignment;

typedef enum FVizTextVerticalAlignment
{
    FVIZ_TEXT_ALIGN_BOTTOM = 0,
    FVIZ_TEXT_ALIGN_MIDDLE = 1,
    FVIZ_TEXT_ALIGN_TOP = 2
} FVizTextVerticalAlignment;

FVIZ_API FVizResult fviz_text_property_create(FVizTextProperty** out_property);
FVIZ_API FVizResult fviz_text_property_set_font(FVizTextProperty* property, FVizFont* font);
FVIZ_API FVizFont* fviz_text_property_font(FVizTextProperty* property);
FVIZ_API const FVizFont* fviz_text_property_const_font(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_set_font_size(FVizTextProperty* property, float logical_pixels);
FVIZ_API float fviz_text_property_font_size(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_set_color(FVizTextProperty* property, float red, float green, float blue, float alpha);
FVIZ_API void fviz_text_property_get_color(const FVizTextProperty* property, float* red, float* green, float* blue,
                                           float* alpha);
FVIZ_API void fviz_text_property_set_background(FVizTextProperty* property, float red, float green, float blue,
                                                float alpha);
FVIZ_API void fviz_text_property_get_background(const FVizTextProperty* property, float* red, float* green, float* blue,
                                                float* alpha);
FVIZ_API void fviz_text_property_set_horizontal_alignment(FVizTextProperty* property,
                                                          FVizTextHorizontalAlignment alignment);
FVIZ_API FVizTextHorizontalAlignment fviz_text_property_horizontal_alignment(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_set_vertical_alignment(FVizTextProperty* property,
                                                        FVizTextVerticalAlignment alignment);
FVIZ_API FVizTextVerticalAlignment fviz_text_property_vertical_alignment(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_set_line_spacing(FVizTextProperty* property, float factor);
FVIZ_API float fviz_text_property_line_spacing(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_set_shadow(FVizTextProperty* property, FVizBool enabled, float offset_x,
                                            float offset_y, float opacity);
FVIZ_API FVizBool fviz_text_property_shadow(const FVizTextProperty* property);
FVIZ_API void fviz_text_property_get_shadow(const FVizTextProperty* property, float* offset_x, float* offset_y,
                                            float* opacity);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_TEXT_PROPERTY_H */
