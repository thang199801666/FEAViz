# Interaction, renderer widget, and parallel ranges

FEAViz 0.5.0 separates native event collection from camera behavior:

```text
Win32 / future Qt, SDL, web host
              |
              v
    FVizInteractionEvent
              |
              v
FVizRenderWindowInteractor
       |              |
       | callback     | active style
       v              v
 application   FVizInteractorStyleTrackballCamera
                      |
                      v
             FVizRenderer / FVizCamera
```

The event callback runs first and may consume an event. Events not consumed are passed to the active style. The Win32 backend therefore contains no orbit, pan, dolly, fit, or representation logic.

Every `FVizRenderWindow` creates a default interactor and trackball-camera style. `FVizRendererWidget` is the high-level standalone-window facade:

```c
FVizRendererWidget* widget = NULL;
FVizActor* actor = NULL;

fviz_renderer_widget_create(1280, 800, "FEA result", &widget);
fviz_actor_create(&actor);
fviz_actor_set_poly_data(actor, surface);
fviz_renderer_widget_add_actor(widget, actor);
fviz_renderer_fit_camera(fviz_renderer_widget_renderer(widget), 1.2f);
fviz_renderer_widget_start(widget);
```

The current widget owns a native FEAViz window. Toolkit-native child embedding remains a later backend extension; application code can already obtain the underlying render window and native handle.

## Parallel ranges

`fviz_parallel_for()` partitions a half-open range into deterministic, non-overlapping chunks. `grain_size` controls the minimum useful work per task, the global thread limit can constrain resource use, and thread-creation failure falls back to executing that range synchronously.

```c
static void update_range(FVizSize begin, FVizSize end, void* user_data)
{
    float* values = (float*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i) values[i] *= 2.0f;
}

fviz_parallel_set_thread_limit(8u);
fviz_parallel_for(0u, count, 4096u, update_range, values);
```

Callbacks must only mutate state owned by their assigned range, or provide their own synchronization. The warp-by-vector filter follows this rule by computing displaced points in parallel and appending topology sequentially.
