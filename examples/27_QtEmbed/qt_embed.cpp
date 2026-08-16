#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>

#include <FViz/FViz.h>
#include <FVizQtWidget.h>

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QMainWindow main_window;
    auto* view = new FVizQtWidget(&main_window);
    FVizCubeSource* cube = nullptr;
    FVizActor* actor = nullptr;

    if (!view->isValid())
    {
        delete view;
        return 2;
    }

    if (fviz_cube_source_create(&cube) != FVIZ_OK ||
        fviz_cube_source_set_lengths(cube, 2.0, 1.4, 1.0) != FVIZ_OK ||
        fviz_cube_source_update(cube) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_cube_source_output(cube)) != FVIZ_OK ||
        fviz_renderer_widget_add_actor(view->rendererWidget(), actor) != FVIZ_OK)
    {
        fviz_release(actor);
        fviz_release(cube);
        delete view;
        return 3;
    }

    fviz_actor_set_color(actor, 0.18f, 0.63f, 0.94f);
    fviz_renderer_set_background(view->renderer(), 0.055f, 0.065f, 0.085f);
    fviz_renderer_fit_camera(view->renderer(), 1.35f);
    (void)view->renderNow();

    /* The actor/source may be released after attaching; FEAViz owns the
     * references needed by the scene/pipeline. */
    fviz_release(actor);
    fviz_release(cube);

    main_window.setCentralWidget(view);
    main_window.statusBar()->showMessage(
        "FEAViz child HWND embedded in QWidget | LMB orbit | MMB pan | wheel zoom");
    main_window.resize(1200, 820);
    main_window.setWindowTitle("FEAViz - Embedded in Qt Widgets");
    main_window.show();
    return application.exec();
}
