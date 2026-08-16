// FEAViz C++ GUI feature test.
//
// Interactive window demonstrating the newest FEAViz features through the
// header-only C++ binding:
//   1. GPU ray-cast volume rendering (FVizVolumeMapper)
//   2. Dual depth peeling transparency
//   3. Multi-plane cutter + 3D iso-surface
//   4. FEA display groups
//
// Keys: 1-4 switch modes, 5 toggles depth peeling on the transparency mode,
// F fits the camera, Esc closes the window.

#include <FVizCpp/FVizCpp.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace fviz;

namespace {

struct AppState {
    Renderer renderer;
    Scene scene;
    TextActor2D panel;
    int mode = 1;
};

UnstructuredGrid build_hex_beam()
{
    UnstructuredGrid grid = UnstructuredGrid::create();
    const uint32_t nx = 4u, ny = 2u, nz = 2u;
    for (uint32_t z = 0u; z <= nz; ++z)
        for (uint32_t y = 0u; y <= ny; ++y)
            for (uint32_t x = 0u; x <= nx; ++x)
                grid.addPoint(Vec3((float)x, (float)y, (float)z));
    const auto pid = [&](uint32_t x, uint32_t y, uint32_t z) { return x + (nx + 1u) * (y + (ny + 1u) * z); };
    for (uint32_t z = 0u; z < nz; ++z)
        for (uint32_t y = 0u; y < ny; ++y)
            for (uint32_t x = 0u; x < nx; ++x)
            {
                const uint32_t ids[8] = {
                    pid(x, y, z), pid(x + 1u, y, z), pid(x + 1u, y + 1u, z), pid(x, y + 1u, z),
                    pid(x, y, z + 1u), pid(x + 1u, y, z + 1u), pid(x + 1u, y + 1u, z + 1u), pid(x, y + 1u, z + 1u)};
                grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, ids);
            }
    DataArray phi = DataArray::createFloat64();
    phi.resize(grid.pointCount());
    {
        const FVizVec3* p = fviz_points_data(fviz_unstructured_grid_points(grid.get()));
        for (FVizSize i = 0u; i < grid.pointCount(); ++i)
            phi.setComponent(i, 0u, (double)p[i].x + 0.5 * (double)p[i].y - 0.25 * (double)p[i].z);
    }
    grid.pointData().add("phi", phi.get());
    grid.pointData().setActive(FVIZ_ATTRIBUTE_SCALARS, "phi");
    grid.validate();
    return grid;
}

Actor make_colored_actor(PolyData& poly, const char* array_name, Vec3 color)
{
    Actor actor = Actor::create();
    actor.setPolyData(poly);
    if (array_name != nullptr)
    {
        actor.mapper().setArraySelection(FVIZ_ASSOCIATION_POINTS, array_name);
        actor.mapper().setScalarVisibility(true);
    }
    else
    {
        actor.setColor(color.x, color.y, color.z);
    }
    return actor;
}

void add_mode_volume(AppState& state)
{
    // 16^3 sphere-like density field.
    ImageData image = ImageData::create();
    const int64_t extent[6] = {0, 15, 0, 15, 0, 15};
    image.setExtent(extent);
    image.setOrigin(0.0, 0.0, 0.0);
    image.setSpacing(0.25, 0.25, 0.25);
    DataArray density = image.allocatePointScalars("Density", FVIZ_DATA_FLOAT32, 1u);
    {
        float* values = static_cast<float*>(density.data());
        for (FVizSize n = 0u; n < density.tupleCount(); ++n)
        {
            std::array<int64_t, 3> ijk{};
            fviz_image_data_point_ijk(image.get(), static_cast<FVizId>(n), ijk.data());
            const double dx = 4.0, dy = 4.0, dz = 4.0;
            values[n] = static_cast<float>(1.0 -
                (double)((ijk[0] - 8) * (ijk[0] - 8) / (dx * dx) +
                         (ijk[1] - 8) * (ijk[1] - 8) / (dy * dy) +
                         (ijk[2] - 8) * (ijk[2] - 8) / (dz * dz)));
        }
    }
    image.validate();

    VolumeMapper volume = VolumeMapper::create();
    volume.setImageData(image);
    volume.addColorPoint(-1.0f, 0.0f, 0.0f, 0.6f);
    volume.addColorPoint(0.0f, 0.1f, 0.7f, 1.0f);
    volume.addColorPoint(1.0f, 1.0f, 0.4f, 0.1f);
    volume.addOpacityPoint(-1.0f, 0.0f);
    volume.addOpacityPoint(0.4f, 0.25f);
    volume.addOpacityPoint(1.0f, 0.95f);
    volume.setScalarRange(-1.0f, 1.0f);
    volume.setSamplingStep(0.08f);
    volume.setShading(true);

    Actor actor = Actor::create();
    actor.setVolumeMapper(volume);
    state.scene.addActor(actor);
}

