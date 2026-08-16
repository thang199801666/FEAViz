#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return __LINE__; } } while(0)

/* Builds a 2x2x2 hex beam grid with a scalar and a vector field. */
static FVizResult build_beam(FVizUnstructuredGrid** out_grid, const char* scalar_name, const char* vector_name)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* scalars = NULL;
    FVizDataArray* vectors = NULL;
    FVizSize x, y, z;
    const FVizSize n = 3u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                const FVizVec3 point = fviz_vec3((float)x, (float)y, (float)z);
                if (fviz_unstructured_grid_add_point(grid, point, NULL) != FVIZ_OK) return fviz_last_error_code();
            }
    for (z = 0u; z + 1u < n; ++z)
        for (y = 0u; y + 1u < n; ++y)
            for (x = 0u; x + 1u < n; ++x)
            {
                const uint32_t base = (uint32_t)(z * n * n + y * n + x);
                const uint32_t n32 = (uint32_t)n;
                const uint32_t ids[8] = {
                    base, base + 1u, base + n32 + 1u, base + n32,
                    base + n32 * n32, base + n32 * n32 + 1u, base + n32 * n32 + n32 + 1u, base + n32 * n32 + n32};
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    return fviz_last_error_code();
            }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &scalars) != FVIZ_OK ||
        fviz_data_array_resize(scalars, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &vectors) != FVIZ_OK ||
        fviz_data_array_resize(vectors, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK)
        return fviz_last_error_code();
    {
        FVizSize i;
        const FVizVec3* p = fviz_points_data(fviz_unstructured_grid_points(grid));
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const double phi = 2.0 * (double)p[i].x + 3.0 * (double)p[i].y - (double)p[i].z + 5.0;
            const double v[3] = {(double)p[i].y, (double)p[i].z, (double)p[i].x};
            if (fviz_data_array_set_component(scalars, i, 0u, phi) != FVIZ_OK ||
                fviz_data_array_set_tuple(vectors, i, v) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), scalar_name, scalars) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), vector_name, vectors) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(scalars);
    fviz_release(vectors);
    *out_grid = grid;
    return FVIZ_OK;
}

/* Builds a simple quad surface with a scalar ramp. */
static FVizResult build_quad(FVizPolyData** out_surface, const char* scalar_name)
{
    const FVizVec3 points[4] = {
        {0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    const double values[4] = {0.0, 10.0, 20.0, 30.0};
    FVizPolyData* surface = NULL;
    FVizDataArray* scalars = NULL;
    if (fviz_poly_data_create(&surface) != FVIZ_OK ||
        fviz_poly_data_add_points(surface, points, 4u, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(surface, 0u, 1u, 2u) != FVIZ_OK ||
        fviz_poly_data_add_triangle(surface, 0u, 2u, 3u) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &scalars) != FVIZ_OK ||
        fviz_data_array_append_tuples(scalars, values, 4u) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_point_data(surface), scalar_name, scalars) != FVIZ_OK ||
        fviz_poly_data_validate(surface) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(scalars);
    *out_surface = surface;
    return FVIZ_OK;
}

static int test_cell_derivatives(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    const FVizDataArray* derivatives = NULL;
    FVizSize i;
    CHECK(build_beam(&grid, "phi", "v") == FVIZ_OK);
    CHECK(fviz_unstructured_grid_cell_derivatives(grid, "phi", "dphi", &result) == FVIZ_OK);
    derivatives = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(result), "dphi");
    CHECK(derivatives != NULL);
    CHECK(fviz_data_array_components(derivatives) == 3u);
    CHECK(fviz_data_array_tuple_count(derivatives) == fviz_unstructured_grid_cell_count(grid));
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
    {
        double dx, dy, dz;
        CHECK(fviz_data_array_get_component(derivatives, i, 0u, &dx) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(derivatives, i, 1u, &dy) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(derivatives, i, 2u, &dz) == FVIZ_OK);
        CHECK(fabs(dx - 2.0) < 1.0e-6);
        CHECK(fabs(dy - 3.0) < 1.0e-6);
        CHECK(fabs(dz + 1.0) < 1.0e-6);
    }
    fviz_release(result);
    fviz_release(grid);
    return 0;
}

static int test_warp_scalar(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    const FVizVec3* p;
    FVizSize i;
    CHECK(build_beam(&grid, "phi", "v") == FVIZ_OK);
    CHECK(fviz_unstructured_grid_warp_scalar(grid, "phi", 0.1, NULL, &result) == FVIZ_OK);
    p = fviz_points_data(fviz_unstructured_grid_points(result));
    /* Point 0 at (0,0,0): phi=5, warped z = 0.5. */
    CHECK(fabs((double)p[0].z - 0.5) < 1.0e-6);
    CHECK(fabs((double)p[0].x) < 1.0e-6 && fabs((double)p[0].y) < 1.0e-6);
    /* Point with x=2,y=2,z=0: phi = 4+6+5=15, z = 1.5. */
    {
        const FVizVec3* orig = fviz_points_data(fviz_unstructured_grid_points(grid));
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const double expected_z = (double)orig[i].z + 0.1 * (2.0 * (double)orig[i].x + 3.0 * (double)orig[i].y - (double)orig[i].z + 5.0);
            CHECK(fabs((double)p[i].z - expected_z) < 1.0e-6);
            CHECK(fabs((double)p[i].x - (double)orig[i].x) < 1.0e-6);
            CHECK(fabs((double)p[i].y - (double)orig[i].y) < 1.0e-6);
        }
    }
    fviz_release(result);
    fviz_release(grid);
    return 0;
}

