#include <stdint.h>
#include <stdio.h>

#include <FViz/Core/FVizObject.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizPolyData.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    FVizPolyData* poly = NULL;
    const FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
    };
    const FVizId triangle[3] = {0u, 1u, 2u};
    const FVizId line[2] = {2u, 3u};
    const FVizId polygon[4] = {0u, 1u, 2u, 3u};
    FVizId first = UINT64_MAX;
    FVizCellView view;

    CHECK(fviz_poly_data_create(&poly) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(poly, points, 4u, &first) == FVIZ_OK);
    CHECK(first == 0u);
    CHECK(fviz_poly_data_point_count(poly) == 4u);

    CHECK(fviz_poly_data_add_cell_ids(poly, FVIZ_CELL_TRIANGLE, 3u, triangle) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(poly) == 1u); /* render-ready fast path */
    CHECK(fviz_poly_data_poly_cell_count(poly) == 1u);

    CHECK(fviz_poly_data_add_cell_ids(poly, FVIZ_CELL_LINE, 2u, line) == FVIZ_OK);
    CHECK(fviz_poly_data_line_count(poly) == 1u);
    CHECK(fviz_poly_data_line_cell_count(poly) == 1u);

    CHECK(fviz_poly_data_add_cell_ids(poly, FVIZ_CELL_QUAD, 4u, polygon) == FVIZ_OK);
    CHECK(fviz_poly_data_poly_cell_count(poly) == 2u);
    CHECK(fviz_cell_array_cell_view(fviz_poly_data_polys(poly), 1u, &view) == FVIZ_OK);
    CHECK(view.type == FVIZ_CELL_QUAD && view.point_count == 4u);
    CHECK(fviz_cell_view_point_id(&view, 3u) == 3u);

    CHECK(fviz_poly_data_compute_normals(poly) == FVIZ_OK);
    CHECK(fviz_poly_data_has_normals(poly) != FVIZ_FALSE);
    CHECK(fviz_poly_data_validate(poly) == FVIZ_OK);

    fviz_release(poly);
    return 0;
}
