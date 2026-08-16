// FEAViz C++ binding - minimal viewer example.
//
// Demonstrates the ergonomic C++17 API: load a VTU result, extract a surface,
// color it by a scalar field, and assemble a renderer. No manual refcounting.
//
// Build: link FEAViz::Core, add bindings/cpp/include to the include path.

#include <FVizCpp/FVizCpp.hpp>

#include <cstdio>

using namespace fviz;

int main(int argc, char** argv)
{
    try
    {
        UnstructuredGrid grid = argc > 1 ? readVtu(argv[1]) : readVtu("part.vtu");

        FVizPolyData* raw_surface = nullptr;
        fviz_unstructured_grid_extract_geometry(grid.get(), &raw_surface);
        PolyData surface(raw_surface);
        surface.computeNormals();

        // Pick the first scalar field for coloring.
        AttributeSet point_data = surface.pointData();
        const char* field = point_data.count() > 0u ? point_data.nameAt(0u) : "stress";
        double minimum = 0.0;
        double maximum = 1.0;
        DataArray first = point_data.get(field);
        if (first.get() != nullptr)
            first.range(0, minimum, maximum);

        LookupTable lut = LookupTable::create(256u);
        lut.setRange((float)minimum, (float)maximum);
        lut.buildPreset(FVIZ_COLOR_MAP_RAINBOW);

        Mapper mapper = Mapper::create();
        mapper.setPolyData(surface);
        mapper.setLookupTable(lut);
        mapper.setArraySelection(FVIZ_ASSOCIATION_POINTS, field);
        mapper.setScalarVisibility(true);
        mapper.setScalarRange((float)minimum, (float)maximum);

        Actor actor = Actor::create();
        actor.setMapper(mapper);
        actor.setEdgeVisibility(true);
        actor.setEdgeColor(0.05f, 0.08f, 0.12f);

        Scene scene = Scene::create();
        scene.addActor(actor);

        Renderer renderer = Renderer::create();
        renderer.setScene(scene);
        renderer.setBackground(Vec3(0.04f, 0.05f, 0.07f));
        renderer.setGradientBackground(true);
        renderer.fitCamera(1.2f);

        std::printf("Loaded %zu points, %zu cells; coloring by '%s' [%.3f .. %.3f]\n",
            (size_t)grid.pointCount(), (size_t)grid.cellCount(), field, minimum, maximum);
        return 0;
    }
    catch (const Error& error)
    {
        std::fprintf(stderr, "FEAViz error: %s\n", error.what());
        return 1;
    }
}
