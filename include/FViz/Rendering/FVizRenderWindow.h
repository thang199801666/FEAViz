#ifndef FVIZ_RENDERING_RENDER_WINDOW_H
#define FVIZ_RENDERING_RENDER_WINDOW_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizExternalOpenGLSurface.h>
#include <FViz/Spatial/FVizBVH.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderWindow FVizRenderWindow;
typedef struct FVizRenderWindowInteractor FVizRenderWindowInteractor;

typedef struct FVizFXAAOptions
{
    uint32_t struct_size;
    float relative_threshold;
    float absolute_threshold;
    float span_max;
} FVizFXAAOptions;

typedef struct FVizRenderWindowOptions
{
    uint32_t struct_size;
    uint32_t multisamples;
    FVizBool fxaa;
    int swap_interval;
    FVizBool adaptive_antialiasing;
    FVizBool srgb;
} FVizRenderWindowOptions;

#define FVIZ_TYPE_RENDER_WINDOW UINT64_C(0x4D73FB2C6579C1DF)

typedef enum FVizRenderWindowState
{
    FVIZ_RENDER_WINDOW_CREATED = 0,
    FVIZ_RENDER_WINDOW_INITIALIZED = 1,
    FVIZ_RENDER_WINDOW_VISIBLE = 2,
    FVIZ_RENDER_WINDOW_OFFSCREEN = 3,
    FVIZ_RENDER_WINDOW_FINALIZED = 4
} FVizRenderWindowState;

typedef uint32_t FVizRenderRequestReasons;

typedef enum FVizRenderRequestReason
{
    FVIZ_RENDER_REQUEST_NONE = 0u,
    FVIZ_RENDER_REQUEST_SCENE = 1u << 0,
    FVIZ_RENDER_REQUEST_CAMERA = 1u << 1,
    FVIZ_RENDER_REQUEST_INTERACTION = 1u << 2,
    FVIZ_RENDER_REQUEST_RESIZE = 1u << 3,
    FVIZ_RENDER_REQUEST_ANIMATION = 1u << 4,
    FVIZ_RENDER_REQUEST_EXTERNAL = 1u << 5,
    FVIZ_RENDER_REQUEST_DIRECT = 1u << 6
} FVizRenderRequestReason;

typedef enum FVizFrameQuality
{
    FVIZ_FRAME_QUALITY_STILL = 0,
    FVIZ_FRAME_QUALITY_INTERACTIVE = 1
} FVizFrameQuality;

typedef struct FVizFrameSchedulerOptions
{
    uint32_t struct_size;
    /* Zero delegates pacing to the interactor's update-rate settings. */
    double interactive_target_fps;
    double still_target_fps;
    FVizBool interactive_quality;
} FVizFrameSchedulerOptions;

typedef struct FVizFrameSchedulerStatistics
{
    uint32_t struct_size;
    uint64_t request_count;
    uint64_t coalesced_request_count;
    uint64_t rendered_frame_count;
    FVizRenderRequestReasons pending_reasons;
    FVizRenderRequestReasons last_frame_reasons;
    FVizFrameQuality last_frame_quality;
} FVizFrameSchedulerStatistics;

typedef struct FVizGPUMemoryOptions
{
    uint32_t struct_size;
    /* Zero means unlimited. Visible resources are never evicted to satisfy the
     * budget; budget_exceeded reports unavoidable active working-set pressure. */
    uint64_t mesh_byte_budget;
    uint32_t unused_resource_retention_frames;
} FVizGPUMemoryOptions;

typedef struct FVizRenderCapabilities
{
    uint32_t struct_size;
    uint32_t gl_major;
    uint32_t gl_minor;
    FVizBool modern_pipeline;
    FVizBool offscreen_supported;
    FVizBool color_readback_supported;
    FVizBool depth_readback_supported;
    FVizBool multisample_supported;
    FVizBool fxaa_supported;
    FVizBool swap_control_supported;
    uint32_t sample_count;
    FVizBool srgb_supported;
    FVizBool weighted_oit_supported;
    FVizBool shader_lines_supported;
    FVizBool text_rendering_supported;
    FVizBool integer_selection_supported;
    FVizBool gpu_timing_supported;
    FVizBool depth_peeling_supported;
} FVizRenderCapabilities;

