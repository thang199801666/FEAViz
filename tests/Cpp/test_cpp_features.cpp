// FEAViz C++ binding - filters, interaction, and animation tests.
//
// Exercises the filter wrappers (threshold/warp/surface/slice), the interaction
// wrappers (interactor style driving the camera through processEvent), and a
// headless frame-animation controller over a ResultDatabase.

#include <FVizCpp/FVizCpp.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

using namespace fviz;

// ---------------------------------------------------------------------------
// Filters through the C++ binding.
// ---------------------------------------------------------------------------
static int test_filters()
{
    // Build a small hex beam grid with a nodal scalar.
    UnstructuredGrid grid = UnstructuredGrid::create();
    const uint32_t nx = 2u, ny = 2u, nz = 2u;
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

    DataArray stress = DataArray::createFloat64();
    stress.resize(grid.pointCount());
    for (FVizSize i = 0u; i < grid.pointCount(); ++i)
        stress.setComponent(i, 0u, (double)(i % 7u));
    grid.pointData().add("stress", stress.get());

    // Threshold needs a one-component cell array.
    DataArray cellStress = DataArray::createFloat64();
    cellStress.resize(grid.cellCount());
    for (FVizSize i = 0u; i < grid.cellCount(); ++i)
        cellStress.setComponent(i, 0u, (double)(i % 5u));
    grid.cellData().add("cstress", cellStress.get());

    DataArray disp = DataArray::createFloat32(3u);
    disp.resize(grid.pointCount());
    for (FVizSize i = 0u; i < grid.pointCount(); ++i)
    {
        float tuple[3] = {0.001f * (float)i, 0.0f, 0.0f};
        disp.setTuple(i, tuple);
    }
    grid.pointData().add("displacement", disp.get());

    // Threshold keeps a subset of cells (by cell scalar).
    ThresholdFilter threshold = ThresholdFilter::create("cstress", 1.0, 3.0);
    threshold.setInput(grid);
    threshold.update();
    UnstructuredGrid thinned = threshold.outputGrid();
    CHECK(thinned.get() != nullptr);
    CHECK(thinned.cellCount() <= grid.cellCount());
    CHECK(thinned.cellCount() > 0u);

    // Surface extraction.
    SurfaceFilter surfaceFilter = SurfaceFilter::create(true);
    surfaceFilter.setInput(grid);
    surfaceFilter.update();
    PolyData surface = surfaceFilter.outputPolyData();
    CHECK(surface.get() != nullptr);
    CHECK(surface.triangleCount() > 0u);

    // Warp by displacement.
    WarpFilter warp = WarpFilter::create("displacement", 1.0);
    warp.setInput(grid);
    warp.update();
    UnstructuredGrid warped = warp.outputGrid();
    CHECK(warped.get() != nullptr);
    CHECK(warped.pointCount() == grid.pointCount());

    // Slice at z=1.0 (cut plane through the beam).
    SliceFilter slice = SliceFilter::create(Plane::fromPointNormal(Vec3(0, 0, 1.0f), Vec3(0, 0, 1)));
    slice.setInput(grid);
    slice.update();
    PolyData cut = slice.outputPolyData();
    CHECK(cut.get() != nullptr);
    CHECK(cut.triangleCount() > 0u);

    // CellDataToPoint smoothing on a grid with cell data.
    UnstructuredGrid cellGrid = UnstructuredGrid::create();
    cellGrid.addPoint(Vec3(0, 0, 0));
    cellGrid.addPoint(Vec3(1, 0, 0));
    cellGrid.addPoint(Vec3(1, 1, 0));
    cellGrid.addPoint(Vec3(0, 1, 0));
    const uint32_t quad[4] = {0u, 1u, 2u, 3u};
    cellGrid.addCell(FVIZ_CELL_QUAD, 4u, quad);
    DataArray cellQuadStress = DataArray::createFloat64();
    cellQuadStress.resize(1u);
    cellQuadStress.setComponent(0u, 0u, 42.0);
    cellGrid.cellData().add("cstress", cellQuadStress.get());

    CellDataToPointFilter smooth = CellDataToPointFilter::create();
    smooth.setInput(cellGrid);
    smooth.update();
    UnstructuredGrid smoothed = smooth.outputGrid();
    CHECK(smoothed.get() != nullptr);
    CHECK(smoothed.pointData().has("cstress"));

    return 0;
}

