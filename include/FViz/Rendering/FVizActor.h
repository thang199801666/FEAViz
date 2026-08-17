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
#include <FViz/Rendering/FVizRenderPass.h>
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

/* How coincident topology (e.g. mesh edges drawn over a filled surface) is
 * separated in depth to avoid z-fighting. */
typedef enum FVizCoincidentTopologyMode
{
    FVIZ_COINCIDENT_TOPOLOGY_OFF = 0,
    FVIZ_COINCIDENT_TOPOLOGY_POLYGON_OFFSET = 1,
    FVIZ_COINCIDENT_TOPOLOGY_SHIFT_Z_BUFFER = 2,
    FVIZ_COINCIDENT_TOPOLOGY_DEFAULT = 3
} FVizCoincidentTopologyMode;

/* OpenGL depth comparison functions. */
typedef enum FVizDepthFunction
{
    FVIZ_DEPTH_FUNCTION_NEVER = 0,
    FVIZ_DEPTH_FUNCTION_LESS = 1,
    FVIZ_DEPTH_FUNCTION_EQUAL = 2,
    FVIZ_DEPTH_FUNCTION_LEQUAL = 3,
    FVIZ_DEPTH_FUNCTION_GREATER = 4,
    FVIZ_DEPTH_FUNCTION_NOTEQUAL = 5,
    FVIZ_DEPTH_FUNCTION_GEQUAL = 6,
    FVIZ_DEPTH_FUNCTION_ALWAYS = 7
} FVizDepthFunction;

/* Overlay topology display modes: which secondary representations are drawn
 * on top of the filled surface. */
typedef enum FVizOverlayTopologyMode
{
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_ONLY = 0,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_EDGES = 1,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_WIREFRAME = 2,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_POINTS = 3,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_CONTOUR = 4,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_LABELS = 5,
    FVIZ_OVERLAY_TOPOLOGY_SURFACE_ANNOTATIONS = 6
} FVizOverlayTopologyMode;

/* Bit flags selecting which topology data is published on the actor. */
typedef enum FVizTopologyDataFlag
{
    FVIZ_TOPOLOGY_DATA_NONE = 0,
    FVIZ_TOPOLOGY_DATA_CONNECTIVITY = 1 << 0,
    FVIZ_TOPOLOGY_DATA_ADJACENCY = 1 << 1,
    FVIZ_TOPOLOGY_DATA_CELL_CLASSIFICATION = 1 << 2,
    FVIZ_TOPOLOGY_DATA_POINT_CELL_IDS = 1 << 3,
    FVIZ_TOPOLOGY_DATA_GLOBAL_IDS = 1 << 4,
    FVIZ_TOPOLOGY_DATA_PEDIGREE_IDS = 1 << 5,
    FVIZ_TOPOLOGY_DATA_EDGE_FLAGS = 1 << 6
} FVizTopologyDataFlag;

/* Per-actor topology / rendering flags. The struct is passed by value to the
 * setter and mirrors VTK's actor coincident-topology and depth controls. */
typedef struct FVizTopologyRenderOptions
{
    uint32_t struct_size;

    /* Coincident topology resolution. */
    FVizCoincidentTopologyMode coincident_mode;
    FVizBool offset_faces;

    /* GL_POLYGON_OFFSET parameters for surfaces. */
    float polygon_offset_factor;
    float polygon_offset_units;

    /* GL_POLYGON_OFFSET parameters for lines. */
    float line_offset_factor;
    float line_offset_units;

    /* Point offset (fraction of one depth unit). */
    float point_offset_units;

    /* Depth-buffer Z shift applied after the projection transform (VTK-style
     * "ShiftZBuffer"): positive values pull the actor toward the camera. */
    float z_shift;

    /* Depth buffer controls. */
    FVizBool depth_test;
    FVizBool depth_write;
    FVizDepthFunction depth_function;
    float depth_range_minimum;
    float depth_range_maximum;

    /* Render ordering. */
    int32_t render_layer;
    int32_t render_priority;
    FVizRenderPassStage pass_order;

    /* Overlay topology mode. */
    FVizOverlayTopologyMode overlay_mode;

    /* Topology data published on the actor. */
    uint32_t topology_data_flags;
} FVizTopologyRenderOptions;

FVIZ_API void fviz_topology_render_options_initialize(FVizTopologyRenderOptions* options);

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

/* ---- Topology / rendering flags ----------------------------------------- */
FVIZ_RENDERING_API void fviz_actor_set_topology_render_options(FVizActor* actor, const FVizTopologyRenderOptions* options);
FVIZ_RENDERING_API void fviz_actor_topology_render_options(const FVizActor* actor, FVizTopologyRenderOptions* out_options);
FVIZ_RENDERING_API void fviz_actor_set_coincident_topology_mode(FVizActor* actor, FVizCoincidentTopologyMode mode);
FVIZ_RENDERING_API FVizCoincidentTopologyMode fviz_actor_coincident_topology_mode(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_polygon_offset(FVizActor* actor, float factor, float units);
FVIZ_RENDERING_API void fviz_actor_polygon_offset(const FVizActor* actor, float* factor, float* units);
FVIZ_RENDERING_API void fviz_actor_set_line_offset(FVizActor* actor, float factor, float units);
FVIZ_RENDERING_API void fviz_actor_line_offset(const FVizActor* actor, float* factor, float* units);
FVIZ_RENDERING_API void fviz_actor_set_point_offset(FVizActor* actor, float units);
FVIZ_RENDERING_API float fviz_actor_point_offset(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_offset_faces(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_offset_faces(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_z_shift(FVizActor* actor, float z_shift);
FVIZ_RENDERING_API float fviz_actor_z_shift(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_depth_test(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_depth_test(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_depth_write(FVizActor* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_actor_depth_write(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_depth_function(FVizActor* actor, FVizDepthFunction function);
FVIZ_RENDERING_API FVizDepthFunction fviz_actor_depth_function(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_depth_range(FVizActor* actor, float minimum, float maximum);
FVIZ_RENDERING_API void fviz_actor_get_depth_range(const FVizActor* actor, float* minimum, float* maximum);
FVIZ_RENDERING_API void fviz_actor_set_render_layer(FVizActor* actor, int32_t layer);
FVIZ_RENDERING_API int32_t fviz_actor_render_layer(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_render_priority(FVizActor* actor, int32_t priority);
FVIZ_RENDERING_API int32_t fviz_actor_render_priority(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_pass_order(FVizActor* actor, FVizRenderPassStage pass_order);
FVIZ_RENDERING_API FVizRenderPassStage fviz_actor_pass_order(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_overlay_topology_mode(FVizActor* actor, FVizOverlayTopologyMode mode);
FVIZ_RENDERING_API FVizOverlayTopologyMode fviz_actor_overlay_topology_mode(const FVizActor* actor);
FVIZ_RENDERING_API void fviz_actor_set_topology_data_flags(FVizActor* actor, uint32_t flags);
FVIZ_RENDERING_API uint32_t fviz_actor_topology_data_flags(const FVizActor* actor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_ACTOR_H */