static int test_extract_edges(void)
{
    FVizPolyData* surface = NULL;
    FVizPolyData* edges = NULL;
    CHECK(build_quad(&surface, "phi") == FVIZ_OK);
    CHECK(fviz_poly_data_extract_edges(surface, &edges) == FVIZ_OK);
    /* Two triangles share edge 0-2; unique edges = 5 (0-1,1-2,2-3,3-0,0-2). */
    CHECK(fviz_poly_data_line_count(edges) == 5u);
    CHECK(fviz_poly_data_triangle_count(edges) == 0u);
    fviz_release(edges);
    fviz_release(surface);
    return 0;
}

static int test_delaunay_2d(void)
{
    FVizPolyData* points = NULL;
    FVizPolyData* tris = NULL;
    const FVizVec3 pts[6] = {
        {0,0,0},{1,0,0},{0.5f,1,0},{2,0,0},{1.5f,1,0},{0.5f,0.5f,0}};
    FVizSize i;
    CHECK(fviz_poly_data_create(&points) == FVIZ_OK);
    for (i = 0u; i < 6u; ++i)
        CHECK(fviz_poly_data_add_point(points, pts[i], NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_delaunay_2d(points, &tris) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(tris) >= 4u);
    /* Every triangle must reference only input points (0..5). */
    {
        const uint32_t* tri = fviz_poly_data_triangle_indices(tris);
        FVizSize t;
        for (t = 0u; t < fviz_poly_data_triangle_count(tris); ++t)
            for (i = 0u; i < 3u; ++i)
                CHECK(tri[t * 3u + i] < 6u);
    }
    fviz_release(tris);
    fviz_release(points);
    return 0;
}

static int test_glyph_3d(void)
{
    FVizPolyData* points = NULL;
    FVizPolyData* glyphs = NULL;
    FVizDataArray* vectors = NULL;
    const FVizVec3 pts[3] = {{0,0,0},{1,0,0},{2,0,0}};
    const double vecs[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    FVizSize i;
    CHECK(fviz_poly_data_create(&points) == FVIZ_OK);
    for (i = 0u; i < 3u; ++i)
        CHECK(fviz_poly_data_add_point(points, pts[i], NULL) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &vectors) == FVIZ_OK);
    for (i = 0u; i < 3u; ++i)
        CHECK(fviz_data_array_append_tuple(vectors, vecs[i]) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(points), "v", vectors) == FVIZ_OK);
    fviz_release(vectors);
    CHECK(fviz_poly_data_glyph_3d(points, NULL, "v", 1.0, &glyphs) == FVIZ_OK);
    /* One arrow per input point: 6 points, 3 lines. */
    CHECK(fviz_poly_data_point_count(glyphs) == 6u);
    CHECK(fviz_poly_data_line_count(glyphs) == 3u);
    fviz_release(glyphs);
    fviz_release(points);
    return 0;
}

static int test_stream_tracer(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* lines = NULL;
    const FVizVec3 seed = {1.0f, 0.0f, 0.0f};
    CHECK(build_beam(&grid, "phi", "v") == FVIZ_OK);
    /* v = (y, z, x): starts at (1,0,0) -> integrates along +x. */
    CHECK(fviz_unstructured_grid_stream_tracer(grid, "v", &seed, 1u, 0.1, 20, &lines) == FVIZ_OK);
    CHECK(fviz_poly_data_line_count(lines) > 0u);
    CHECK(fviz_poly_data_point_count(lines) >= 2u);
    fviz_release(lines);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_cell_derivatives()) != 0)
    { fprintf(stderr, "test_cell_derivatives failed at line %d\n", result); return result; }
    if ((result = test_warp_scalar()) != 0)
    { fprintf(stderr, "test_warp_scalar failed at line %d\n", result); return result; }
    if ((result = test_extract_edges()) != 0)
    { fprintf(stderr, "test_extract_edges failed at line %d\n", result); return result; }
    if ((result = test_delaunay_2d()) != 0)
    { fprintf(stderr, "test_delaunay_2d failed at line %d\n", result); return result; }
    if ((result = test_glyph_3d()) != 0)
    { fprintf(stderr, "test_glyph_3d failed at line %d\n", result); return result; }
    if ((result = test_stream_tracer()) != 0)
    { fprintf(stderr, "test_stream_tracer failed at line %d\n", result); return result; }
    printf("FVizTestVTKFilters passed\n");
    return 0;
}