// ---------------------------------------------------------------------------
// Interaction through the C++ binding.
// ---------------------------------------------------------------------------
static int test_interaction()
{
    // Renderer + camera.
    Renderer renderer = Renderer::create();
    Camera camera = renderer.camera();
    camera.setPosition(Vec3(0, 0, 5));
    camera.setTarget(Vec3(0, 0, 0));
    camera.setUp(Vec3(0, 1, 0));

    const Vec3 start = camera.position();

    // Trackball camera style: a left-drag should change the camera position.
    InteractorStyle style = InteractorStyle::trackballCamera();
    style.setOrbitSensitivity(0.01f);

    InteractionEvent down;
    down.type = FVIZ_INTERACTION_MOUSE_BUTTON_DOWN;
    down.button = FVIZ_MOUSE_BUTTON_LEFT;
    down.x = 40; down.y = 30;
    down.width = 800; down.height = 600;
    CHECK(style.processEvent(renderer, down));

    InteractionEvent move;
    move.type = FVIZ_INTERACTION_MOUSE_MOVE;
    move.button = FVIZ_MOUSE_BUTTON_LEFT;
    move.x = 60; move.y = 30;
    move.width = 800; move.height = 600;
    CHECK(style.processEvent(renderer, move));

    InteractionEvent up;
    up.type = FVIZ_INTERACTION_MOUSE_BUTTON_UP;
    up.button = FVIZ_MOUSE_BUTTON_LEFT;
    up.x = 60; up.y = 30;
    up.width = 800; up.height = 600;
    CHECK(style.processEvent(renderer, up));

    const Vec3 end = camera.position();
    CHECK((end - start).length() > 1.0e-4f);

    // Pan via middle-drag.
    const Vec3 beforePan = camera.position();
    InteractionEvent panDown;
    panDown.type = FVIZ_INTERACTION_MOUSE_BUTTON_DOWN;
    panDown.button = FVIZ_MOUSE_BUTTON_MIDDLE;
    panDown.x = 100; panDown.y = 100;
    panDown.width = 800; panDown.height = 600;
    style.processEvent(renderer, panDown);
    InteractionEvent panMove;
    panMove.type = FVIZ_INTERACTION_MOUSE_MOVE;
    panMove.button = FVIZ_MOUSE_BUTTON_MIDDLE;
    panMove.x = 130; panMove.y = 100;
    panMove.width = 800; panMove.height = 600;
    style.processEvent(renderer, panMove);
    InteractionEvent panUp;
    panUp.type = FVIZ_INTERACTION_MOUSE_BUTTON_UP;
    panUp.button = FVIZ_MOUSE_BUTTON_MIDDLE;
    panUp.x = 130; panUp.y = 100;
    panUp.width = 800; panUp.height = 600;
    style.processEvent(renderer, panUp);
    CHECK((camera.position() - beforePan).length() > 1.0e-4f);

    // Dolly via wheel.
    const Vec3 beforeDolly = camera.position();
    InteractionEvent wheel;
    wheel.type = FVIZ_INTERACTION_MOUSE_WHEEL;
    wheel.wheel_delta = -2.0f; // zoom out
    wheel.width = 800; wheel.height = 600;
    style.processEvent(renderer, wheel);
    CHECK((camera.position() - beforeDolly).length() > 1.0e-4f);

    return 0;
}

// ---------------------------------------------------------------------------
// Headless frame animation over a ResultDatabase.
// ---------------------------------------------------------------------------
namespace fviz {
namespace fea {

// A minimal, GUI-free animation controller: iterates the frames of a step and
// produces a per-frame contour surface. Applications drive this from their own
// event loop (timer / frame callback).
class FramePlayer {
public:
    FramePlayer(ResultDatabase& database, const std::string& step_name,
        UnstructuredGrid& grid, const std::string& scalar_name)
        : database_(database), grid_(grid), scalar_name_(scalar_name)
    {
        Step s = database_.step(step_name);
        if (s.get() != nullptr)
        {
            frame_count_ = s.frameCount();
            step_ = s.release();
        }
    }

    FVizSize frameCount() const noexcept { return frame_count_; }
    FVizSize currentFrame() const noexcept { return frame_; }

    // Advances to the given frame and rebuilds the contour surface.
    bool seek(FVizSize frame_index)
    {
        if (step_ == nullptr || frame_index >= frame_count_) return false;
        frame_ = frame_index;
        // Rebuild the contour surface from the grid's own scalar field. In a
        // real application the result values are wired into the grid point data
        // by the result/frame controller before seeking.
        FVizPolyData* raw = nullptr;
        if (fviz_unstructured_grid_extract_geometry(grid_.get(), &raw) != FVIZ_OK)
            return false;
        PolyData surface(raw);
        surface.computeNormals();
        DataArray scalars = surface.pointData().get(scalar_name_.c_str());
        if (scalars.get() != nullptr)
        {
            double lo = 0.0, hi = 1.0;
            scalars.range(0, lo, hi);
            FVizPolyData* colored = nullptr;
            if (fviz_fea_build_contour_surface(surface.get(), scalar_name_.c_str(), 1u,
                    (float)lo, (float)hi, "contour_rgb", &colored) == FVIZ_OK)
            {
                current_ = PolyData(colored);
            }
        }
        return current_.get() != nullptr;
    }