void add_mode_depth_peeling(AppState& state)
{
    // Two interpenetrating translucent slabs.
    PolyData red = PolyData::create();
    red.addPoint(Vec3(-1.2f, -0.7f, -0.5f));
    red.addPoint(Vec3(1.2f, -0.7f, -0.5f));
    red.addPoint(Vec3(1.2f, 0.7f, 0.5f));
    red.addPoint(Vec3(-1.2f, 0.7f, 0.5f));
    red.addQuad(0u, 1u, 2u, 3u);
    red.computeNormals();
    Actor red_actor = Actor::create();
    red_actor.setPolyData(red);
    red_actor.setColor(1.0f, 0.15f, 0.15f);
    red_actor.setOpacity(0.45f);
    state.scene.addActor(red_actor);

    PolyData blue = PolyData::create();
    blue.addPoint(Vec3(-0.6f, -1.1f, -0.6f));
    blue.addPoint(Vec3(0.6f, -1.1f, -0.6f));
    blue.addPoint(Vec3(0.6f, 1.1f, 0.6f));
    blue.addPoint(Vec3(-0.6f, 1.1f, 0.6f));
    blue.addQuad(0u, 1u, 2u, 3u);
    blue.computeNormals();
    Actor blue_actor = Actor::create();
    blue_actor.setPolyData(blue);
    blue_actor.setColor(0.15f, 0.35f, 1.0f);
    blue_actor.setOpacity(0.45f);
    state.scene.addActor(blue_actor);

    state.renderer.setTransparencyMode(FVIZ_TRANSPARENCY_DEPTH_PEELING);
}

void add_mode_cutter_iso(AppState& state)
{
    UnstructuredGrid beam = build_hex_beam();

    // Full surface tinted by the scalar field.
    PolyData surface = beam.extractSurface();
    surface.computeNormals();
    Actor surface_actor = make_colored_actor(surface, "phi", Vec3());
    surface_actor.setColor(0.62f, 0.68f, 0.75f);
    state.scene.addActor(surface_actor);

    // Multi-plane cutter.
    std::vector<Plane> planes = {
        Plane::fromPointNormal(Vec3(1.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)),
        Plane::fromPointNormal(Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f))};
    PolyData cut = beam.cutter(planes);
    Actor cut_actor = make_colored_actor(cut, "phi", Vec3());
    cut_actor.setOpacity(0.9f);
    state.scene.addActor(cut_actor);

    // Iso-surface inside the beam.
    PolyData iso = beam.isoSurface("phi", 2.5);
    iso.computeNormals();
    Actor iso_actor = Actor::create();
    iso_actor.setPolyData(iso);
    iso_actor.setColor(0.95f, 0.72f, 0.18f);
    state.scene.addActor(iso_actor);
}

void add_mode_display_groups(AppState& state)
{
    UnstructuredGrid beam = build_hex_beam();

    // Identity provenance labels for the display-group machinery.
    DataArray point_ids = DataArray::createUint64();
    point_ids.resize(beam.pointCount());
    for (FVizSize i = 0u; i < beam.pointCount(); ++i) point_ids.setComponent(i, 0u, (double)i);
    DataArray cell_ids = DataArray::createUint64();
    cell_ids.resize(beam.cellCount());
    for (FVizSize i = 0u; i < beam.cellCount(); ++i) cell_ids.setComponent(i, 0u, (double)i);
    beam.pointData().add(FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME, point_ids.get());
    beam.cellData().add(FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME, cell_ids.get());

    // One group per material half; extract two surfaces and color each.
    fea::DisplayGroup first_half = fea::DisplayGroup::create("LowerCells");
    std::vector<uint64_t> lower;
    for (uint32_t y = 0u; y < 2u; ++y)
        for (uint32_t x = 0u; x < 2u; ++x) lower.push_back((uint64_t)(y * 2u + x));
    first_half.setElements(lower);

    fea::DisplayGroup second_half = fea::DisplayGroup::create("UpperCells");
    std::vector<uint64_t> upper;
    for (uint32_t y = 2u; y < 4u; ++y)
        for (uint32_t x = 0u; x < 2u; ++x) upper.push_back((uint64_t)(y * 2u + x));
    second_half.setElements(upper);

    PolyData full_surface = beam.extractSurface();
    PolyData first_surface = first_half.applyToSurface(full_surface);
    first_surface.computeNormals();
    Actor first_actor = Actor::create();
    first_actor.setPolyData(first_surface);
    first_actor.setColor(0.20f, 0.72f, 0.42f);
    state.scene.addActor(first_actor);

    PolyData second_surface = second_half.applyToSurface(full_surface);
    second_surface.computeNormals();
    Actor second_actor = Actor::create();
    second_actor.setPolyData(second_surface);
    second_actor.setColor(0.85f, 0.35f, 0.28f);
    state.scene.addActor(second_actor);
}

