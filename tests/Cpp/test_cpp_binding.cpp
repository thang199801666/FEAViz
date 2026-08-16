// FEAViz C++ binding tests.
//
// Exercises the header-only C++ layer: math value types, RAII object
// ownership/refcounting, grid construction, data arrays, file readers, and
// headless rendering-object assembly.

#include <FVizCpp/FVizCpp.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef FVIZ_TESTDATA_DIR
#define FVIZ_TESTDATA_DIR "."
#endif

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

using namespace fviz;

static int test_math()
{
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);

    CHECK(a + b == Vec3(5.0f, 7.0f, 9.0f));
    CHECK(b - a == Vec3(3.0f, 3.0f, 3.0f));
    CHECK(a * 2.0f == Vec3(2.0f, 4.0f, 6.0f));
    CHECK(2.0f * a == Vec3(2.0f, 4.0f, 6.0f));
    CHECK(a.dot(b) == 32.0f);
    CHECK(a.cross(b) == Vec3(-3.0f, 6.0f, -3.0f));
    CHECK(a.length() > 3.7f && a.length() < 3.8f);

    const Vec3 n = Vec3(3.0f, 0.0f, 0.0f).normalized();
    CHECK(n.x > 0.99f && n.y == 0.0f && n.z == 0.0f);

    // C interop: implicit conversion into FVizVec3.
    const FVizVec3 raw = a;
    CHECK(raw.x == 1.0f && raw.y == 2.0f && raw.z == 3.0f);

    Mat4 m = Mat4::identity();
    CHECK(m.m[0] == 1.0f && m.m[5] == 1.0f && m.m[10] == 1.0f && m.m[15] == 1.0f);

    Quat q = Quat::identity();
    CHECK(q.w == 1.0f);
    const Vec3 rotated = q.rotate(Vec3(1.0f, 0.0f, 0.0f));
    CHECK(rotated.x > 0.99f);

    Bounds bounds;
    bounds.includePoint(Vec3(1.0f, 2.0f, 3.0f));
    bounds.includePoint(Vec3(-1.0f, -2.0f, -3.0f));
    CHECK(bounds.valid);
    CHECK(bounds.size() == Vec3(2.0f, 4.0f, 6.0f));
    const Vec3 center = bounds.center();
    CHECK(center.x == 0.0f && center.y == 0.0f && center.z == 0.0f);

    Ray ray = Ray::fromPoints(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
    CHECK(ray.pointAt(2.0f) == Vec3(2.0f, 0.0f, 0.0f));
    float hit = -1.0f;
    CHECK(ray.intersectsSphere(Vec3(5.0f, 0.0f, 0.0f), 1.0f, &hit));
    CHECK(hit > 3.9f && hit < 4.1f);

    Plane plane = Plane::fromPointNormal(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    CHECK(plane.distanceToPoint(Vec3(0.0f, 3.0f, 0.0f)) > 2.9f);
    return 0;
}

static int test_raii_refcount()
{
    {
        DataArray array = DataArray::createFloat32();
        CHECK(array.get() != nullptr);
        CHECK(array.refCount() == 1u);
        CHECK(array.isType(FVIZ_TYPE_DATA_ARRAY));

        {
            DataArray copy = array; // retains
            CHECK(copy.get() == array.get());
            CHECK(array.refCount() == 2u);
        }
        CHECK(array.refCount() == 1u);

        DataArray moved = std::move(array);
        CHECK(array.get() == nullptr);       // moved-from is empty
        CHECK(moved.get() != nullptr);
        CHECK(moved.refCount() == 1u);

        DataArray assigned;
        assigned = moved; // copy assignment retains
        CHECK(assigned.get() == moved.get());
        CHECK(moved.refCount() == 2u);
    }
    return 0;
}

static int test_data_array()
{
    DataArray scalars = DataArray::createFloat32();
    scalars.resize(4u);
    CHECK(scalars.size() == 4u);
    CHECK(scalars.components() == 1u);

    for (FVizSize i = 0u; i < scalars.size(); ++i)
        scalars.setComponent(i, 0u, static_cast<double>(i) * 10.0);

    CHECK(scalars.component(0u) == 0.0);
    CHECK(scalars.component(3u) == 30.0);

    double minimum = 0.0;
    double maximum = 0.0;
    scalars.range(0, minimum, maximum);
    CHECK(minimum == 0.0 && maximum == 30.0);

    DataArray vectors = DataArray::createFloat32(3u);
    vectors.resize(2u);
    float tuple[3] = {1.0f, 2.0f, 3.0f};
    vectors.setTuple(0u, tuple);
    CHECK(vectors.components() == 3u);
    CHECK(vectors.component(0u, 2u) == 3.0);

    return 0;
}

static int test_grid_build()
{
    UnstructuredGrid grid = UnstructuredGrid::create();
    const uint32_t nx = 3u;
    const uint32_t ny = 2u;
    const uint32_t nz = 2u;

    for (uint32_t z = 0u; z <= nz; ++z)
        for (uint32_t y = 0u; y <= ny; ++y)
            for (uint32_t x = 0u; x <= nx; ++x)
                grid.addPoint(Vec3((float)x, (float)y, (float)z));

    const auto pointId = [](uint32_t x, uint32_t y, uint32_t z) {
        return x + 4u * (y + 3u * z);
    };
    for (uint32_t z = 0u; z < nz; ++z)
        for (uint32_t y = 0u; y < ny; ++y)
            for (uint32_t x = 0u; x < nx; ++x)
            {
                const uint32_t ids[8] = {
                    pointId(x, y, z), pointId(x + 1u, y, z), pointId(x + 1u, y + 1u, z), pointId(x, y + 1u, z),
                    pointId(x, y, z + 1u), pointId(x + 1u, y, z + 1u), pointId(x + 1u, y + 1u, z + 1u), pointId(x, y + 1u, z + 1u)};
                grid.addCell(FVIZ_CELL_HEXAHEDRON, 8u, ids);
            }

    CHECK(grid.pointCount() == 36u);
    CHECK(grid.cellCount() == 12u);

    // Attach a scalar point field.
    DataArray stress = DataArray::createFloat64();
    stress.resize(grid.pointCount());
    for (FVizSize i = 0u; i < grid.pointCount(); ++i)
        stress.setComponent(i, 0u, static_cast<double>(i) * 2.5);
    AttributeSet point_data = grid.pointData();
    point_data.add("stress", stress.get());
    CHECK(point_data.has("stress"));

    // Attach a vector displacement field.
    DataArray displacement = DataArray::createFloat32(3u);
    displacement.resize(grid.pointCount());
    for (FVizSize i = 0u; i < grid.pointCount(); ++i)
    {
        float tuple[3] = {0.001f * (float)i, 0.0f, 0.0f};
        displacement.setTuple(i, tuple);
    }
    point_data.add("displacement", displacement.get());

    const Bounds grid_bounds = grid.bounds();
    CHECK(grid_bounds.valid);
    CHECK(grid_bounds.size().x > 2.9f);

    grid.validate();

    // Cell data -> point data smoothing path.
    {
        FVizUnstructuredGrid* smoothed = nullptr;
        CHECK(fviz_unstructured_grid_cell_data_to_point_data(grid.get(), &smoothed) == FVIZ_OK);
        UnstructuredGrid smoothedGrid(smoothed);
        CHECK(smoothedGrid.pointCount() == grid.pointCount());
    }

    // Surface extraction.
    {
        FVizPolyData* surface = nullptr;
        CHECK(fviz_unstructured_grid_extract_geometry(grid.get(), &surface) == FVIZ_OK);
        PolyData poly(surface);
        CHECK(poly.pointCount() > 0u);
        CHECK(poly.triangleCount() > 0u);
        poly.computeNormals();
        CHECK(poly.pointCount() == poly.pointCount());
    }

    return 0;
}

static int test_readers()
{
    const std::string dir = FVIZ_TESTDATA_DIR;
    {
        UnstructuredGrid grid = readVtu(dir + "/hex.vtu");
        CHECK(grid.get() != nullptr);
        CHECK(grid.pointCount() > 0u);
    }
    {
        UnstructuredGrid grid = readVtu(dir + "/hex_binary.vtu");
        CHECK(grid.pointCount() > 0u);
    }
    {
        UnstructuredGrid grid = readVtkLegacy(dir + "/hex_legacy.vtk");
        CHECK(grid.pointCount() > 0u);
    }
    {
        PolyData poly = readObj(dir + "/cube.obj");
        CHECK(poly.pointCount() > 0u);
        CHECK(poly.triangleCount() > 0u);
    }
    {
        PolyData poly = readStl(dir + "/triangle.stl");
        CHECK(poly.pointCount() > 0u);
        CHECK(poly.triangleCount() > 0u);
    }
    return 0;
}

static int test_rendering_objects()
{
    // Pure-object assembly that does not require a window.
    UnstructuredGrid grid = UnstructuredGrid::create();
    grid.addPoint(Vec3(0.0f, 0.0f, 0.0f));
    grid.addPoint(Vec3(1.0f, 0.0f, 0.0f));
    grid.addPoint(Vec3(1.0f, 1.0f, 0.0f));
    grid.addPoint(Vec3(0.0f, 1.0f, 0.0f));
    const uint32_t quad[4] = {0u, 1u, 2u, 3u};
    grid.addCell(FVIZ_CELL_QUAD, 4u, quad);

    FVizPolyData* surface_raw = nullptr;
    CHECK(fviz_unstructured_grid_extract_geometry(grid.get(), &surface_raw) == FVIZ_OK);
    PolyData surface(surface_raw);
    surface.computeNormals();
    CHECK(surface.triangleCount() > 0u);

    DataArray stress = DataArray::createFloat64();
    stress.resize(surface.pointCount());
    for (FVizSize i = 0u; i < surface.pointCount(); ++i)
        stress.setComponent(i, 0u, static_cast<double>(i));
    AttributeSet point_data = surface.pointData();
    point_data.add("stress", stress.get());

    LookupTable lut = LookupTable::create(256u);
    lut.setRange(0.0f, 3.0f);
    lut.buildPreset(FVIZ_COLOR_MAP_RAINBOW);

    Mapper mapper = Mapper::create();
    mapper.setPolyData(surface);
    mapper.setLookupTable(lut);
    mapper.setArraySelection(FVIZ_ASSOCIATION_POINTS, "stress", FVIZ_COMPONENT_DIRECT);
    mapper.setScalarVisibility(true);
    mapper.setScalarRange(0.0f, 3.0f);

    Actor actor = Actor::create();
    actor.setMapper(mapper);
    actor.setColor(0.2f, 0.5f, 0.8f);
    actor.setVisible(true);
    actor.setEdgeVisibility(true);
    actor.setEdgeColor(0.05f, 0.08f, 0.12f);

    Scene scene = Scene::create();
    scene.addActor(actor);
    CHECK(scene.actorCount() == 1u);

    Renderer renderer = Renderer::create();
    renderer.setScene(scene);
    renderer.setBackground(Vec3(0.04f, 0.05f, 0.07f));
    renderer.setGradientBackground(true);
    renderer.fitCamera(1.2f);

    Camera camera = renderer.camera();
    CHECK(camera.get() != nullptr);
    camera.setPerspective(30.0f, 0.01f, 100.0f);
    camera.setProjectionMode(FVIZ_CAMERA_PARALLEL);
    CHECK(camera.projectionMode() == FVIZ_CAMERA_PARALLEL);
    camera.setParallelScale(1.0f);
    camera.setTarget(Vec3(0.5f, 0.5f, 0.0f));

    ScalarLegend legend = ScalarLegend::create();
    legend.setLookupTable(lut);
    legend.setRange(0.0f, 3.0f);
    legend.setTitle("stress");
    legend.setPosition(FVIZ_LEGEND_TOP_LEFT);
    legend.setVisible(true);

    // Mapper selection must have registered the array.
    FVizMapper* raw_mapper = mapper.get();
    const FVizDataArray* selected = fviz_mapper_selected_array(raw_mapper);
    CHECK(selected != nullptr);

    return 0;
}

static int test_cell_types_vtk()
{
    // Append and validate the newly added VTK cell types through the binding.
    CellArray cells = CellArray::create();
    const uint32_t voxel[8] = {0, 1, 3, 2, 4, 5, 7, 6};
    const uint32_t penta[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    cells.append(FVIZ_CELL_VOXEL, 8u, voxel);
    cells.append(FVIZ_CELL_PENTAGONAL_PRISM, 10u, penta);
    CHECK(cells.count() == 2u);
    CHECK(cells.cellType(0u) == FVIZ_CELL_VOXEL);
    CHECK(cells.cellType(1u) == FVIZ_CELL_PENTAGONAL_PRISM);

    // Shape weights: partition of unity for the 27-node hex.
    const FVizCellType interpolatable[] = {
        FVIZ_CELL_VOXEL, FVIZ_CELL_PIXEL, FVIZ_CELL_QUADRATIC_LINEAR_QUAD,
        FVIZ_CELL_BIQUADRATIC_TRIANGLE, FVIZ_CELL_CUBIC_LINE,
        FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON, FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON};
    const FVizVec3 test_point = {0.2f, -0.1f, 0.15f};
    for (size_t i = 0u; i < sizeof(interpolatable) / sizeof(interpolatable[0]); ++i)
    {
        double weights[32];
        FVizSize count = 0u;
        double sum = 0.0;
        CHECK(fviz_cell_type_shape_weights(interpolatable[i], test_point, weights, 32u, &count) == FVIZ_OK);
        for (FVizSize k = 0u; k < count; ++k) sum += weights[k];
        CHECK(std::fabs(sum - 1.0) < 1.0e-9);
    }
    return 0;
}

static int test_gradient_cpp()
{
    // Hex beam with a linear scalar phi = 2x + 3y - z + 5; gradient (2,3,-1).
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
    DataArray phi = DataArray::createFloat64();
    phi.resize(grid.pointCount());
    {
        const FVizVec3* p = fviz_points_data(fviz_unstructured_grid_points(grid.get()));
        for (FVizSize i = 0u; i < grid.pointCount(); ++i)
            phi.setComponent(i, 0u, 2.0 * p[i].x + 3.0 * p[i].y - (double)p[i].z + 5.0);
    }
    grid.pointData().add("phi", phi.get());

    UnstructuredGrid result = grid.gradient("phi", "grad_phi");
    CHECK(result.get() != nullptr);
    DataArray grad = result.pointData().get("grad_phi");
    CHECK(grad.get() != nullptr);
    CHECK(grad.components() == 3u);
    for (FVizSize i = 0u; i < result.pointCount(); ++i)
    {
        CHECK(std::fabs(grad.component(i, 0u) - 2.0) < 1.0e-6);
        CHECK(std::fabs(grad.component(i, 1u) - 3.0) < 1.0e-6);
        CHECK(std::fabs(grad.component(i, 2u) + 1.0) < 1.0e-6);
    }
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_math()) != 0) { std::printf("test_math failed at line %d\n", result); return result; }
    if ((result = test_raii_refcount()) != 0) { std::printf("test_raii_refcount failed at line %d\n", result); return result; }
    if ((result = test_data_array()) != 0) { std::printf("test_data_array failed at line %d\n", result); return result; }
    if ((result = test_grid_build()) != 0) { std::printf("test_grid_build failed at line %d\n", result); return result; }
    if ((result = test_readers()) != 0) { std::printf("test_readers failed at line %d\n", result); return result; }
    if ((result = test_rendering_objects()) != 0) { std::printf("test_rendering_objects failed at line %d\n", result); return result; }
    if ((result = test_cell_types_vtk()) != 0) { std::printf("test_cell_types_vtk failed at line %d\n", result); return result; }
    if ((result = test_gradient_cpp()) != 0) { std::printf("test_gradient_cpp failed at line %d\n", result); return result; }
    std::printf("FVizCpp binding tests passed\n");
    return 0;
}
