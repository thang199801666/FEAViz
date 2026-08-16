#include "FVizQtOpenGLWindow.h"

#include <algorithm>
#include <cmath>
#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWheelEvent>

namespace
{
FVizMouseButton fvizMouseButton(Qt::MouseButton button)
{
    switch (button)
    {
        case Qt::LeftButton: return FVIZ_MOUSE_BUTTON_LEFT;
        case Qt::MiddleButton: return FVIZ_MOUSE_BUTTON_MIDDLE;
        case Qt::RightButton: return FVIZ_MOUSE_BUTTON_RIGHT;
        case Qt::BackButton: return FVIZ_MOUSE_BUTTON_X1;
        case Qt::ForwardButton: return FVIZ_MOUSE_BUTTON_X2;
        default: return FVIZ_MOUSE_BUTTON_NONE;
    }
}

void eventPosition(const QMouseEvent* event, double* x, double* y)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF p = event->position();
#else
    const QPointF p = event->localPos();
#endif
    *x = p.x();
    *y = p.y();
}

void wheelPosition(const QWheelEvent* event, double* x, double* y)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF p = event->position();
#else
    const QPointF p = event->posF();
#endif
    *x = p.x();
    *y = p.y();
}
} // namespace

FVizQtOpenGLWindow::FVizQtOpenGLWindow(
    QWindow* parent,
    const FVizRenderWindowOptions* options)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, parent)
{
    QSurfaceFormat format;
    fviz_render_window_options_initialize(&options_);
    if (options != nullptr) options_ = *options;

    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(static_cast<int>(options_.multisamples));
    setFormat(format);

    elapsed_.start();
    timer_pump_.setTimerType(Qt::PreciseTimer);
    timer_pump_.setInterval(16);
    QObject::connect(&timer_pump_, &QTimer::timeout, this, [this]() {
        pumpInteractorTimers();
    });
}

FVizQtOpenGLWindow::~FVizQtOpenGLWindow()
{
    timer_pump_.stop();
    if (window_ != nullptr)
    {
        makeCurrent();
        fviz_release(window_);
        window_ = nullptr;
        doneCurrent();
    }
}

bool FVizQtOpenGLWindow::isValid() const noexcept
{
    return window_ != nullptr && initialization_result_ == FVIZ_OK;
}

FVizResult FVizQtOpenGLWindow::initializationResult() const noexcept
{
    return initialization_result_;
}

FVizRenderWindow* FVizQtOpenGLWindow::renderWindow() noexcept
{
    return window_;
}

FVizRenderer* FVizQtOpenGLWindow::renderer() noexcept
{
    return window_ != nullptr ? fviz_render_window_renderer(window_) : nullptr;
}

FVizRenderWindowInteractor* FVizQtOpenGLWindow::interactor() noexcept
{
    return window_ != nullptr ? fviz_render_window_interactor(window_) : nullptr;
}

