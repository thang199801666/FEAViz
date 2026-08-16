// FEAViz C++ binding - interaction.
//
// RAII wrappers over the interactor and interactor-style API. These drive the
// trackball camera controls (rotate/pan/dolly) used by the renderer widget.

#ifndef FVIZ_CPP_INTERACTION_HPP
#define FVIZ_CPP_INTERACTION_HPP

#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Interaction/FVizInteractorStyle.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>

#include "FVizCppObject.hpp"
#include "FVizCppRendering.hpp"

#include <functional>
#include <memory>

namespace fviz {

// ---------------------------------------------------------------------------
// InteractionEvent - a thin value wrapper over FVizInteractionEvent.
// ---------------------------------------------------------------------------
struct InteractionEvent {
    FVizInteractionEventType type = FVIZ_INTERACTION_EVENT_ANY;
    FVizMouseButton button = FVIZ_MOUSE_BUTTON_NONE;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int key = 0;
    float wheel_delta = 0.0f;
    FVizId timer_id = 0;
    double timestamp_seconds = 0.0;
    bool shift = false;
    bool control = false;
    bool alt = false;
    unsigned int character = 0;
    int delta_x = 0;
    int delta_y = 0;
    float content_scale = 1.0f;

    FVizInteractionEvent toC() const noexcept
    {
        FVizInteractionEvent event;
        event.type = type;
        event.button = button;
        event.x = x;
        event.y = y;
        event.width = width;
        event.height = height;
        event.key = key;
        event.wheel_delta = wheel_delta;
        event.timer_id = timer_id;
        event.timestamp_seconds = timestamp_seconds;
        event.shift = detail::fbool(shift);
        event.control = detail::fbool(control);
        event.alt = detail::fbool(alt);
        event.character = character;
        event.delta_x = delta_x;
        event.delta_y = delta_y;
        event.content_scale = content_scale;
        return event;
    }
};

// ---------------------------------------------------------------------------
// InteractorStyle - trackball camera / actor controls.
// ---------------------------------------------------------------------------
class InteractorStyle : public Object<FVizInteractorStyle> {
public:
    InteractorStyle() = default;
    explicit InteractorStyle(FVizInteractorStyle* owned) : Object<FVizInteractorStyle>(owned) {}
    explicit InteractorStyle(void* owned) : Object<FVizInteractorStyle>(owned) {}

    static InteractorStyle trackballCamera()
    {
        FVizInteractorStyle* style = nullptr;
        detail::checkResult(fviz_interactor_style_trackball_camera_create(&style));
        return InteractorStyle(style);
    }

    static InteractorStyle trackballActorStyle()
    {
        FVizInteractorStyle* style = nullptr;
        detail::checkResult(fviz_interactor_style_trackball_actor_create(&style));
        return InteractorStyle(style);
    }

    void setTrackballActor(Actor& actor) { detail::checkResult(fviz_interactor_style_trackball_actor_set_actor(ptr_, actor.get())); }
    Actor trackballActor() const
    {
        FVizActor* actor = ptr_ ? fviz_interactor_style_trackball_actor_actor(ptr_) : nullptr;
        return Actor(actor != nullptr ? static_cast<FVizActor*>(fviz_retain(actor)) : nullptr);
    }

    void setOrbitSensitivity(float radians_per_pixel) noexcept { if (ptr_) fviz_interactor_style_set_orbit_sensitivity(ptr_, radians_per_pixel); }
    float orbitSensitivity() const noexcept { return ptr_ ? fviz_interactor_style_orbit_sensitivity(ptr_) : 0.0f; }
    void setPanSensitivity(float fraction_per_pixel) noexcept { if (ptr_) fviz_interactor_style_set_pan_sensitivity(ptr_, fraction_per_pixel); }
    float panSensitivity() const noexcept { return ptr_ ? fviz_interactor_style_pan_sensitivity(ptr_) : 0.0f; }
    void setDollyFactor(float factor) noexcept { if (ptr_) fviz_interactor_style_set_dolly_factor(ptr_, factor); }
    float dollyFactor() const noexcept { return ptr_ ? fviz_interactor_style_dolly_factor(ptr_) : 0.0f; }
    FVizInteractionState state() const noexcept { return ptr_ ? fviz_interactor_style_state(ptr_) : FVIZ_INTERACTION_STATE_NONE; }
    void cancelInteraction() noexcept { if (ptr_) fviz_interactor_style_cancel_interaction(ptr_); }

    bool processEvent(Renderer& renderer, const InteractionEvent& event) noexcept
    {
        if (!ptr_) return false;
        const FVizInteractionEvent c_event = event.toC();
        return fviz_interactor_style_process_event(ptr_, renderer.get(), &c_event) != FVIZ_FALSE;
    }
};

// ---------------------------------------------------------------------------
// RenderWindowInteractor - routes native events to an interactor style.
// ---------------------------------------------------------------------------
class RenderWindowInteractor : public Object<FVizRenderWindowInteractor> {
public:
    RenderWindowInteractor() = default;
    explicit RenderWindowInteractor(FVizRenderWindowInteractor* owned) : Object<FVizRenderWindowInteractor>(owned) {}
    explicit RenderWindowInteractor(void* owned) : Object<FVizRenderWindowInteractor>(owned) {}