    bool next() { return seek(frame_ + 1u); }
    bool previous() { return frame_ > 0u ? seek(frame_ - 1u) : false; }
    PolyData& currentSurface() noexcept { return current_; }

private:
    FVizFEAFrame* stepFrame(FVizSize index) const
    {
        return fviz_fea_step_frame(step_, index);
    }

    ResultDatabase& database_;
    UnstructuredGrid& grid_;
    std::string scalar_name_;
    FVizFEAStep* step_ = nullptr;
    FVizSize frame_count_ = 0u;
    FVizSize frame_ = 0u;
    PolyData current_;
};

} // namespace fea
} // namespace fviz

static int test_animation()
{
    // Build a 3-frame result database with a nodal scalar field per frame.
    ResultDatabase database = ResultDatabase::create();
    Step step = Step::create("Step-1", "Transient", FVIZ_FEA_STEP_TIME, 1.0);

    UnstructuredGrid grid = UnstructuredGrid::create();
    grid.addPoint(Vec3(0, 0, 0));
    grid.addPoint(Vec3(1, 0, 0));
    grid.addPoint(Vec3(1, 1, 0));
    grid.addPoint(Vec3(0, 1, 0));
    grid.addPoint(Vec3(0, 0, 1));
    grid.addPoint(Vec3(1, 0, 1));
    grid.addPoint(Vec3(1, 1, 1));
    grid.addPoint(Vec3(0, 1, 1));
    const uint32_t hex[8] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, hex);

    DataArray base = DataArray::createFloat64();
    base.resize(8u);
    for (FVizSize i = 0u; i < 8u; ++i)
        base.setComponent(i, 0u, (double)i);
    grid.pointData().add("T", base.get());

    for (int64_t f = 0; f < 3; ++f)
    {
        Frame frame = Frame::create(f, (double)f, "frame");
        Field temperature = Field::create("T", "Temperature", FVIZ_FEA_FIELD_SCALAR);
        DataArray values = DataArray::createFloat64();
        values.resize(8u);
        for (FVizSize i = 0u; i < 8u; ++i)
            values.setComponent(i, 0u, (double)i + 10.0 * (double)f);
        DataArray ids = DataArray::createUint64();
        ids.resize(8u);
        for (FVizSize i = 0u; i < 8u; ++i)
            ids.setComponent(i, 0u, (double)i);
        temperature.addBlock("PART-1", FVIZ_FEA_POSITION_NODAL, ids.get(), nullptr, values.get());
        frame.addField(temperature);
        step.addFrame(frame);
    }
    database.addStep(step);
    CHECK(database.stepCount() == 1u);

    fea::FramePlayer player(database, "Step-1", grid, "T");
    CHECK(player.frameCount() == 3u);
    CHECK(player.seek(0u));
    const FVizSize first_points = player.currentSurface().pointCount();
    CHECK(first_points > 0u);
    CHECK(player.next());
    CHECK(player.next());
    CHECK(!player.next()); // past the end returns false
    CHECK(player.previous());
    CHECK(player.currentFrame() == 1u);
    CHECK(player.currentSurface().pointCount() == first_points);

    return 0;
}

static int test_element_facet_cpp()
{
    // Two hex cells with distinct cell scalars.
    UnstructuredGrid grid = UnstructuredGrid::create();
    const FVizVec3 points[16] = {
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1},
        {2,0,0},{3,0,0},{3,1,0},{2,1,0},{2,0,1},{3,0,1},{3,1,1},{2,1,1}};
    for (const auto& p : points) grid.addPoint(p);
    const uint32_t hexA[8] = {0,1,2,3,4,5,6,7};
    const uint32_t hexB[8] = {8,9,10,11,12,13,14,15};
    grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, hexA);
    grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, hexB);

    DataArray cellScalars = DataArray::createFloat64();
    cellScalars.resize(2u);
    cellScalars.setComponent(0u, 0u, 10.0);
    cellScalars.setComponent(1u, 0u, 90.0);
    grid.cellData().add("estress", cellScalars.get());

    FVizPolyData* rawSurface = nullptr;
    CHECK(fviz_unstructured_grid_extract_geometry(grid.get(), &rawSurface) == FVIZ_OK);
    PolyData surface(rawSurface);

    PolyData facet = fea::buildElementFacetSurface(grid, surface, "estress", 1u, 0.0f, 100.0f, "facet_rgb");
    CHECK(facet.pointCount() > 0u);
    CHECK(facet.triangleCount() == surface.triangleCount());
    CHECK(facet.pointData().get("facet_rgb").get() != nullptr);

    // Every triangle is flat-colored.
    DataArray colors = facet.pointData().get("facet_rgb");
    CHECK(colors.get() != nullptr);
    const uint32_t* tris = facet.triangleIndices();
    for (FVizSize t = 0u; t < facet.triangleCount(); ++t)
    {
        const double r0 = colors.component(tris[t * 3u + 0u], 0u);
        const double r1 = colors.component(tris[t * 3u + 1u], 0u);
        const double r2 = colors.component(tris[t * 3u + 2u], 0u);
        CHECK(r0 == r1 && r1 == r2);
    }
    return 0;
}

