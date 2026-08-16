#ifndef FVIZ_INTERNAL_RENDERING_GL_DEVICE_H
#define FVIZ_INTERNAL_RENDERING_GL_DEVICE_H

#include <FViz/Core/FVizResult.h>
#include <FViz/Rendering/FVizRenderPass.h>
#include <FViz/Rendering/FVizRenderWindow.h>

typedef struct FVizGLFunctions FVizGLFunctions;
typedef struct FVizGLDevice FVizGLDevice;
typedef struct FVizRenderer FVizRenderer;
typedef struct FVizScalarLegend FVizScalarLegend;

typedef struct FVizGLStateSnapshot
{
    int viewport[4];
    int scissor_box[4];
    int framebuffer;
    int program;
    int vertex_array;
    int depth_function;
    int cull_face_mode;
    int polygon_mode[2];
    float line_width;
    float point_size;
    FVizBool blend_enabled;
    FVizBool depth_test_enabled;
    FVizBool cull_face_enabled;
    FVizBool scissor_enabled;
    FVizBool multisample_enabled;
    FVizBool dither_enabled;
    FVizBool depth_write;
    FVizBool color_write[4];
} FVizGLStateSnapshot;

typedef struct FVizGLFrameStatistics
{
    uint64_t draw_calls;
    uint64_t triangles;
    uint64_t lines;
    uint64_t gpu_uploads;
    uint64_t gpu_upload_bytes;
    uint64_t resident_actor_resources;
    uint64_t resident_mesh_gpu_bytes;
    uint64_t gpu_mesh_byte_budget;
    uint64_t gpu_resource_evictions;
    FVizBool gpu_mesh_budget_exceeded;
    uint64_t actors_considered;
    uint64_t actors_frustum_culled;
    uint64_t actors_small_object_culled;
    uint64_t actors_visible_after_culling;
    uint64_t gpu_frame_nanoseconds;
    FVizBool gpu_timing_valid;
    uint64_t resident_geometry_gpu_bytes;
    uint64_t resident_attribute_gpu_bytes;
    uint64_t resident_instance_gpu_bytes;
    uint64_t resident_render_target_gpu_bytes;
    uint64_t pinned_gpu_resources;
    uint64_t custom_pass_state_restorations;
} FVizGLFrameStatistics;

FVizGLDevice* fviz_internal_gl_device_create(const FVizGLFunctions* functions);
void fviz_internal_gl_device_destroy(FVizGLDevice* device);
void fviz_internal_gl_device_begin_frame(FVizGLDevice* device);
void fviz_internal_gl_device_end_frame(FVizGLDevice* device);
void fviz_internal_gl_device_bind_framebuffer(FVizGLDevice* device, uint32_t framebuffer);
void fviz_internal_gl_device_capture_state(
    FVizGLDevice* device, FVizGLStateSnapshot* out_snapshot);
FVizResult fviz_internal_gl_device_restore_state(
    FVizGLDevice* device, const FVizGLStateSnapshot* snapshot);
FVizBool fviz_internal_gl_device_fxaa_supported(const FVizGLDevice* device);
FVizBool fviz_internal_gl_device_weighted_oit_supported(const FVizGLDevice* device);
FVizBool fviz_internal_gl_device_shader_lines_supported(const FVizGLDevice* device);
FVizBool fviz_internal_gl_device_text_supported(const FVizGLDevice* device);
FVizBool fviz_internal_gl_device_integer_selection_supported(const FVizGLDevice* device);
FVizBool fviz_internal_gl_device_gpu_timing_supported(const FVizGLDevice* device);
void fviz_internal_gl_device_get_frame_statistics(
    const FVizGLDevice* device,
    FVizGLFrameStatistics* out_statistics);
void fviz_internal_gl_device_set_memory_options(
    FVizGLDevice* device,
    const FVizGPUMemoryOptions* options);
void fviz_internal_gl_device_release_mesh_resources(FVizGLDevice* device);
FVizResult fviz_internal_gl_device_render_stage(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio,
    int viewport_width,
    int viewport_height,
    FVizRenderPassStage stage);
FVizResult fviz_internal_gl_device_render_weighted_oit(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    int viewport_x,
    int viewport_y,
    int width,
    int height,
    uint32_t samples,
    float aspect_ratio,
    uint32_t target_framebuffer);
FVizResult fviz_internal_gl_device_apply_fxaa(
    FVizGLDevice* device,
    int width,
    int height,
    const FVizFXAAOptions* options,
    FVizBool srgb,
    uint32_t target_framebuffer);
FVizResult fviz_internal_gl_device_render_gradient_background(
    FVizGLDevice* device,
    const float bottom_color[3],
    const float top_color[3]);
FVizResult fviz_internal_gl_device_render_text_actors(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    int width,
    int height,
    float content_scale);
FVizResult fviz_internal_gl_device_render_legend(
    FVizGLDevice* device,
    const FVizScalarLegend* legend,
    int width,
    int height,
    float content_scale);
FVizResult fviz_internal_gl_device_select(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio,
    int x,
    int y,
    int viewport_width,
    int viewport_height,
    FVizSelectionAssociation association,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth);

#endif /* FVIZ_INTERNAL_RENDERING_GL_DEVICE_H */