typedef struct FVizRenderStatistics
{
    uint32_t struct_size;
    uint64_t frame_number;
    double render_seconds;
    double present_seconds;
    uint64_t draw_calls;
    uint64_t triangles;
    uint64_t lines;
    uint64_t gpu_uploads;
    uint64_t gpu_upload_bytes;
    /* Historical field name retained for ABI compatibility. Counts resident
     * mapper/glyph GPU mesh resources; multiple actors sharing one mapper can
     * therefore contribute a single resource. */
    uint64_t resident_actor_resources;
    uint32_t sample_count;
    FVizBool fxaa_applied;
    FVizBool interaction_active;
    FVizBool srgb_applied;
    FVizTransparencyMode transparency_mode_requested;
    FVizTransparencyMode transparency_mode_applied;
    /* Appended large-scene counters (0.25+). */
    uint64_t actors_considered;
    uint64_t actors_frustum_culled;
    uint64_t actors_small_object_culled;
    uint64_t actors_visible_after_culling;
    uint64_t gpu_frame_nanoseconds;
    FVizBool gpu_timing_valid;
    /* Approximate bytes occupied by resident mapper/glyph mesh buffers
     * (positions, normals, indices, colors, adjacency and instance data).
     * Render targets, textures, shaders and driver overhead are excluded. */
    uint64_t resident_mesh_gpu_bytes;
    FVizRenderRequestReasons request_reasons;
    FVizFrameQuality frame_quality;
    uint64_t gpu_mesh_byte_budget;
    uint64_t gpu_resource_evictions;
    FVizBool gpu_mesh_budget_exceeded;
    uint64_t resident_geometry_gpu_bytes;
    uint64_t resident_attribute_gpu_bytes;
    uint64_t resident_instance_gpu_bytes;
    uint64_t resident_render_target_gpu_bytes;
    uint64_t pinned_gpu_resources;
    /* Appended executable render-graph observability (0.51+). */
    uint64_t render_graph_compile_generation;
    uint32_t render_graph_pass_count;
    uint64_t custom_pass_state_restorations;
} FVizRenderStatistics;

#define FVIZ_RENDER_PASS_STATISTICS_NAME_CAPACITY 32u

typedef struct FVizRenderPassStatistics
{
    uint32_t struct_size;
    FVizRenderer* renderer;
    FVizRenderGraphPassId graph_pass_id;
    FVizSize execution_index;
    FVizRenderPassStage stage;
    char name[FVIZ_RENDER_PASS_STATISTICS_NAME_CAPACITY];
    double cpu_seconds;
    uint64_t gpu_nanoseconds;
    FVizBool gpu_timing_valid;
    FVizResult result;
} FVizRenderPassStatistics;

typedef struct FVizHardwarePick
{
    uint32_t struct_size;
    FVizRenderer* renderer;
    FVizActor* actor;
    FVizSize rendered_primitive_id;
    FVizId original_cell_id;
    FVizId original_face_id;
    float depth;
    FVizSelectionAssociation association;
} FVizHardwarePick;

typedef struct FVizPickEventData
{
    uint32_t struct_size;
    int x;
    int y;
    FVizSelectionAssociation association;
    FVizBool hardware;
    FVizResult result;
    FVizHardwarePick hardware_pick;
    FVizRayHit ray_hit;
} FVizPickEventData;

typedef void (*FVizPickCallbackFn)(
    FVizRenderWindow* window,
    int x,
    int y,
    const FVizRayHit* hit,
    void* user_data);