FVizResult FVizQtOpenGLWindow::requestRender()
{
    if (window_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    fviz_render_window_request_render(window_);
    return FVIZ_OK;
}

bool FVizQtOpenGLWindow::renderPending() const noexcept
{
    return window_ != nullptr && fviz_render_window_render_requested(window_) != FVIZ_FALSE;
}

FVizResult FVizQtOpenGLWindow::renderIfPending()
{
    if (window_ == nullptr) return FVIZ_ERROR_INVALID_STATE;
    if (!renderPending()) return FVIZ_OK;
    update();
    return FVIZ_OK;
}

FVizResult FVizQtOpenGLWindow::addObserver(
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

FVizResult FVizQtOpenGLWindow::addCommandObserver(
    FVizObject* object,
    FVizEventId eventId,
    float priority,
    FVizCommand* command,
    FVizObserverTag* outTag)
{
    if (object == nullptr || command == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_add_command_observer(object, eventId, priority, command, outTag);
}

FVizResult FVizQtOpenGLWindow::removeObserver(FVizObject* object, FVizObserverTag tag)
{
    if (object == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_object_remove_observer(object, tag);
}

void FVizQtOpenGLWindow::setInteractorTimerPumpEnabled(bool enabled)
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

bool FVizQtOpenGLWindow::interactorTimerPumpEnabled() const noexcept
{
    return timer_pump_requested_;
}

void FVizQtOpenGLWindow::setInteractorTimerPumpInterval(int milliseconds)
{
    timer_pump_.setInterval(std::max(1, milliseconds));
}

int FVizQtOpenGLWindow::interactorTimerPumpInterval() const noexcept
{
    return timer_pump_.interval();
}

void FVizQtOpenGLWindow::initializeGL()
{
    if (context() != nullptr)
    {
        QObject::connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this]() {
            if (window_ == nullptr) return;
            makeCurrent();
            (void)fviz_render_window_release_external_opengl_resources(window_);
            doneCurrent();
        }, Qt::DirectConnection);
    }

    if (window_ != nullptr)
    {
        initialization_result_ = fviz_render_window_reinitialize_external_opengl(window_);
        if (initialization_result_ == FVIZ_OK)
        {
            (void)fviz_render_window_sync_external_surface_size(window_);
            fviz_render_window_request_render(window_);
        }
        return;
    }

    FVizExternalOpenGLSurface surface;
    fviz_external_opengl_surface_initialize(&surface);
    surface.user_data = this;
    surface.make_current = &FVizQtOpenGLWindow::makeCurrentCallback;
    surface.get_framebuffer_size = &FVizQtOpenGLWindow::framebufferSizeCallback;
    surface.get_default_framebuffer = &FVizQtOpenGLWindow::defaultFramebufferCallback;
    surface.request_render = &FVizQtOpenGLWindow::requestRenderCallback;
    surface.sample_count = static_cast<uint32_t>(std::max(0, format().samples()));
    surface.srgb_capable = FVIZ_FALSE;

    const qreal dpr = devicePixelRatio();
    const int physicalWidth = std::max(1, static_cast<int>(std::lround(width() * dpr)));
    const int physicalHeight = std::max(1, static_cast<int>(std::lround(height() * dpr)));
    initialization_result_ = fviz_render_window_create_external_opengl_with_options(
        physicalWidth, physicalHeight, &surface, &options_, &window_);
    if (initialization_result_ != FVIZ_OK) return;

    FVizRenderWindowInteractor* current = interactor();
    if (current != nullptr)
    {
        (void)fviz_render_window_interactor_initialize(current);
        fviz_render_window_interactor_enable(current);
    }
    fviz_render_window_request_render(window_);
}

void FVizQtOpenGLWindow::resizeGL(int, int)
{
    if (window_ == nullptr) return;
    (void)fviz_render_window_sync_external_surface_size(window_);
    FVizInteractionEvent event = makeBasicEvent(FVIZ_INTERACTION_RESIZE);
    fviz_render_window_get_size(window_, &event.width, &event.height);
    dispatch(&event);
    fviz_render_window_request_render(window_);
}

void FVizQtOpenGLWindow::paintGL()
{
    if (window_ == nullptr) return;
    (void)fviz_render_window_sync_external_surface_size(window_);
    (void)fviz_render_window_render(window_);
}

bool FVizQtOpenGLWindow::event(QEvent* event)
{
    if (event != nullptr && window_ != nullptr)
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave ||
            event->type() == QEvent::Expose)
        {
            FVizInteractionEvent interaction = makeBasicEvent(
                event->type() == QEvent::Enter ? FVIZ_INTERACTION_ENTER :
                event->type() == QEvent::Leave ? FVIZ_INTERACTION_LEAVE :
                                                FVIZ_INTERACTION_EXPOSE);
            dispatch(&interaction);
        }
    }

    const bool handled = QOpenGLWindow::event(event);
    if (event != nullptr)
    {
        if (event->type() == QEvent::Hide)
            timer_pump_.stop();
        else if (event->type() == QEvent::Expose && timer_pump_requested_ && isExposed() &&
                 !timer_pump_.isActive())
            timer_pump_.start();
    }
    return handled;
}

