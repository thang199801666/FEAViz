#ifndef FVIZ_RENDERING_RENDER_WINDOW_H
#define FVIZ_RENDERING_RENDER_WINDOW_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Spatial/FVizBVH.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderWindow FVizRenderWindow;
typedef struct FVizRenderWindowInteractor FVizRenderWindowInteractor;
#define FVIZ_TYPE_RENDER_WINDOW UINT64_C(0x4D73FB2C6579C1DF)

typedef enum FVizRenderWindowState
{
    FVIZ_RENDER_WINDOW_CREATED = 0,
    FVIZ_RENDER_WINDOW_INITIALIZED = 1,
    FVIZ_RENDER_WINDOW_VISIBLE = 2,
    FVIZ_RENDER_WINDOW_OFFSCREEN = 3,
    FVIZ_RENDER_WINDOW_FINALIZED = 4
} FVizRenderWindowState;

typedef struct FVizRenderCapabilities
{
    uint32_t struct_size;
    uint32_t gl_major;
    uint32_t gl_minor;
    FVizBool modern_pipeline;
    FVizBool offscreen_supported;
    FVizBool color_readback_supported;
    FVizBool depth_readback_supported;
} FVizRenderCapabilities;

typedef struct FVizHardwarePick
{
    uint32_t struct_size;
    FVizRenderer* renderer;
    FVizActor* actor;
    FVizSize rendered_primitive_id;
    FVizId original_cell_id;
    FVizId original_face_id;
    float depth;
} FVizHardwarePick;

typedef void (*FVizPickCallbackFn)(
    FVizRenderWindow* window,
    int x,
    int y,
    const FVizRayHit* hit,
    void* user_data);

FVIZ_API FVizResult fviz_render_window_create(
    int width,
    int height,
    const char* title,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_offscreen(
    int width,
    int height,
    FVizRenderWindow** out_window);
FVIZ_API FVizResult fviz_render_window_create_attached(
    void* host_native_handle,
    int width,
    int height,
    FVizRenderWindow** out_window);
FVIZ_API FVizRenderWindowState fviz_render_window_state(const FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_initialize(FVizRenderWindow* window);
FVIZ_API void fviz_render_window_finalize(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_resize(FVizRenderWindow* window, int width, int height);
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
FVIZ_API FVizResult fviz_render_window_hardware_pick(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizHardwarePick* out_pick);
FVIZ_API FVizResult fviz_render_window_set_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizRenderer* fviz_render_window_renderer(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_add_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizResult fviz_render_window_remove_renderer(FVizRenderWindow* window, FVizRenderer* renderer);
FVIZ_API FVizSize fviz_render_window_renderer_count(const FVizRenderWindow* window);
FVIZ_API FVizRenderer* fviz_render_window_renderer_at(FVizRenderWindow* window, FVizSize index);
FVIZ_API FVizRenderer* fviz_render_window_find_renderer(FVizRenderWindow* window, int x, int y);
FVIZ_API void fviz_render_window_get_size(const FVizRenderWindow* window, int* width, int* height);
FVIZ_API FVizRenderWindowInteractor* fviz_render_window_interactor(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_show(FVizRenderWindow* window);
FVIZ_API FVizResult fviz_render_window_render(FVizRenderWindow* window);
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
FVIZ_API void fviz_render_window_set_pick_callback(
    FVizRenderWindow* window,
    FVizPickCallbackFn callback,
    void* user_data);
FVIZ_API void fviz_render_window_request_close(FVizRenderWindow* window);
FVIZ_API void* fviz_render_window_native_handle(FVizRenderWindow* window);
FVIZ_API FVizBool fviz_render_window_supported(void);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDER_WINDOW_H */
