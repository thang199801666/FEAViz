#ifndef FVIZ_INTEGRATIONS_QT_FVIZ_QT_WIDGET_H
#define FVIZ_INTEGRATIONS_QT_FVIZ_QT_WIDGET_H

#include <QElapsedTimer>
#include <QTimer>

class QEvent;
class QHideEvent;
class QPaintEngine;
class QResizeEvent;
class QShowEvent;
class QFocusEvent;
#include <QWidget>

#include <FViz/Rendering/FVizRendererWidget.h>

/*
 * Thin Qt Widgets adapter for the native FEAViz renderer.
 *
 * Design rules:
 *  - FEAViz remains a C17 library and has no Qt dependency.
 *  - On Windows, this QWidget owns a native host HWND. FEAViz creates its own
 *    WGL child HWND below it and receives mouse/keyboard messages directly.
 *  - Qt owns the application message loop. Never call
 *    fviz_renderer_widget_start()/process_events() for this embedded widget.
 *  - Interactor timers are serviced by a small QTimer without peeking the
 *    process-wide Win32 message queue.
 */
class FVizQtWidget final : public QWidget
{
public:
    explicit FVizQtWidget(
        QWidget* parent = nullptr,
        const FVizRenderWindowOptions* options = nullptr);
    ~FVizQtWidget() override;

    FVizQtWidget(const FVizQtWidget&) = delete;
    FVizQtWidget& operator=(const FVizQtWidget&) = delete;

    bool isValid() const noexcept;
    FVizResult initializationResult() const noexcept;

    FVizRendererWidget* rendererWidget() noexcept;
    FVizRenderWindow* renderWindow() noexcept;
    FVizRenderer* renderer() noexcept;
    FVizRenderWindowInteractor* interactor() noexcept;

    void* nativeRenderHandle() noexcept;
    void* nativeHostHandle() noexcept;

    FVizResult syncToHost();
    FVizResult renderNow();
    FVizResult requestRender();
    bool renderPending() const noexcept;
    FVizResult renderIfPending();
    FVizResult addObserver(
        FVizObject* object,
        FVizEventId eventId,
        float priority,
        FVizObserverCallbackFn callback,
        void* clientData,
        FVizObserverTag* outTag);
    FVizResult addCommandObserver(
        FVizObject* object,
        FVizEventId eventId,
        float priority,
        FVizCommand* command,
        FVizObserverTag* outTag);
    FVizResult removeObserver(FVizObject* object, FVizObserverTag tag);

    void setInteractorTimerPumpEnabled(bool enabled);
    bool interactorTimerPumpEnabled() const noexcept;
    void setInteractorTimerPumpInterval(int milliseconds);
    int interactorTimerPumpInterval() const noexcept;

protected:
    bool event(QEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    FVizResult ensureCurrentHost();
    void pumpInteractorTimers();

    FVizRendererWidget* widget_ = nullptr;
    FVizResult initialization_result_ = FVIZ_ERROR_INVALID_STATE;
    void* host_handle_ = nullptr;
    QTimer timer_pump_;
    QElapsedTimer elapsed_;
    bool timer_pump_requested_ = true;
};

#endif /* FVIZ_INTEGRATIONS_QT_FVIZ_QT_WIDGET_H */
