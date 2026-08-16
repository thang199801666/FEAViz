#ifndef FVIZ_RENDERING_ACTOR_H
#define FVIZ_RENDERING_ACTOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizTransform.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizVolumeMapper.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizActor FVizActor;
typedef struct FVizGlyphMapper FVizGlyphMapper;
typedef struct FVizVolumeMapper FVizVolumeMapper;

typedef enum FVizShadingMode
{
    FVIZ_SHADING_SMOOTH = 0,
    FVIZ_SHADING_FLAT = 1
} FVizShadingMode;

typedef enum FVizCullMode
{
    FVIZ_CULL_NONE = 0,
    FVIZ_CULL_BACK = 1,
    FVIZ_CULL_FRONT = 2
} FVizCullMode;

typedef enum FVizLineCap
{
    FVIZ_LINE_CAP_BUTT = 0,
    FVIZ_LINE_CAP_SQUARE = 1,
    FVIZ_LINE_CAP_ROUND = 2
} FVizLineCap;

typedef enum FVizLineJoin
{
    FVIZ_LINE_JOIN_MITER = 0,
    FVIZ_LINE_JOIN_BEVEL = 1,
    FVIZ_LINE_JOIN_ROUND = 2
} FVizLineJoin;

typedef enum FVizPointShape
{
    FVIZ_POINT_SQUARE = 0,
    FVIZ_POINT_CIRCLE = 1,
    FVIZ_POINT_SPHERE_IMPOSTOR = 2
} FVizPointShape;

#define FVIZ_TYPE_ACTOR UINT64_C(0x0A99A2B9935E483C)

FVIZ_RENDERING_API FVizResult fviz_actor_create(FVizActor** out_actor);
FVIZ_RENDERING_API FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data);
FVIZ_RENDERING_API FVizPolyData* fviz_actor_poly_data(FVizActor* actor);
FVIZ_RENDERING_API const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor);
FVIZ_RENDERING_API FVizResult fviz_actor_set_mapper(FVizActor* actor, FVizMapper* mapper);
FVIZ_RENDERING_API FVizMapper* fviz_actor_mapper(FVizActor* actor);
FVIZ_RENDERING_API FVizResult fviz_actor_set_glyph_mapper(FVizActor* actor, FVizGlyphMapper* mapper);
FVIZ_RENDERING_API FVizGlyphMapper* fviz_actor_glyph_mapper(FVizActor* actor);
FVIZ_RENDERING_API const FVizGlyphMapper* fviz_actor_const_glyph_mapper(const FVizActor* actor);
FVIZ_RENDERING_API FVizResult fviz_actor_set_volume_mapper(FVizActor* actor, FVizVolumeMapper* mapper);
FVIZ_RENDERING_API FVizVolumeMapper* fviz_actor_volume_mapper(FVizActor* actor);
FVIZ_RENDERING_API const FVizVolumeMapper* fviz_actor_const_volume_mapper(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_color(FVizActor* actor, float red, float green, float blue);
FVIZ_RENDERING_API void fviz_actor_get_color(const FVizActor* actor, float* red, float* green, float* blue);
FVIZ_RENDERING_API void fviz_actor_set_visible(FVizActor* actor, FVizBool visible);
FVIZ_RENDERING_API FVizBool fviz_actor_is_visible(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_pickable(FVizActor* actor, FVizBool pickable);
FVIZ_RENDERING_API FVizBool fviz_actor_pickable(const FVizActor* actor);
FVIZ_RENDERING_API FVizBounds fviz_actor_bounds(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_wireframe(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_wireframe(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_opacity(FVizActor* actor, float opacity);
FVIZ_RENDERING_API float fviz_actor_opacity(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_edge_visibility(FVizActor* actor, FVizBool visible);
FVIZ_RENDERING_API FVizBool fviz_actor_edge_visibility(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_edge_color(FVizActor* actor, float red, float green, float blue);
FVIZ_RENDERING_API void fviz_actor_get_edge_color(const FVizActor* actor, float* red, float* green, float* blue);
FVIZ_RENDERING_API void fviz_actor_set_line_width(FVizActor* actor, float width);
FVIZ_RENDERING_API float fviz_actor_line_width(const FVizActor* actor);
/* Pulls line fragments toward the camera in normalized clip-space to resolve
 * coincident topology without disabling depth testing. Range: 0..0.01. */
FVIZ_RENDERING_API void fviz_actor_set_line_depth_bias(FVizActor* actor, float bias);
FVIZ_RENDERING_API float fviz_actor_line_depth_bias(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_line_cap(FVizActor* actor, FVizLineCap cap);
FVIZ_RENDERING_API FVizLineCap fviz_actor_line_cap(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_line_join(FVizActor* actor, FVizLineJoin join);
FVIZ_RENDERING_API FVizLineJoin fviz_actor_line_join(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_line_miter_limit(FVizActor* actor, float limit);
FVIZ_RENDERING_API float fviz_actor_line_miter_limit(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_line_dash(FVizActor* actor, float dash_length, float gap_length, float phase);
FVIZ_RENDERING_API void fviz_actor_get_line_dash(const FVizActor* actor, float* dash_length, float* gap_length, float* phase);
FVIZ_RENDERING_API void fviz_actor_set_line_scalar_coloring(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_line_scalar_coloring(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_point_visibility(FVizActor* actor, FVizBool visible);
FVIZ_RENDERING_API FVizBool fviz_actor_point_visibility(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_point_size(FVizActor* actor, float size_pixels);
FVIZ_RENDERING_API float fviz_actor_point_size(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_point_shape(FVizActor* actor, FVizPointShape shape);
FVIZ_RENDERING_API FVizPointShape fviz_actor_point_shape(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_point_color(FVizActor* actor, float red, float green, float blue);
FVIZ_RENDERING_API void fviz_actor_get_point_color(const FVizActor* actor, float* red, float* green, float* blue);
FVIZ_RENDERING_API void fviz_actor_set_point_scalar_coloring(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_point_scalar_coloring(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_material(FVizActor* actor, float ambient, float diffuse, float specular,
                                      float specular_power);
FVIZ_RENDERING_API void fviz_actor_get_material(const FVizActor* actor, float* ambient, float* diffuse, float* specular,
                                      float* specular_power);
FVIZ_RENDERING_API void fviz_actor_set_shading_mode(FVizActor* actor, FVizShadingMode mode);
FVIZ_RENDERING_API FVizShadingMode fviz_actor_shading_mode(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_cull_mode(FVizActor* actor, FVizCullMode mode);
FVIZ_RENDERING_API FVizCullMode fviz_actor_cull_mode(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_position(FVizActor* actor, FVizVec3 position);
FVIZ_RENDERING_API FVizVec3 fviz_actor_position(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_orientation(FVizActor* actor, FVizQuat orientation);
FVIZ_RENDERING_API FVizQuat fviz_actor_orientation(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_scale(FVizActor* actor, FVizVec3 scale);
FVIZ_RENDERING_API FVizVec3 fviz_actor_scale(const FVizActor* actor);
FVIZ_RENDERING_API FVizMat4 fviz_actor_transform_matrix(const FVizActor* actor);
FVIZ_RENDERING_API FVizResult fviz_actor_set_user_transform(FVizActor* actor, FVizTransform* transform);
FVIZ_RENDERING_API FVizTransform* fviz_actor_user_transform(FVizActor* actor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_ACTOR_H */