void FVizQtOpenGLWindow::mousePressEvent(QMouseEvent* event)
{
    double x = 0.0;
    double y = 0.0;
    eventPosition(event, &x, &y);
    FVizInteractionEvent interaction = makePointerEvent(
        FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, fvizMouseButton(event->button()), x, y);
    if (dispatch(&interaction))
    {
        if (interaction.button != FVIZ_MOUSE_BUTTON_NONE) setMouseGrabEnabled(true);
        event->accept();
    }
    else
    {
        QOpenGLWindow::mousePressEvent(event);
    }
}

void FVizQtOpenGLWindow::mouseReleaseEvent(QMouseEvent* event)
{
    double x = 0.0;
    double y = 0.0;
    eventPosition(event, &x, &y);
    FVizInteractionEvent interaction = makePointerEvent(
        FVIZ_INTERACTION_MOUSE_BUTTON_UP, fvizMouseButton(event->button()), x, y);
    const bool handled = dispatch(&interaction);
    if (event->buttons() == Qt::NoButton) setMouseGrabEnabled(false);
    if (handled) event->accept();
    else QOpenGLWindow::mouseReleaseEvent(event);
}

void FVizQtOpenGLWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    double x = 0.0;
    double y = 0.0;
    eventPosition(event, &x, &y);
    FVizInteractionEvent interaction = makePointerEvent(
        FVIZ_INTERACTION_DOUBLE_CLICK, fvizMouseButton(event->button()), x, y);
    if (dispatch(&interaction)) event->accept();
    else QOpenGLWindow::mouseDoubleClickEvent(event);
}

void FVizQtOpenGLWindow::mouseMoveEvent(QMouseEvent* event)
{
    double x = 0.0;
    double y = 0.0;
    eventPosition(event, &x, &y);
    FVizInteractionEvent interaction = makePointerEvent(
        FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, x, y);
    if (dispatch(&interaction)) event->accept();
    else QOpenGLWindow::mouseMoveEvent(event);
}

void FVizQtOpenGLWindow::wheelEvent(QWheelEvent* event)
{
    double x = 0.0;
    double y = 0.0;
    wheelPosition(event, &x, &y);
    FVizInteractionEvent interaction = makePointerEvent(
        FVIZ_INTERACTION_MOUSE_WHEEL, FVIZ_MOUSE_BUTTON_NONE, x, y);
    const QPoint delta = event->angleDelta();
    interaction.delta_x = delta.x();
    interaction.delta_y = delta.y();
    interaction.wheel_delta = static_cast<float>(delta.y()) / 120.0f;
    if (dispatch(&interaction)) event->accept();
    else QOpenGLWindow::wheelEvent(event);
}

void FVizQtOpenGLWindow::keyPressEvent(QKeyEvent* event)
{
    FVizInteractionEvent interaction = makeBasicEvent(FVIZ_INTERACTION_KEY_DOWN);
    interaction.key = event->key() == Qt::Key_Escape ? FVIZ_KEY_ESCAPE : event->key();
    bool handled = dispatch(&interaction);
    if (!event->text().isEmpty())
    {
        FVizInteractionEvent character = interaction;
        character.type = FVIZ_INTERACTION_CHAR;
        character.character = event->text().at(0).unicode();
        handled = dispatch(&character) || handled;
    }
    if (handled) event->accept();
    else QOpenGLWindow::keyPressEvent(event);
}

void FVizQtOpenGLWindow::keyReleaseEvent(QKeyEvent* event)
{
    FVizInteractionEvent interaction = makeBasicEvent(FVIZ_INTERACTION_KEY_UP);
    interaction.key = event->key() == Qt::Key_Escape ? FVIZ_KEY_ESCAPE : event->key();
    if (dispatch(&interaction)) event->accept();
    else QOpenGLWindow::keyReleaseEvent(event);
}