const char* mode_title(int mode)
{
    switch (mode)
    {
        case 1: return "1 - Volume rendering (ray cast)";
        case 2: return "2 - Dual depth peeling";
        case 3: return "3 - Cutter + iso-surface";
        case 4: return "4 - FEA display groups";
        default: return "";
    }
}

void rebuild_scene(AppState& state)
{
    state.scene.clear();
    state.renderer.setTransparencyMode(FVIZ_TRANSPARENCY_SORTED);
    switch (state.mode)
    {
        case 1: add_mode_volume(state); break;
        case 2: add_mode_depth_peeling(state); break;
        case 3: add_mode_cutter_iso(state); break;
        case 4: add_mode_display_groups(state); break;
        default: break;
    }
    state.renderer.fitCamera(1.5f);
    state.panel.setText(mode_title(state.mode));
}

bool handle_key(AppState* state, const FVizInteractionEvent& event)
{
    if (event.type != FVIZ_INTERACTION_KEY_DOWN) return false;
    const int key = event.key;
    if (key >= '1' && key <= '4')
    {
        state->mode = key - '0';
        rebuild_scene(*state);
        return true;
    }
    if (key == 'F' || key == 'f')
    {
        state->renderer.fitCamera(1.5f);
        return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    if (!RenderWindow::supported())
    {
        std::fprintf(stderr, "FEAViz render window is not supported on this platform\n");
        return 1;
    }

    Renderer renderer = Renderer::create();
    Scene scene = renderer.scene();
    renderer.setBackground(0.08f, 0.09f, 0.12f);

    TextActor2D panel = TextActor2D::create();
    panel.setPosition(0.02f, 0.95f);
    panel.setCoordinateSystem(FVIZ_TEXT_COORDINATE_NORMALIZED_VIEWPORT);
    panel.setText("FEAViz C++ feature test - press 1-4");
    renderer.addTextActor2D(panel);

    if (smoke)
    {
        // Headless verification: render each mode offscreen once through the
        // C++ API and count colored pixels, then exit.
        RenderWindow window = RenderWindow::createOffscreen(320, 240);
        window.setRenderer(renderer);
        window.setFxaa(false);
        AppState state;
        state.renderer = renderer;
        state.scene = scene;
        state.panel = panel;
        for (int mode = 1; mode <= 4; ++mode)
        {
            state.mode = mode;
            rebuild_scene(state);
            window.render();
            const FVizSize bytes = 320u * 240u * 4u;
            std::vector<uint8_t> pixels(bytes);
            window.readRgba8(pixels.data(), bytes);
            FVizSize colored = 0u;
            for (FVizSize i = 0u; i < bytes; i += 4u)
                if (pixels[i] > 8u || pixels[i + 1u] > 8u || pixels[i + 2u] > 8u) ++colored;
            std::printf("mode %d: colored_pixels=%llu\n", mode, (unsigned long long)colored);
            if (colored < 50u)
            {
                std::fprintf(stderr, "mode %d produced too few visible pixels\n", mode);
                return 1;
            }
        }
        std::printf("FEAViz C++ feature test smoke passed\n");
        return 0;
    }

    RenderWindow window = RenderWindow::create(1280, 800, "FEAViz C++ Feature Test");
    window.setRenderer(renderer);

    AppState state;
    state.renderer = renderer;
    state.scene = scene;
    state.panel = panel;
    rebuild_scene(state);

    // Route keys through the interactor.
    RenderWindowInteractor interactor = window.interactor();
    interactor.setEventCallback([&state](const FVizInteractionEvent& event) {
        return handle_key(&state, event);
    });

    std::printf("FEAViz C++ feature test\n");
    std::printf("  Keys: 1 volume | 2 depth peeling | 3 cutter/iso | 4 display groups | F fit | Esc close\n");
    window.run();
    return 0;
}
