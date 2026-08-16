#ifndef FVIZ_RENDERING_LABEL_SET_H
#define FVIZ_RENDERING_LABEL_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizTextProperty.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizLabelSet3D FVizLabelSet3D;
#define FVIZ_TYPE_LABEL_SET_3D UINT64_C(0xE42391AD7B50A106)

FVIZ_API FVizResult fviz_label_set_3d_create(FVizLabelSet3D** out_label_set);
FVIZ_API void fviz_label_set_3d_clear(FVizLabelSet3D* label_set);
FVIZ_API FVizResult fviz_label_set_3d_reserve(FVizLabelSet3D* label_set, FVizSize capacity);
FVIZ_API FVizResult fviz_label_set_3d_add(FVizLabelSet3D* label_set, FVizVec3 position, const char* utf8,
                                          FVizSize* out_index);
FVIZ_API FVizSize fviz_label_set_3d_count(const FVizLabelSet3D* label_set);
FVIZ_API FVizVec3 fviz_label_set_3d_position_at(const FVizLabelSet3D* label_set, FVizSize index);
FVIZ_API const char* fviz_label_set_3d_text_at(const FVizLabelSet3D* label_set, FVizSize index);
FVIZ_API FVizResult fviz_label_set_3d_set_position(FVizLabelSet3D* label_set, FVizSize index, FVizVec3 position);
FVIZ_API FVizResult fviz_label_set_3d_set_text(FVizLabelSet3D* label_set, FVizSize index, const char* utf8);
FVIZ_API FVizTextProperty* fviz_label_set_3d_text_property(FVizLabelSet3D* label_set);
FVIZ_API const FVizTextProperty* fviz_label_set_3d_const_text_property(const FVizLabelSet3D* label_set);
FVIZ_API void fviz_label_set_3d_set_visible(FVizLabelSet3D* label_set, FVizBool visible);
FVIZ_API FVizBool fviz_label_set_3d_visible(const FVizLabelSet3D* label_set);
FVIZ_API void fviz_label_set_3d_set_depth_test(FVizLabelSet3D* label_set, FVizBool enabled);
FVIZ_API FVizBool fviz_label_set_3d_depth_test(const FVizLabelSet3D* label_set);
FVIZ_API void fviz_label_set_3d_set_pixel_offset(FVizLabelSet3D* label_set, float x, float y);
FVIZ_API void fviz_label_set_3d_get_pixel_offset(const FVizLabelSet3D* label_set, float* x, float* y);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_LABEL_SET_H */
