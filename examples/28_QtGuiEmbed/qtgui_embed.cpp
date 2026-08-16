#include <QGuiApplication>

#include <FViz/FViz.h>
#include <FVizQtWindow.h>

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    FVizQtWindow view;
    FVizCubeSource* cube = nullptr;
    FVizActor* actor = nullptr;

    if (!view.isValid()) return 2;

    if (fviz_cube_source_create(&cube) != FVIZ_OK ||
        fviz_cube_source_set_lengths(cube, 2.0, 1.4, 1.0) != FVIZ_OK ||
        fviz_cube_source_update(cube) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_cube_source_output(cube)) != FVIZ_OK ||
        fviz_renderer_widget_add_actor(view.rendererWidget(), actor) != FVIZ_OK)
    {
        fviz_release(actor);
        fviz_release(cube);
        return 3;
    }

    fviz_actor_set_color(actor, 0.18f, 0.63f, 0.94f);
    fviz_renderer_set_background(view.renderer(), 0.055f, 0.065f, 0.085f);
    fviz_renderer_fit_camera(view.renderer(), 1.35f);

    fviz_release(actor);
    fviz_release(cube);

    view.resize(1200, 820);
    view.setTitle("FEAViz - Embedded in QtGui QWindow");
    view.show();
    (void)view.requestRender();
    return application.exec();
}
