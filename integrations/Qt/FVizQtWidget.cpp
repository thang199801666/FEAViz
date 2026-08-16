#include "FVizQtWidget.h"

#include <algorithm>
#include <QEvent>
#include <QFocusEvent>
#include <QHideEvent>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <FViz/Core/FVizError.h>

namespace
{
void* qwidget_native_handle(QWidget* widget)
{
    if (widget == nullptr) return nullptr;
    const WId id = widget->winId();
    return reinterpret_cast<void*>(id);
}
} // namespace

FVizQtWidget::FVizQtWidget(QWidget* parent, const FVizRenderWindowOptions* options)
    : QWidget(parent)
{
    FVizRenderWindowOptions defaults;
    const FVizRenderWindowOptions* effective_options = options;

    setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    /* Force a stable native host before FEAViz creates its child HWND. */
    (void)winId();
    host_handle_ = qwidget_native_handle(this);

    if (effective_options == nullptr)
    {
        fviz_render_window_options_initialize(&defaults);
        effective_options = &defaults;
    }

    initialization_result_ = fviz_renderer_widget_create_attached_with_options(
        host_handle_,
        std::max(1, width()),
        std::max(1, height()),
        effective_options,
        &widget_);

    if (initialization_result_ == FVIZ_OK)
    {
        /* Query the HWND client rect to convert Qt logical geometry to actual
         * native pixels on 125/150/200% displays. */
        (void)fviz_renderer_widget_sync_host_size(widget_);
        (void)fviz_renderer_widget_show(widget_);
    }

    elapsed_.start();
    timer_pump_.setTimerType(Qt::PreciseTimer);
    timer_pump_.setInterval(16);
    QObject::connect(&timer_pump_, &QTimer::timeout, this, [this]() {
        pumpInteractorTimers();
    });
    /* Started lazily from showEvent(); hidden widgets do not need timer work. */
}

FVizQtWidget::~FVizQtWidget()
{
    timer_pump_.stop();
    fviz_release(widget_);
    widget_ = nullptr;
}

bool FVizQtWidget::isValid() const noexcept
{
    return widget_ != nullptr && initialization_result_ == FVIZ_OK;
}

FVizResult FVizQtWidget::initializationResult() const noexcept
{
    return initialization_result_;
}

FVizRendererWidget* FVizQtWidget::rendererWidget() noexcept
{
    return widget_;
}

FVizRenderWindow* FVizQtWidget::renderWindow() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_window(widget_) : nullptr;
}

FVizRenderer* FVizQtWidget::renderer() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_renderer(widget_) : nullptr;
}

FVizRenderWindowInteractor* FVizQtWidget::interactor() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_interactor(widget_) : nullptr;
}

void* FVizQtWidget::nativeRenderHandle() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_native_handle(widget_) : nullptr;
}

void* FVizQtWidget::nativeHostHandle() noexcept
{
    return host_handle_;
}

FVizResult FVizQtWidget::syncToHost()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    return fviz_renderer_widget_sync_host_size(widget_);
}

FVizResult FVizQtWidget::renderNow()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    return fviz_renderer_widget_render(widget_);
}

FVizResult FVizQtWidget::requestRender()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    if (window == nullptr) return FVIZ_ERROR_INVALID_STATE;
    fviz_render_window_request_render(window);
    return FVIZ_OK;
}

bool FVizQtWidget::renderPending() const noexcept
{
    if (widget_ == nullptr) return false;
    const FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    return window != nullptr && fviz_render_window_render_requested(window) != FVIZ_FALSE;
}

FVizResult FVizQtWidget::renderIfPending()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    return window != nullptr
        ? fviz_render_window_render_if_requested(window)
        : FVIZ_ERROR_INVALID_STATE;
}

FVizResult FVizQtWidget::addObserver(
    FVizObject* object,
    FVizEventId eventId,
    float priority,
    FVizObserverCallbackFn callback,
    void* clientData,
    FVizObserverTag* outTag)
{
    if (object == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_add_observer(object, eventId, priority, callback, clientData, outTag);
}

FVizResult FVizQtWidget::addCommandObserver(
    FVizObject* object,
    FVizEventId eventId,
    float priority,
    FVizCommand* command,
    FVizObserverTag* outTag)
{
    if (object == nullptr || command == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_add_command_observer(object, eventId, priority, command, outTag);
}

FVizResult FVizQtWidget::removeObserver(FVizObject* object, FVizObserverTag tag)
{
    if (object == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_remove_observer(object, tag);
}

void FVizQtWidget::setInteractorTimerPumpEnabled(bool enabled)
{
    timer_pump_requested_ = enabled;
    if (enabled && isVisible())
    {
        if (!timer_pump_.isActive()) timer_pump_.start();
    }
    else
    {
        timer_pump_.stop();
    }
}

bool FVizQtWidget::interactorTimerPumpEnabled() const noexcept
{
    return timer_pump_requested_;
}

void FVizQtWidget::setInteractorTimerPumpInterval(int milliseconds)
{
    timer_pump_.setInterval(std::max(1, milliseconds));
}

int FVizQtWidget::interactorTimerPumpInterval() const noexcept
{
    return timer_pump_.interval();
}

FVizResult FVizQtWidget::ensureCurrentHost()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    void* current = qwidget_native_handle(this);
    if (current == nullptr) return FVIZ_ERROR_INVALID_STATE;
    if (current == host_handle_) return FVIZ_OK;
    const FVizResult result = fviz_renderer_widget_reparent(widget_, current);
    if (result == FVIZ_OK) host_handle_ = current;
    return result;
}

bool FVizQtWidget::event(QEvent* event)
{
    const bool handled = QWidget::event(event);
    if (widget_ != nullptr && event != nullptr)
    {
        switch (event->type())
        {
            case QEvent::WinIdChange:
            case QEvent::ParentChange:
            case QEvent::ShowToParent:
            case QEvent::WindowStateChange:
                (void)ensureCurrentHost();
                (void)fviz_renderer_widget_sync_host_size(widget_);
                break;
            default:
                break;
        }
    }
    return handled;
}

QPaintEngine* FVizQtWidget::paintEngine() const
{
    /* FEAViz paints its native child HWND directly; telling Qt there is no
     * QWidget paint engine prevents unnecessary backing-store work/flicker. */
    return nullptr;
}

void FVizQtWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (widget_ != nullptr)
    {
        (void)ensureCurrentHost();
        (void)fviz_renderer_widget_sync_host_size(widget_);
    }
}

void FVizQtWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (widget_ != nullptr)
    {
        (void)ensureCurrentHost();
        (void)fviz_renderer_widget_sync_host_size(widget_);
        (void)fviz_renderer_widget_show(widget_);
        (void)requestRender();
    }
    if (timer_pump_requested_ && !timer_pump_.isActive()) timer_pump_.start();
}

void FVizQtWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    timer_pump_.stop();
}

void FVizQtWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (widget_ != nullptr)
    {
        (void)ensureCurrentHost();
        HWND child = reinterpret_cast<HWND>(fviz_renderer_widget_native_handle(widget_));
        if (child != nullptr && IsWindow(child) != FALSE) (void)SetFocus(child);
    }
}

void FVizQtWidget::pumpInteractorTimers()
{
    FVizRenderWindowInteractor* current = interactor();
    if (current == nullptr || !isVisible()) return;
    (void)fviz_render_window_interactor_process_timers(
        current,
        static_cast<double>(elapsed_.nsecsElapsed()) / 1.0e9);
}