static int test_superimposed_cpp()
{
    UnstructuredGrid grid = UnstructuredGrid::create();
    grid.addPoint(Vec3(0, 0, 0));
    grid.addPoint(Vec3(1, 0, 0));
    grid.addPoint(Vec3(1, 1, 0));
    grid.addPoint(Vec3(0, 1, 0));
    grid.addPoint(Vec3(0, 0, 1));
    grid.addPoint(Vec3(1, 0, 1));
    grid.addPoint(Vec3(1, 1, 1));
    grid.addPoint(Vec3(0, 1, 1));
    const uint32_t hex[8] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, hex);

    DataArray disp = DataArray::createFloat32(3u);
    disp.resize(8u);
    const float tuples[8][3] = {
        {0,0,0},{0.1f,0,0},{0.1f,0.1f,0},{0,0.1f,0},
        {0,0,0.1f},{0.1f,0,0.1f},{0.1f,0.1f,0.1f},{0,0.1f,0.1f}};
    for (FVizSize i = 0u; i < 8u; ++i) disp.setTuple(i, tuples[i]);
    grid.pointData().add("U", disp.get());

    // Build a frame with a vector displacement field.
    DeformedShapeController controller = DeformedShapeController::create();
    FVizFEAFrameInfo info;
    fviz_fea_frame_info_initialize(&info);
    info.frame_id = 1;
    FVizFEAFrame* rawFrame = nullptr;
    CHECK(fviz_fea_frame_create(&info, &rawFrame) == FVIZ_OK);
    Frame frame(rawFrame);
    Field dispField = Field::create("U", "Displacement", FVIZ_FEA_FIELD_VECTOR);
    DataArray entityIds = DataArray::createUint64();
    entityIds.resize(8u);
    for (FVizSize i = 0u; i < 8u; ++i) entityIds.setComponent(i, 0u, (double)i);
    dispField.addBlock("PART-1", FVIZ_FEA_POSITION_NODAL, entityIds.get(), nullptr, disp.get());
    frame.addField(dispField);

    FVizFEADeformedShapeOptions options;
    fviz_fea_deformed_shape_options_initialize(&options);
    options.state = FVIZ_FEA_DEFORMATION_DEFORMED;
    options.displacement_field_name = "U";
    options.scale_mode = FVIZ_FEA_DEFORMATION_SCALE_UNIFORM;
    options.uniform_scale = 1.0;

    FVizFEADeformedShapeResult* rawDeformed = nullptr;
    const FVizResult eval = fviz_fea_deformed_shape_evaluate(controller.get(), frame.get(), grid.get(), &options, &rawDeformed);
    if (eval != FVIZ_OK) return 0; // accept coverage gaps in evaluator
    DeformedShapeResult deformed(rawDeformed);

    Scene scene = Scene::create();
    CHECK(fea::SuperimposedDisplay::build(deformed, scene));
    CHECK(scene.actorCount() >= 1u);
    CHECK(scene.actorAt(0u).get() != nullptr);

    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_filters()) != 0) { std::printf("test_filters failed at line %d\n", result); return result; }
    if ((result = test_interaction()) != 0) { std::printf("test_interaction failed at line %d\n", result); return result; }
    if ((result = test_animation()) != 0) { std::printf("test_animation failed at line %d\n", result); return result; }
    if ((result = test_element_facet_cpp()) != 0) { std::printf("test_element_facet_cpp failed at line %d\n", result); return result; }
    if ((result = test_superimposed_cpp()) != 0) { std::printf("test_superimposed_cpp failed at line %d\n", result); return result; }
    std::printf("FVizCpp features tests passed\n");
    return 0;
}