FVIZ_API void fviz_fxaa_options_initialize(FVizFXAAOptions* options);
FVIZ_API void fviz_render_window_options_initialize(FVizRenderWindowOptions* options);
FVIZ_API void fviz_frame_scheduler_options_initialize(FVizFrameSchedulerOptions* options);
FVIZ_API void fviz_gpu_memory_options_initialize(FVizGPUMemoryOptions* options);
FVIZ_API FVizResult fviz_render_window_create_with_options(
    int width,
    int height,
    const char* title,
    const FVizRenderWindowOptions* options,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_offscreen_with_options(
    int width,
    int height,
    const FVizRenderWindowOptions* options,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create(
    int width,
    int height,
    const char* title,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_offscreen(
    int width,
    int height,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_attached_with_options(
    void* host_native_handle,
    int width,
    int height,
    const FVizRenderWindowOptions* options,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_attached(
    void* host_native_handle,
    int width,
    int height,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_external_opengl_with_options(
    int width,
    int height,
    const FVizExternalOpenGLSurface* surface,
    const FVizRenderWindowOptions* options,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_external_opengl(
    int width,
    int height,
    const FVizExternalOpenGLSurface* surface,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_sync_external_surface_size(FVizRenderWindow* window);
FVIZ_API FVizBool fviz_render_window_is_external_opengl(const FVizRenderWindow* window);
/* External-context lifecycle hooks. Hosts that recreate their OpenGL context
 * (for example QOpenGLWidget during some reparent/top-level transitions) can
 * release only FEAViz GPU resources before the old context dies, then rebuild
 * them on the replacement context without replacing scene/render objects. */
FVIZ_API FVizResult fviz_render_window_release_external_opengl_resources(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_reinitialize_external_opengl(FVizRenderWindow* window);
FVIZ_API FVizRenderWindowState fviz_render_window_state(const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_initialize(FVizRenderWindow* window);
FVIZ_API void fviz_render_window_finalize(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_resize(FVizRenderWindow* window, int width, int height);
FVIZ_API FVizResult fviz_render_window_sync_host_size(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_reparent(
    FVizRenderWindow* window,
    void* host_native_handle);
FVIZ_API FVizResult fviz_render_window_read_rgba8(
    FVizRenderWindow* window,
    uint8_t* pixels,
    FVizSize capacity);
FVIZ_API FVizResult fviz_render_window_read_depth_f32(
    FVizRenderWindow* window,
    float* depth,
    FVizSize capacity);
FVIZ_API FVizResult fviz_render_window_write_ppm(
    FVizRenderWindow* window,
    const char* path);
FVIZ_API void fviz_render_window_get_capabilities(
    const FVizRenderWindow* window,
    FVizRenderCapabilities* out_capabilities);
FVIZ_API FVizResult fviz_render_window_set_multisamples(FVizRenderWindow* window, uint32_t samples);
FVIZ_API uint32_t fviz_render_window_multisamples(const FVizRenderWindow* window);
FVIZ_API uint32_t fviz_render_window_actual_multisamples(const FVizRenderWindow* window);
FVIZ_API void fviz_render_window_set_fxaa(FVizRenderWindow* window, FVizBool enabled);
FVIZ_API FVizBool fviz_render_window_fxaa(const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_set_fxaa_options(
    FVizRenderWindow* window,
    const FVizFXAAOptions* options);
FVIZ_API void fviz_render_window_get_fxaa_options(
    const FVizRenderWindow* window,
    FVizFXAAOptions* out_options);
FVIZ_API void fviz_render_window_set_adaptive_antialiasing(
    FVizRenderWindow* window,
    FVizBool enabled);
FVIZ_API FVizBool fviz_render_window_adaptive_antialiasing(const FVizRenderWindow* window);
FVIZ_API FVizBool fviz_render_window_interaction_active(const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_set_swap_interval(FVizRenderWindow* window, int interval);
FVIZ_API int fviz_render_window_swap_interval(const FVizRenderWindow* window);
FVIZ_API void fviz_render_window_set_srgb(FVizRenderWindow* window, FVizBool enabled);
FVIZ_API FVizBool fviz_render_window_srgb(const FVizRenderWindow* window);
FVIZ_API void fviz_render_window_get_statistics(
    const FVizRenderWindow* window,
    FVizRenderStatistics* out_statistics);
FVIZ_API FVizSize fviz_render_window_pass_statistics_count(
    const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_get_pass_statistics(
    const FVizRenderWindow* window,
    FVizSize index,
    FVizRenderPassStatistics* out_statistics);
FVIZ_API FVizResult fviz_render_window_hardware_pick(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizHardwarePick* out_pick);
FVIZ_API FVizResult fviz_render_window_hardware_pick_association(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizSelectionAssociation association,
    FVizHardwarePick* out_pick);
FVIZ_API FVizResult fviz_render_window_set_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizRenderer* fviz_render_window_renderer(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_add_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizResult fviz_render_window_remove_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizSize fviz_render_window_renderer_count(const FVizRenderWindow* window);
FVIZ_API FVizRenderer* fviz_render_window_renderer_at(FVizRenderWindow* window, FVizSize index);
FVIZ_API FVizRenderer* fviz_render_window_find_renderer(FVizRenderWindow* window, int x, int y);
FVIZ_API void fviz_render_window_get_size(const FVizRenderWindow* window, int* width, int* height);
FVIZ_API uint32_t fviz_render_window_dpi(const FVizRenderWindow* window);
FVIZ_API float fviz_render_window_content_scale(const FVizRenderWindow* window);
FVIZ_API FVizRenderWindowInteractor* fviz_render_window_interactor(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_show(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_render(FVizRenderWindow* window);
/* Coalesced redraw path for GUI/toolkit integration. Multiple requests while a
 * redraw is already pending collapse into one platform paint notification. */
FVIZ_API void fviz_render_window_request_render(FVizRenderWindow* window);
FVIZ_API void fviz_render_window_request_render_reason(
    FVizRenderWindow* window,
    FVizRenderRequestReasons reasons);
FVIZ_API FVizBool fviz_render_window_render_requested(const FVizRenderWindow* window);
FVIZ_API uint64_t fviz_render_window_render_request_serial(const FVizRenderWindow* window);
FVIZ_API FVizRenderRequestReasons fviz_render_window_pending_render_reasons(
    const FVizRenderWindow* window);
FVIZ_API FVizFrameQuality fviz_render_window_frame_quality(const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_set_frame_scheduler_options(
    FVizRenderWindow* window,
    const FVizFrameSchedulerOptions* options);
FVIZ_API void fviz_render_window_get_frame_scheduler_options(
    const FVizRenderWindow* window,
    FVizFrameSchedulerOptions* out_options);
FVIZ_API void fviz_render_window_get_frame_scheduler_statistics(
    const FVizRenderWindow* window,
    FVizFrameSchedulerStatistics* out_statistics);
FVIZ_API void fviz_render_window_reset_frame_scheduler_statistics(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_set_gpu_memory_options(
    FVizRenderWindow* window,
    const FVizGPUMemoryOptions* options);
FVIZ_API void fviz_render_window_get_gpu_memory_options(
    const FVizRenderWindow* window,
    FVizGPUMemoryOptions* out_options);
/* Releases cached and active mesh buffers. They are rebuilt lazily on the next
 * render; render targets, shaders and font textures are left intact. */
FVIZ_API FVizResult fviz_render_window_release_gpu_mesh_resources(
    FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_render_if_requested(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_run(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_process_events(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_pick(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizRayHit* out_hit);
FVIZ_API FVizResult fviz_render_window_select_rectangle(
    FVizRenderWindow* window,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    FVizSelection** out_selection);
FVIZ_API FVizResult fviz_render_window_select_rectangle_association(
    FVizRenderWindow* window,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    FVizSelectionAssociation association,
    FVizSelection** out_selection);
FVIZ_API FVizResult fviz_render_window_select_polygon(
    FVizRenderWindow* window,
    const int* xy_points,
    FVizSize point_count,
    FVizSelectionAssociation association,
    FVizSelection** out_selection);
FVIZ_API FVizResult fviz_render_window_select_at(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizSelectionAssociation association,
    FVizSelection** out_selection);
FVIZ_API void fviz_render_window_set_pick_callback(
    FVizRenderWindow* window,
    FVizPickCallbackFn callback,
    void* user_data);
FVIZ_API void fviz_render_window_request_close(FVizRenderWindow* window);
/* Native embedding helpers.
 * The host handle is owned by the GUI toolkit/application. FEAViz only owns the
 * child render handle returned by fviz_render_window_native_handle(). Embedded
 * windows use the host application's event loop; do not call run()/start() from
 * a Qt/Win32 GUI that already dispatches native messages.
 */
FVIZ_API void* fviz_render_window_native_handle(FVizRenderWindow* window);
FVIZ_API void* fviz_render_window_host_native_handle(FVizRenderWindow* window);
FVIZ_API FVizBool fviz_render_window_is_attached(const FVizRenderWindow* window);
FVIZ_API FVizBool fviz_render_window_supported(void);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDER_WINDOW_H */
