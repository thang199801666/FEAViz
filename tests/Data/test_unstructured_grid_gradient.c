#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return __LINE__; } } while(0)

/* Builds a 2x2x2 hex beam with a linear scalar field phi = 2x + 3y - z + 5,
 * whose gradient is (2, 3, -1) everywhere. */
static FVizResult build_linear_field_grid(FVizUnstructuredGrid** out_grid, const char* scalar_name)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* scalars = NULL;
    FVizSize x, y, z;
    const FVizSize n = 3u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                const FVizVec3 point = fviz_vec3((float)x, (float)y, (float)z);
                uint32_t id;
                if (fviz_unstructured_grid_add_point(grid, point, &id) != FVIZ_OK) return fviz_last_error_code();
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
    {
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const FVizVec3* p = fviz_points_data(fviz_unstructured_grid_points(grid));
            const double value = 2.0 * (double)p[i].x + 3.0 * (double)p[i].y - (double)p[i].z + 5.0;
            if (fviz_data_array_set_component(scalars, i, 0u, value) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), scalar_name, scalars) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(scalars);
    *out_grid = grid;
    return FVIZ_OK;
}

static int test_gradient_scalar(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    const FVizDataArray* gradient = NULL;
    FVizSize i;
    CHECK(build_linear_field_grid(&grid, "phi") == FVIZ_OK);
    CHECK(fviz_unstructured_grid_gradient(grid, "phi", "grad_phi", &result) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(result) == fviz_unstructured_grid_point_count(grid));
    gradient = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(result), "grad_phi");
    CHECK(gradient != NULL);
    CHECK(fviz_data_array_components(gradient) == 3u);
    for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
    {
        double dx = 0.0, dy = 0.0, dz = 0.0;
        CHECK(fviz_data_array_get_component(gradient, i, 0u, &dx) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(gradient, i, 1u, &dy) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(gradient, i, 2u, &dz) == FVIZ_OK);
        CHECK(fabs(dx - 2.0) < 1.0e-6);
        CHECK(fabs(dy - 3.0) < 1.0e-6);
        CHECK(fabs(dz + 1.0) < 1.0e-6);
    }
    fviz_release(result);
    fviz_release(grid);
    return 0;
}

/* Linear vector field u = (x, 2y, x+y); the Jacobian rows are
 * du0=(1,0,0), du1=(0,2,0), du2=(1,1,0). Exact for a linear field. */
static int test_gradient_vector(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizDataArray* vectors = NULL;
    const FVizDataArray* gradient = NULL;
    FVizSize i;
    CHECK(build_linear_field_grid(&grid, "phi") == FVIZ_OK);
    /* Reuse the grid; attach a vector field. */
    {
        const FVizSize count = fviz_unstructured_grid_point_count(grid);
        const FVizVec3* p = fviz_points_data(fviz_unstructured_grid_points(grid));
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &vectors) == FVIZ_OK);
        CHECK(fviz_data_array_resize(vectors, count) == FVIZ_OK);
        for (i = 0u; i < count; ++i)
        {
            const double tuple[3] = {(double)p[i].x, 2.0 * (double)p[i].y, (double)p[i].x + (double)p[i].y};
            CHECK(fviz_data_array_set_tuple(vectors, i, tuple) == FVIZ_OK);
        }
        CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "u", vectors) == FVIZ_OK);
        fviz_release(vectors);
    }
    CHECK(fviz_unstructured_grid_gradient(grid, "u", "grad_u", &result) == FVIZ_OK);
    gradient = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(result), "grad_u");
    CHECK(gradient != NULL);
    CHECK(fviz_data_array_components(gradient) == 9u);
    {
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            double g[9];
            FVizSize c;
            for (c = 0u; c < 9u; ++c)
                CHECK(fviz_data_array_get_component(gradient, i, (uint32_t)c, &g[c]) == FVIZ_OK);
            /* Row 0 = du0/dx,du0/dy,du0/dz = (1, 0, 0) */
            CHECK(fabs(g[0] - 1.0) < 1.0e-6);
            CHECK(fabs(g[1] - 0.0) < 1.0e-6);
            CHECK(fabs(g[2] - 0.0) < 1.0e-6);
            /* Row 1 = du1 = (0, 2, 0) */
            CHECK(fabs(g[3] - 0.0) < 1.0e-6);
            CHECK(fabs(g[4] - 2.0) < 1.0e-6);
            CHECK(fabs(g[5] - 0.0) < 1.0e-6);
            /* Row 2 = du2 = (1, 1, 0) */
            CHECK(fabs(g[6] - 1.0) < 1.0e-6 && fabs(g[7] - 1.0) < 1.0e-6 && fabs(g[8]) < 1.0e-6);
        }
    }
    fviz_release(result);
    fviz_release(grid);
    return 0;
}

static int test_gradient_errors(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    CHECK(build_linear_field_grid(&grid, "phi") == FVIZ_OK);
    CHECK(fviz_unstructured_grid_gradient(grid, "missing", "g", &result) == FVIZ_ERROR_NOT_FOUND);
    CHECK(result == NULL);
    CHECK(fviz_unstructured_grid_gradient(grid, NULL, "g", &result) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_unstructured_grid_gradient(grid, "phi", NULL, &result) == FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_gradient_scalar()) != 0)
    { fprintf(stderr, "test_gradient_scalar failed at line %d\n", result); return result; }
    if ((result = test_gradient_vector()) != 0)
    { fprintf(stderr, "test_gradient_vector failed at line %d\n", result); return result; }
    if ((result = test_gradient_errors()) != 0)
    { fprintf(stderr, "test_gradient_errors failed at line %d\n", result); return result; }
    printf("FVizTestUnstructuredGridGradient passed\n");
    return 0;
}
