#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return __LINE__; } } while(0)

/* Verifies the topology table (dimension, point count, edges, faces) of every
 * newly added VTK-compatible cell type. */
static int test_topology_tables(void)
{
    struct { FVizCellType type; uint8_t dim; uint8_t points; uint8_t edges; uint8_t faces; } cases[] = {
        {FVIZ_CELL_EMPTY, 0u, 0u, 0u, 0u},
        {FVIZ_CELL_PIXEL, 2u, 4u, 4u, 1u},
        {FVIZ_CELL_VOXEL, 3u, 8u, 12u, 6u},
        {FVIZ_CELL_PENTAGONAL_PRISM, 3u, 10u, 15u, 7u},
        {FVIZ_CELL_HEXAGONAL_PRISM, 3u, 12u, 18u, 8u},
        {FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON, 3u, 27u, 12u, 6u},
        {FVIZ_CELL_QUADRATIC_LINEAR_QUAD, 2u, 6u, 4u, 1u},
        {FVIZ_CELL_QUADRATIC_LINEAR_WEDGE, 3u, 12u, 9u, 5u},
        {FVIZ_CELL_BIQUADRATIC_QUADRATIC_WEDGE, 3u, 18u, 9u, 5u},
        {FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON, 3u, 27u, 12u, 6u},
        {FVIZ_CELL_BIQUADRATIC_TRIANGLE, 2u, 7u, 3u, 1u},
        {FVIZ_CELL_CUBIC_LINE, 1u, 4u, 1u, 0u},
    };
    size_t i;
    for (i = 0u; i < sizeof(cases)/sizeof(cases[0]); ++i)
    {
        const FVizCellTypeTraits t = fviz_cell_type_traits(cases[i].type);
        CHECK(fviz_cell_type_is_supported(cases[i].type) == FVIZ_TRUE);
        CHECK(t.dimension == cases[i].dim);
        CHECK(t.fixed_point_count == cases[i].points);
        CHECK(t.edge_count == cases[i].edges);
        CHECK(t.face_count == cases[i].faces);
        CHECK(fviz_cell_type_accepts_point_count(cases[i].type, cases[i].points) == FVIZ_TRUE);
    }
    /* Variable-point-count types. */
    CHECK(fviz_cell_type_traits(FVIZ_CELL_QUADRATIC_POLYGON).variable_point_count == FVIZ_TRUE);
    CHECK(fviz_cell_type_traits(FVIZ_CELL_CONVEX_POINT_SET).variable_point_count == FVIZ_TRUE);
    CHECK(fviz_cell_type_traits(FVIZ_CELL_POLYHEDRON).variable_point_count == FVIZ_TRUE);
    CHECK(fviz_cell_type_accepts_point_count(FVIZ_CELL_QUADRATIC_POLYGON, 5u) == FVIZ_TRUE);
    CHECK(fviz_cell_type_accepts_point_count(FVIZ_CELL_CONVEX_POINT_SET, 6u) == FVIZ_TRUE);
    CHECK(fviz_cell_type_accepts_point_count(FVIZ_CELL_POLYHEDRON, 6u) == FVIZ_TRUE);
    /* Legacy enum value parity with VTK IDs. */
    CHECK(FVIZ_CELL_VOXEL == 11 && FVIZ_CELL_PIXEL == 8 && FVIZ_CELL_HEXAHEDRON == 12);
    CHECK(FVIZ_CELL_PENTAGONAL_PRISM == 15 && FVIZ_CELL_HEXAGONAL_PRISM == 16);
    CHECK(FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON == 29);
    CHECK(FVIZ_CELL_QUADRATIC_LINEAR_QUAD == 30 && FVIZ_CELL_QUADRATIC_LINEAR_WEDGE == 31);
    CHECK(FVIZ_CELL_BIQUADRATIC_QUADRATIC_WEDGE == 32);
    CHECK(FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON == 33);
    CHECK(FVIZ_CELL_BIQUADRATIC_TRIANGLE == 34);
    CHECK(FVIZ_CELL_CUBIC_LINE == 36 && FVIZ_CELL_QUADRATIC_POLYGON == 37);
    CHECK(FVIZ_CELL_CONVEX_POINT_SET == 41 && FVIZ_CELL_POLYHEDRON == 42);
    /* Vertex/edge topology spot checks. */
    {
        uint32_t edge[2];
        CHECK(fviz_cell_type_edge(FVIZ_CELL_VOXEL, 0u, edge) == FVIZ_OK);
        CHECK(edge[0] == 0u && edge[1] == 1u);
        CHECK(fviz_cell_type_edge(FVIZ_CELL_PIXEL, 2u, edge) == FVIZ_OK);
        CHECK(edge[0] == 2u && edge[1] == 3u);
    }
    return 0;
}

