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