    void enable() noexcept { if (ptr_) fviz_render_window_interactor_enable(ptr_); }
    void disable() noexcept { if (ptr_) fviz_render_window_interactor_disable(ptr_); }
    bool enabled() const noexcept { return ptr_ ? fviz_render_window_interactor_enabled(ptr_) != FVIZ_FALSE : false; }
    void setDone(bool done) noexcept { if (ptr_) fviz_render_window_interactor_set_done(ptr_, detail::fbool(done)); }
    bool done() const noexcept { return ptr_ ? fviz_render_window_interactor_done(ptr_) != FVIZ_FALSE : false; }

    void setStyle(InteractorStyle& style) { detail::checkResult(fviz_render_window_interactor_set_style(ptr_, style.get())); }
    InteractorStyle style() const
    {
        FVizInteractorStyle* s = ptr_ ? fviz_render_window_interactor_style(ptr_) : nullptr;
        return InteractorStyle(s != nullptr ? static_cast<FVizInteractorStyle*>(fviz_retain(s)) : nullptr);
    }

    void setUpdateRates(double desired, double still) noexcept { if (ptr_) fviz_render_window_interactor_set_update_rates(ptr_, desired, still); }

    Renderer pokedRenderer() const
    {
        FVizRenderer* r = ptr_ ? fviz_render_window_interactor_poked_renderer(ptr_) : nullptr;
        return Renderer(r != nullptr ? static_cast<FVizRenderer*>(fviz_retain(r)) : nullptr);
    }
    Renderer capturedRenderer() const
    {
        FVizRenderer* r = ptr_ ? fviz_render_window_interactor_captured_renderer(ptr_) : nullptr;
        return Renderer(r != nullptr ? static_cast<FVizRenderer*>(fviz_retain(r)) : nullptr);
    }

    void grabFocus() noexcept { if (ptr_) fviz_render_window_interactor_grab_focus(ptr_); }
    void releaseFocus() noexcept { if (ptr_) fviz_render_window_interactor_release_focus(ptr_); }
    bool hasFocus() const noexcept { return ptr_ ? fviz_render_window_interactor_has_focus(ptr_) != FVIZ_FALSE : false; }
    void cancelInteraction() noexcept { if (ptr_) fviz_render_window_interactor_cancel_interaction(ptr_); }

    FVizTimerId createTimer(double interval_seconds, bool repeating, double now_seconds)
    {
        FVizTimerId id = 0;
        detail::checkResult(fviz_render_window_interactor_create_timer(ptr_, interval_seconds, detail::fbool(repeating), now_seconds, &id));
        return id;
    }

    bool processEvent(const InteractionEvent& event) noexcept
    {
        if (!ptr_) return false;
        const FVizInteractionEvent c_event = event.toC();
        return fviz_render_window_interactor_process_event(ptr_, &c_event) != FVIZ_FALSE;
    }

    void start() { detail::checkResult(fviz_render_window_interactor_start(ptr_)); }
    void render() { detail::checkResult(fviz_render_window_interactor_render(ptr_)); }
    void requestRender() { detail::checkResult(fviz_render_window_interactor_request_render(ptr_)); }

    // Registers a per-event callback. The callback receives the C event struct;
    // returning true consumes the event (like an aborting observer). The
    // std::function is kept alive on the heap for the interactor lifetime.
    using EventCallback = std::function<bool(const FVizInteractionEvent&)>;
    void setEventCallback(EventCallback callback)
    {
        if (!ptr_) return;
        auto holder = std::make_shared<CallbackHolder>();
        holder->fn = std::move(callback);
        callback_holder_ = holder;
        fviz_render_window_interactor_set_event_callback(ptr_, &CallbackHolder::invoke, holder.get());
    }

private:
    struct CallbackHolder {
        EventCallback fn;
        static FVizBool invoke(FVizRenderWindowInteractor* /*interactor*/,
            const FVizInteractionEvent* event, void* user_data)
        {
            auto* holder = static_cast<CallbackHolder*>(user_data);
            if (holder == nullptr || holder->fn == nullptr) return FVIZ_FALSE;
            return holder->fn(*event) ? detail::fbool(true) : detail::fbool(false);
        }
    };
    std::shared_ptr<CallbackHolder> callback_holder_;
};

// Defined here so RenderWindowInteractor is complete. Returns a retained view
// of the window's interactor.
inline RenderWindowInteractor RenderWindow::interactor() const
{
    FVizRenderWindowInteractor* raw = ptr_ != nullptr ? fviz_render_window_interactor(ptr_) : nullptr;
    if (raw == nullptr) return RenderWindowInteractor();
    return RenderWindowInteractor(static_cast<FVizRenderWindowInteractor*>(fviz_retain(raw)));
}

} // namespace fviz

#endif // FVIZ_CPP_INTERACTION_HPP