/* Partition of unity: shape weights sum to one at the reference center. */
static int test_partition_of_unity(void)
{
    static const FVizCellType interpolatable[] = {
        FVIZ_CELL_VOXEL, FVIZ_CELL_PIXEL, FVIZ_CELL_PENTAGONAL_PRISM,
        FVIZ_CELL_HEXAGONAL_PRISM, FVIZ_CELL_QUADRATIC_LINEAR_QUAD,
        FVIZ_CELL_QUADRATIC_LINEAR_WEDGE, FVIZ_CELL_BIQUADRATIC_TRIANGLE,
        FVIZ_CELL_CUBIC_LINE, FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON,
        FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON
    };
    const FVizVec3 centers[] = {
        {0.0f, 0.0f, 0.0f},
        {0.25f, -0.15f, 0.1f},
        {-0.2f, 0.3f, -0.1f}
    };
    size_t i, c;
    for (i = 0u; i < sizeof(interpolatable)/sizeof(interpolatable[0]); ++i)
    {
        for (c = 0u; c < sizeof(centers)/sizeof(centers[0]); ++c)
        {
            double weights[32];
            FVizSize count = 0u;
            double sum = 0.0;
            FVizSize k;
            CHECK(fviz_cell_type_shape_weights(interpolatable[i], centers[c], weights, 32u, &count) == FVIZ_OK);
            CHECK(count >= 2u && count <= 27u);
            for (k = 0u; k < count; ++k) sum += weights[k];
            CHECK(fabs(sum - 1.0) < 1.0e-9);
        }
    }
    return 0;
}

/* The interpolatable weight sets are delta functions at their own node. */
static int test_interpolation_delta(void)
{
    struct { FVizCellType type; FVizVec3 node; FVizSize expected_index; } cases[] = {
        {FVIZ_CELL_VOXEL, {-1.0f, -1.0f, -1.0f}, 0u},
        {FVIZ_CELL_BIQUADRATIC_TRIANGLE, {0.0f, 0.0f, 0.0f}, 0u},
        {FVIZ_CELL_CUBIC_LINE, {-1.0f, 0.0f, 0.0f}, 0u},
        {FVIZ_CELL_QUADRATIC_LINEAR_QUAD, {-1.0f, -1.0f, 0.0f}, 0u},
    };
    size_t i;
    for (i = 0u; i < sizeof(cases)/sizeof(cases[0]); ++i)
    {
        double weights[32];
        FVizSize count = 0u;
        FVizSize k;
        CHECK(fviz_cell_type_shape_weights(cases[i].type, cases[i].node, weights, 32u, &count) == FVIZ_OK);
        for (k = 0u; k < count; ++k)
        {
            const double expected = k == cases[i].expected_index ? 1.0 : 0.0;
            CHECK(fabs(weights[k] - expected) < 1.0e-9);
        }
    }
    /* 27-node hex: body-center weight is 1 at the origin. */
    {
        double weights[32];
        FVizSize count = 0u;
        FVizSize k;
        CHECK(fviz_cell_type_shape_weights(FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON,
            fviz_vec3(0.0f, 0.0f, 0.0f), weights, 32u, &count) == FVIZ_OK);
        CHECK(count == 27u);
        for (k = 0u; k < count; ++k)
            CHECK(fabs(weights[k] - (k == 26u ? 1.0 : 0.0)) < 1.0e-9);
    }
    return 0;
}

