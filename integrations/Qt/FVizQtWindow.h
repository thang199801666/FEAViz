#ifndef FVIZ_INTEGRATIONS_QT_FVIZ_QT_WINDOW_H
#define FVIZ_INTEGRATIONS_QT_FVIZ_QT_WINDOW_H

#include <QElapsedTimer>
#include <QTimer>
#include <QWindow>

class QEvent;
class QExposeEvent;
class QFocusEvent;
class QResizeEvent;

#include <FViz/Rendering/FVizRendererWidget.h>

/*
 * QtGui-only native host for FEAViz.
 *
 * This adapter intentionally derives from QWindow rather than QOpenGLWindow:
 * FEAViz owns a WGL child HWND and its GL context while Qt owns the application
 * event loop and the top-level/native host lifetime.  The same QWindow can be
 * embedded into a Qt Widgets layout with QWidget::createWindowContainer().
 */
class FVizQtWindow final : public QWindow
{
public:
    explicit FVizQtWindow(
        QWindow* parent = nullptr,
        const FVizRenderWindowOptions* options = nullptr);
    ~FVizQtWindow() override;

    FVizQtWindow(const FVizQtWindow&) = delete;
    FVizQtWindow& operator=(const FVizQtWindow&) = delete;

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
    void resizeEvent(QResizeEvent* event) override;
    void exposeEvent(QExposeEvent* event) override;
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

#endif /* FVIZ_INTEGRATIONS_QT_FVIZ_QT_WINDOW_H */
