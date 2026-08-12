#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_beam_grid(FVizUnstructuredGrid** out_grid, FVizDataArray** out_stress)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizSize x;
    FVizSize y;
    FVizSize z;
    const FVizSize n = 3u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK) return fviz_last_error_code();

    for (z = 0u; z < n; ++z)
    {
        for (y = 0u; y < n; ++y)
        {
            for (x = 0u; x < n; ++x)
            {
                const FVizVec3 point = fviz_vec3((float)x, (float)y, (float)z);
                uint32_t id;
                if (fviz_unstructured_grid_add_point(grid, point, &id) != FVIZ_OK) return fviz_last_error_code();
            }
        }
    }

    for (z = 0u; z + 1u < n; ++z)
    {
        for (y = 0u; y + 1u < n; ++y)
        {
            for (x = 0u; x + 1u < n; ++x)
            {
                const uint32_t base = (uint32_t)(z * n * n + y * n + x);
                const uint32_t n32 = (uint32_t)n;
                const uint32_t ids[8] = {
                    base,
                    base + 1u,
                    base + n32 + 1u,
                    base + n32,
                    base + n32 * n32,
                    base + n32 * n32 + 1u,
                    base + n32 * n32 + n32 + 1u,
                    base + n32 * n32 + n32
                };
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                {
                    fviz_release(grid);
                    return fviz_last_error_code();
                }
            }
        }
    }

    if (fviz_data_array_resize(stress, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float scalar = (float)i;
            if (fviz_data_array_set_tuple(stress, i, &scalar) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "stress", stress) != FVIZ_OK) return fviz_last_error_code();
    *out_grid = grid;
    *out_stress = stress;
    return FVIZ_OK;
}

static int test_extract_surface_scalars(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizPolyData* surface = NULL;
    const FVizDataArray* transferred;
    CHECK(build_beam_grid(&grid, &stress) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface_scalars(grid, &surface) == FVIZ_OK);
    CHECK(surface != NULL);
    CHECK(fviz_poly_data_triangle_count(surface) > 0u);
    transferred = fviz_poly_data_const_scalars(surface);
    CHECK(transferred != NULL);
    CHECK(fviz_data_array_tuple_count(transferred) == fviz_poly_data_point_count(surface));
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_point_data(surface), "stress") != NULL);
    {
        const float* values = (const float*)fviz_data_array_const_data(transferred);
        CHECK(fabsf(values[0] - 0.0f) < 1.0e-6f);
        CHECK(fabsf(values[26] - 26.0f) < 1.0e-6f);
    }
    fviz_release(surface);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_slice_midplane(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizPolyData* slice = NULL;
    const FVizDataArray* sliced_stress;
    FVizPlane plane = fviz_plane_from_point_normal(fviz_vec3(1.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f));
    FVizSize i;
    CHECK(build_beam_grid(&grid, &stress) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_slice(grid, plane, &slice) == FVIZ_OK);
    CHECK(slice != NULL);
    CHECK(fviz_poly_data_triangle_count(slice) > 0u);
    sliced_stress = fviz_poly_data_const_scalars(slice);
    CHECK(sliced_stress != NULL);
    CHECK(fviz_data_array_tuple_count(sliced_stress) == fviz_poly_data_point_count(slice));
    {
        const float* values = (const float*)fviz_data_array_const_data((FVizDataArray*)sliced_stress);
        for (i = 0u; i < fviz_data_array_tuple_count(sliced_stress); ++i)
        {
            CHECK(values[i] >= 0.0f && values[i] <= (float)(fviz_unstructured_grid_point_count(grid) - 1u));
        }
    }
    fviz_release(slice);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_slice_offset_plane(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizPolyData* slice = NULL;
    FVizPlane plane = fviz_plane_from_point_normal(fviz_vec3(0.5f, 0.5f, 0.0f), fviz_vec3(0.0f, 0.0f, 1.0f));
    CHECK(build_beam_grid(&grid, &stress) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_slice(grid, plane, &slice) == FVIZ_OK);
    CHECK(slice != NULL);
    CHECK(fviz_poly_data_triangle_count(slice) > 0u);
    CHECK(fviz_poly_data_const_scalars(slice) != NULL);
    fviz_release(slice);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_slice_miss(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizPolyData* slice = NULL;
    FVizPlane plane = fviz_plane_from_point_normal(fviz_vec3(100.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f));
    CHECK(build_beam_grid(&grid, &stress) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_slice(grid, plane, &slice) == FVIZ_OK);
    CHECK(slice != NULL);
    CHECK(fviz_poly_data_triangle_count(slice) == 0u);
    fviz_release(slice);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    CHECK(test_extract_surface_scalars() == 0);
    CHECK(test_slice_midplane() == 0);
    CHECK(test_slice_offset_plane() == 0);
    CHECK(test_slice_miss() == 0);
    return 0;
}