void FVizQtOpenGLWindow::focusInEvent(QFocusEvent* event)
{
    QOpenGLWindow::focusInEvent(event);
    FVizInteractionEvent interaction = makeBasicEvent(FVIZ_INTERACTION_FOCUS_IN);
    dispatch(&interaction);
}

void FVizQtOpenGLWindow::focusOutEvent(QFocusEvent* event)
{
    QOpenGLWindow::focusOutEvent(event);
    FVizInteractionEvent interaction = makeBasicEvent(FVIZ_INTERACTION_FOCUS_OUT);
    dispatch(&interaction);
    if (interactor() != nullptr) fviz_render_window_interactor_cancel_interaction(interactor());
    setMouseGrabEnabled(false);
}

FVizResult FVizQtOpenGLWindow::makeCurrentCallback(void* userData)
{
    auto* self = static_cast<FVizQtOpenGLWindow*>(userData);
    if (self == nullptr || self->context() == nullptr) return FVIZ_ERROR_INVALID_STATE;
    self->makeCurrent();
    return QOpenGLContext::currentContext() == self->context() ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult FVizQtOpenGLWindow::framebufferSizeCallback(void* userData, int* width, int* height)
{
    auto* self = static_cast<FVizQtOpenGLWindow*>(userData);
    if (self == nullptr || width == nullptr || height == nullptr) return FVIZ_ERROR_INVALID_ARGUMENT;
    const qreal dpr = self->devicePixelRatio();
    *width = std::max(1, static_cast<int>(std::lround(self->width() * dpr)));
    *height = std::max(1, static_cast<int>(std::lround(self->height() * dpr)));
    return FVIZ_OK;
}

uint32_t FVizQtOpenGLWindow::defaultFramebufferCallback(void* userData)
{
    auto* self = static_cast<FVizQtOpenGLWindow*>(userData);
    return self != nullptr ? static_cast<uint32_t>(self->defaultFramebufferObject()) : 0u;
}

void FVizQtOpenGLWindow::requestRenderCallback(void* userData)
{
    auto* self = static_cast<FVizQtOpenGLWindow*>(userData);
    if (self != nullptr) self->update();
}

FVizInteractionEvent FVizQtOpenGLWindow::makePointerEvent(
    FVizInteractionEventType type,
    FVizMouseButton button,
    double logicalX,
    double logicalY) const
{
    FVizInteractionEvent event = makeBasicEvent(type);
    const double dpr = static_cast<double>(devicePixelRatio());
    event.button = button;
    event.x = static_cast<int>(std::lround(logicalX * dpr));
    event.y = static_cast<int>(std::lround(logicalY * dpr));
    return event;
}

FVizInteractionEvent FVizQtOpenGLWindow::makeBasicEvent(FVizInteractionEventType type) const
{
    FVizInteractionEvent event{};
    event.type = type;
    event.timestamp_seconds = timestampSeconds();
    event.content_scale = static_cast<float>(devicePixelRatio());
    event.shift = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    event.control = (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    event.alt = (QGuiApplication::keyboardModifiers() & Qt::AltModifier) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    return event;
}

bool FVizQtOpenGLWindow::dispatch(FVizInteractionEvent* event)
{
    FVizRenderWindowInteractor* current = interactor();
    if (current == nullptr || event == nullptr) return false;
    const FVizBool handled = fviz_render_window_interactor_process_event(current, event);
    if (handled != FVIZ_FALSE && window_ != nullptr) fviz_render_window_request_render(window_);
    return handled != FVIZ_FALSE;
}

void FVizQtOpenGLWindow::pumpInteractorTimers()
{
    FVizRenderWindowInteractor* current = interactor();
    if (current == nullptr || !isExposed()) return;
    const FVizSize fired = fviz_render_window_interactor_process_timers(current, timestampSeconds());
    if (fired != 0u && window_ != nullptr) fviz_render_window_request_render(window_);
}

double FVizQtOpenGLWindow::timestampSeconds() const noexcept
{
    return static_cast<double>(elapsed_.nsecsElapsed()) / 1.0e9;
}
