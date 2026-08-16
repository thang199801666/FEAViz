#include "FVizQtWindow.h"

#include <algorithm>
#include <QEvent>
#include <QExposeEvent>
#include <QFocusEvent>
#include <QResizeEvent>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
void* qwindow_native_handle(QWindow* window)
{
    if (window == nullptr) return nullptr;
    const WId id = window->winId();
    return reinterpret_cast<void*>(id);
}
} // namespace

FVizQtWindow::FVizQtWindow(QWindow* parent, const FVizRenderWindowOptions* options)
    : QWindow(parent)
{
    FVizRenderWindowOptions defaults;
    const FVizRenderWindowOptions* effectiveOptions = options;

    setSurfaceType(QSurface::RasterSurface);
    host_handle_ = qwindow_native_handle(this);

    if (effectiveOptions == nullptr)
    {
        fviz_render_window_options_initialize(&defaults);
        effectiveOptions = &defaults;
    }

    initialization_result_ = fviz_renderer_widget_create_attached_with_options(
        host_handle_,
        std::max(1, width()),
        std::max(1, height()),
        effectiveOptions,
        &widget_);

    if (initialization_result_ == FVIZ_OK)
    {
        (void)fviz_renderer_widget_sync_host_size(widget_);
        (void)fviz_renderer_widget_show(widget_);
    }

    elapsed_.start();
    timer_pump_.setTimerType(Qt::PreciseTimer);
    timer_pump_.setInterval(16);
    QObject::connect(&timer_pump_, &QTimer::timeout, this, [this]() {
        pumpInteractorTimers();
    });
}

FVizQtWindow::~FVizQtWindow()
{
    timer_pump_.stop();
    fviz_release(widget_);
    widget_ = nullptr;
}

bool FVizQtWindow::isValid() const noexcept
{
    return widget_ != nullptr && initialization_result_ == FVIZ_OK;
}

FVizResult FVizQtWindow::initializationResult() const noexcept
{
    return initialization_result_;
}

FVizRendererWidget* FVizQtWindow::rendererWidget() noexcept
{
    return widget_;
}

FVizRenderWindow* FVizQtWindow::renderWindow() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_window(widget_) : nullptr;
}

FVizRenderer* FVizQtWindow::renderer() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_renderer(widget_) : nullptr;
}

FVizRenderWindowInteractor* FVizQtWindow::interactor() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_interactor(widget_) : nullptr;
}

void* FVizQtWindow::nativeRenderHandle() noexcept
{
    return widget_ != nullptr ? fviz_renderer_widget_native_handle(widget_) : nullptr;
}

void* FVizQtWindow::nativeHostHandle() noexcept
{
    return host_handle_;
}

FVizResult FVizQtWindow::syncToHost()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    return fviz_renderer_widget_sync_host_size(widget_);
}

FVizResult FVizQtWindow::renderNow()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    return fviz_renderer_widget_render(widget_);
}

FVizResult FVizQtWindow::requestRender()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    if (window == nullptr) return FVIZ_ERROR_INVALID_STATE;
    fviz_render_window_request_render(window);
    return FVIZ_OK;
}

bool FVizQtWindow::renderPending() const noexcept
{
    if (widget_ == nullptr) return false;
    const FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    return window != nullptr && fviz_render_window_render_requested(window) != FVIZ_FALSE;
}

FVizResult FVizQtWindow::renderIfPending()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    const FVizResult hostResult = ensureCurrentHost();
    if (hostResult != FVIZ_OK) return hostResult;
    FVizRenderWindow* window = fviz_renderer_widget_window(widget_);
    return window != nullptr
        ? fviz_render_window_render_if_requested(window)
        : FVIZ_ERROR_INVALID_STATE;
}

FVizResult FVizQtWindow::addObserver(
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

FVizResult FVizQtWindow::addCommandObserver(
    FVizObject* object,
    FVizEventId eventId,
    float priority,
    FVizCommand* command,
    FVizObserverTag* outTag)
{
    if (object == nullptr || command == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_add_command_observer(object, eventId, priority, command, outTag);
}

FVizResult FVizQtWindow::removeObserver(FVizObject* object, FVizObserverTag tag)
{
    if (object == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_remove_observer(object, tag);
}

void FVizQtWindow::setInteractorTimerPumpEnabled(bool enabled)
{
    timer_pump_requested_ = enabled;
    if (enabled && isExposed())
    {
        if (!timer_pump_.isActive()) timer_pump_.start();
    }
    else
    {
        timer_pump_.stop();
    }
}

bool FVizQtWindow::interactorTimerPumpEnabled() const noexcept
{
    return timer_pump_requested_;
}

void FVizQtWindow::setInteractorTimerPumpInterval(int milliseconds)
{
    timer_pump_.setInterval(std::max(1, milliseconds));
}

int FVizQtWindow::interactorTimerPumpInterval() const noexcept
{
    return timer_pump_.interval();
}

FVizResult FVizQtWindow::ensureCurrentHost()
{
    if (widget_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    void* current = qwindow_native_handle(this);
    if (current == nullptr) return FVIZ_ERROR_INVALID_STATE;
    if (current == host_handle_) return FVIZ_OK;
    const FVizResult result = fviz_renderer_widget_reparent(widget_, current);
    if (result == FVIZ_OK) host_handle_ = current;
    return result;
}

bool FVizQtWindow::event(QEvent* event)
{
    const bool handled = QWindow::event(event);
    if (event == nullptr) return handled;

    if (event->type() == QEvent::Hide)
        timer_pump_.stop();

    if (widget_ != nullptr)
    {
        switch (event->type())
        {
            case QEvent::WinIdChange:
            case QEvent::ParentChange:
            case QEvent::Show:
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

void FVizQtWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);
    if (widget_ != nullptr)
    {
        (void)ensureCurrentHost();
        (void)fviz_renderer_widget_sync_host_size(widget_);
        (void)requestRender();
    }
}

void FVizQtWindow::exposeEvent(QExposeEvent* event)
{
    QWindow::exposeEvent(event);
    if (widget_ != nullptr && isExposed())
    {
        (void)ensureCurrentHost();
        (void)fviz_renderer_widget_sync_host_size(widget_);
        (void)fviz_renderer_widget_show(widget_);
        (void)requestRender();
    }
    if (timer_pump_requested_ && isExposed() && !timer_pump_.isActive())
        timer_pump_.start();
}

void FVizQtWindow::focusInEvent(QFocusEvent* event)
{
    QWindow::focusInEvent(event);
    if (widget_ != nullptr)
    {
        (void)ensureCurrentHost();
        HWND child = reinterpret_cast<HWND>(fviz_renderer_widget_native_handle(widget_));
        if (child != nullptr && IsWindow(child) != FALSE) (void)SetFocus(child);
    }
}

void FVizQtWindow::pumpInteractorTimers()
{
    FVizRenderWindowInteractor* current = interactor();
    if (current == nullptr || !isExposed()) return;
    (void)fviz_render_window_interactor_process_timers(
        current,
        static_cast<double>(elapsed_.nsecsElapsed()) / 1.0e9);
}