/* Append + validate new cell types in a CellArray. */
static int test_cell_array_storage(void)
{
    FVizCellArray* cells = NULL;
    CHECK(fviz_cell_array_create(&cells) == FVIZ_OK);
    {
        const uint32_t voxel[8] = {0,1,3,2,4,5,7,6};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_VOXEL, 8u, voxel) == FVIZ_OK);
        CHECK(fviz_cell_array_validate(cells, 8u) == FVIZ_OK);
    }
    {
        const uint32_t penta[10] = {0,1,2,3,4,5,6,7,8,9};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_PENTAGONAL_PRISM, 10u, penta) == FVIZ_OK);
    }
    {
        const uint32_t hexa[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_HEXAGONAL_PRISM, 12u, hexa) == FVIZ_OK);
    }
    {
        const uint32_t quad27[27] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON, 27u, quad27) == FVIZ_OK);
    }
    {
        const uint32_t pixel[4] = {0,1,2,3};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_PIXEL, 4u, pixel) == FVIZ_OK);
    }
    CHECK(fviz_cell_array_count(cells) == 5u);
    CHECK(fviz_cell_array_validate(cells, 27u) == FVIZ_OK);
    CHECK(fviz_cell_array_type(cells, 0u) == FVIZ_CELL_VOXEL);
    CHECK(fviz_cell_array_point_count(cells, 3u) == 27u);
    /* Wrong point count must be rejected. */
    {
        const uint32_t bad[8] = {0,1,2,3,4,5,6,7};
        CHECK(fviz_cell_array_append(cells, FVIZ_CELL_PENTAGONAL_PRISM, 8u, bad) == FVIZ_ERROR_INVALID_ARGUMENT);
    }
    fviz_release(cells);
    return 0;
}

/* VTU round-trip preserves the new cell type IDs. */
static int test_vtu_roundtrip(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* loaded = NULL;
    const uint32_t voxel[8] = {0,1,3,2,4,5,7,6};
    const char* const path = "fviz_celltypes_test.vtu";
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    for (i = 0u; i < 8u; ++i)
    {
        const FVizVec3 p = fviz_vec3((float)(i & 1u), (float)((i >> 1) & 1u), (float)((i >> 2) & 1u));
        CHECK(fviz_unstructured_grid_add_point(grid, p, NULL) == FVIZ_OK);
    }
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_VOXEL, 8u, voxel) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_validate(grid) == FVIZ_OK);
    {
        FVizVTUWriterOptions options;
        fviz_vtu_writer_options_initialize(&options);
        options.output_mode = FVIZ_VTU_OUTPUT_ASCII;
        CHECK(fviz_vtu_write(path, grid, &options) == FVIZ_OK);
    }
    CHECK(fviz_vtu_read(path, &loaded) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_cell_count(loaded) == 1u);
    {
        const FVizCellArray* cells = fviz_unstructured_grid_cells(loaded);
        CHECK(fviz_cell_array_type(cells, 0u) == FVIZ_CELL_VOXEL);
        CHECK(fviz_cell_array_point_count(cells, 0u) == 8u);
    }
    fviz_release(loaded);
    fviz_release(grid);
    remove(path);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_topology_tables()) != 0)
    { fprintf(stderr, "test_topology_tables failed at line %d\n", result); return result; }
    if ((result = test_partition_of_unity()) != 0)
    { fprintf(stderr, "test_partition_of_unity failed at line %d\n", result); return result; }
    if ((result = test_interpolation_delta()) != 0)
    { fprintf(stderr, "test_interpolation_delta failed at line %d\n", result); return result; }
    if ((result = test_cell_array_storage()) != 0)
    { fprintf(stderr, "test_cell_array_storage failed at line %d\n", result); return result; }
    if ((result = test_vtu_roundtrip()) != 0)
    { fprintf(stderr, "test_vtu_roundtrip failed at line %d\n", result); return result; }
    printf("FVizTestCellTypesVTK passed\n");
    return 0;
}
