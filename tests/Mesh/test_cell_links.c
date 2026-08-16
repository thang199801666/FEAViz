#include <stdio.h>

#include <FViz/Core/FVizObject.h>
#include <FViz/Mesh/FVizCellAdjacency.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizCellLinks.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void)
{
    FVizCellArray* cells = NULL;
    FVizCellLinks* links = NULL;
    FVizCellAdjacency* adjacency = NULL;
    const FVizId hexes[4][8] = {
        {0,1,4,3,6,7,10,9},
        {1,2,5,4,7,8,11,10},
        {6,7,10,9,12,13,16,15},
        {7,8,11,10,13,14,17,16}
    };
    FVizSize count = 0u;
    const FVizId* ids;

    CHECK(fviz_cell_array_create(&cells) == FVIZ_OK);
    CHECK(fviz_cell_array_append_fixed_ids(cells, FVIZ_CELL_HEXAHEDRON, 8u, 4u, &hexes[0][0]) == FVIZ_OK);
    CHECK(fviz_cell_links_build(cells, 18u, &links) == FVIZ_OK);
    CHECK(fviz_cell_links_point_count(links) == 18u);
    CHECK(fviz_cell_links_cell_count(links) == 4u);
    CHECK(fviz_cell_links_incidence_count(links) == 32u);
    CHECK(fviz_cell_links_cell_count_for_point(links, 7u) == 4u);
    ids = fviz_cell_links_cells_for_point(links, 7u, &count);
    CHECK(ids != NULL && count == 4u);
    CHECK(ids[0] == 0u && ids[1] == 1u && ids[2] == 2u && ids[3] == 3u);

    CHECK(fviz_cell_adjacency_build(cells, 18u, &adjacency) == FVIZ_OK);
    CHECK(fviz_cell_adjacency_edge_count(adjacency) == 4u);
    CHECK(fviz_cell_adjacency_neighbor_count(adjacency, 0u) == 2u);
    ids = fviz_cell_adjacency_neighbors(adjacency, 0u, &count);
    CHECK(count == 2u && ids[0] == 1u && ids[1] == 2u);
    CHECK(fviz_cell_adjacency_neighbor_count(adjacency, 3u) == 2u);
    ids = fviz_cell_adjacency_neighbors(adjacency, 3u, &count);
    CHECK(count == 2u && ids[0] == 1u && ids[1] == 2u);

    fviz_release(adjacency);
    fviz_release(links);
    fviz_release(cells);
    return 0;
}
