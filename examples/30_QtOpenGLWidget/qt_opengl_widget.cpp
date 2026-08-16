#include <QApplication>
#include <QCoreApplication>

#include <FViz/FViz.h>
#include <FVizQtOpenGLWidget.h>

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    options.multisamples = 4u;
    options.fxaa = FVIZ_TRUE;

    FVizQtOpenGLWidget view(nullptr, &options);
    FVizCubeSource* cube = nullptr;
    FVizActor* actor = nullptr;

    view.resize(1200, 820);
    view.setWindowTitle("FEAViz - Qt-owned QOpenGLWidget framebuffer");
    view.show();
    QCoreApplication::processEvents();
    if (!view.isValid()) return 2;

    if (fviz_cube_source_create(&cube) != FVIZ_OK ||
        fviz_cube_source_set_lengths(cube, 2.0, 1.4, 1.0) != FVIZ_OK ||
        fviz_cube_source_update(cube) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_cube_source_output(cube)) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(view.renderer()), actor) != FVIZ_OK)
    {
        fviz_release(actor);
        fviz_release(cube);
        return 3;
    }

    fviz_actor_set_color(actor, 0.17f, 0.66f, 0.93f);
    fviz_renderer_set_background(view.renderer(), 0.05f, 0.06f, 0.08f);
    fviz_renderer_fit_camera(view.renderer(), 1.35f);
    fviz_release(actor);
    fviz_release(cube);

    (void)view.requestRender();
    return application.exec();
}
