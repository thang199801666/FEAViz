#ifndef FVIZ_INTEGRATIONS_QT_FVIZ_QT_OPENGL_WINDOW_H
#define FVIZ_INTEGRATIONS_QT_FVIZ_QT_OPENGL_WINDOW_H

#include <QElapsedTimer>
#include <QOpenGLWindow>
#include <QTimer>

class QEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

#include <FViz/Rendering/FVizRendering.h>

/*
 * Qt-owned OpenGL context/surface integration for FEAViz.
 *
 * Unlike FVizQtWindow/FVizQtWidget, this adapter does not create a FEAViz
 * child HWND. QOpenGLWindow owns the context, swap/present lifecycle and the
 * default framebuffer. FEAViz renders into that host-provided target from
 * paintGL(), which is the integration mode closest to QVTKOpenGLNativeWidget.
 */
class FVizQtOpenGLWindow final : public QOpenGLWindow
{
public:
    explicit FVizQtOpenGLWindow(
        QWindow* parent = nullptr,
        const FVizRenderWindowOptions* options = nullptr);
    ~FVizQtOpenGLWindow() override;

    FVizQtOpenGLWindow(const FVizQtOpenGLWindow&) = delete;
    FVizQtOpenGLWindow& operator=(const FVizQtOpenGLWindow&) = delete;

    bool isValid() const noexcept;
    FVizResult initializationResult() const noexcept;

    FVizRenderWindow* renderWindow() noexcept;
    FVizRenderer* renderer() noexcept;
    FVizRenderWindowInteractor* interactor() noexcept;

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
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    static FVizResult makeCurrentCallback(void* userData);
    static FVizResult framebufferSizeCallback(void* userData, int* width, int* height);
    static uint32_t defaultFramebufferCallback(void* userData);
    static void requestRenderCallback(void* userData);

    FVizInteractionEvent makePointerEvent(
        FVizInteractionEventType type,
        FVizMouseButton button,
        double logicalX,
        double logicalY) const;
    FVizInteractionEvent makeBasicEvent(FVizInteractionEventType type) const;
    bool dispatch(FVizInteractionEvent* event);
    void pumpInteractorTimers();
    double timestampSeconds() const noexcept;

    FVizRenderWindow* window_ = nullptr;
    FVizRenderWindowOptions options_{};
    FVizResult initialization_result_ = FVIZ_ERROR_INVALID_STATE;
    QTimer timer_pump_;
    QElapsedTimer elapsed_;
    bool timer_pump_requested_ = true;
};

#endif /* FVIZ_INTEGRATIONS_QT_FVIZ_QT_OPENGL_WINDOW_H */
