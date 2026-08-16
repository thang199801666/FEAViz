#include <QCoreApplication>
#include <QGuiApplication>

#include <FViz/FViz.h>
#include <FVizQtOpenGLWindow.h>

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    options.multisamples = 4u;
    options.fxaa = FVIZ_TRUE;
    FVizQtOpenGLWindow view(nullptr, &options);
    FVizCubeSource* cube = nullptr;
    FVizActor* actor = nullptr;

    view.resize(1200, 820);
    view.setTitle("FEAViz - Qt-owned OpenGL context");
    view.show();

    /* QOpenGLWindow creates the FEAViz render window lazily in initializeGL()
       once the window is exposed. Process the initial expose/paint before
       populating the FEAViz scene. */
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

    fviz_actor_set_color(actor, 0.15f, 0.67f, 0.92f);
    fviz_renderer_set_background(view.renderer(), 0.05f, 0.06f, 0.08f);
    fviz_renderer_fit_camera(view.renderer(), 1.35f);
    fviz_release(actor);
    fviz_release(cube);

    (void)view.requestRender();
    return application.exec();
}
