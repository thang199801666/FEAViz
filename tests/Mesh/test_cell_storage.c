#include <stdint.h>
#include <stdio.h>

#include <FViz/Core/FVizObject.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    FVizCellArray* cells = NULL;
    FVizCellArray* copy = NULL;
    const FVizId ids[3] = {UINT64_C(7), UINT64_C(9), UINT64_C(0x100000005)};
    FVizId copied[3] = {0u,0u,0u};
    FVizId id = FVIZ_INVALID_ID;
    uint32_t edge[2];
    uint32_t face[4];
    uint32_t face_count = 0u;
    FVizCellTypeTraits traits;

    CHECK(fviz_cell_array_create(&cells) == FVIZ_OK);
    CHECK(fviz_cell_array_id_storage(cells) == FVIZ_ID_STORAGE_UINT32);
    CHECK(fviz_cell_array_append_ids(cells, FVIZ_CELL_TRIANGLE, 3u, ids) == FVIZ_OK);
    CHECK(fviz_cell_array_id_storage(cells) == FVIZ_ID_STORAGE_UINT64);
    CHECK(fviz_cell_array_point_ids(cells, 0u) == NULL);
    CHECK(fviz_cell_array_point_ids64(cells, 0u) != NULL);
    CHECK(fviz_cell_array_point_id(cells, 0u, 2u, &id) == FVIZ_OK);
    CHECK(id == ids[2]);
    CHECK(fviz_cell_array_copy_point_ids(cells, 0u, copied, 3u) == FVIZ_OK);
    CHECK(copied[0] == ids[0] && copied[1] == ids[1] && copied[2] == ids[2]);
    CHECK(fviz_cell_array_convert_id_storage(cells, FVIZ_ID_STORAGE_UINT32) == FVIZ_ERROR_OVERFLOW);
    CHECK(fviz_cell_array_id_storage(cells) == FVIZ_ID_STORAGE_UINT64);
    CHECK(fviz_cell_array_deep_copy(cells, &copy) == FVIZ_OK);
    CHECK(fviz_cell_array_id_storage(copy) == FVIZ_ID_STORAGE_UINT64);
    CHECK(fviz_cell_array_point_id(copy, 0u, 2u, &id) == FVIZ_OK && id == ids[2]);

    traits = fviz_cell_type_traits(FVIZ_CELL_HEXAHEDRON);
    CHECK(traits.dimension == 3u && traits.fixed_point_count == 8u && traits.edge_count == 12u && traits.face_count == 6u);
    CHECK(fviz_cell_type_edge(FVIZ_CELL_HEXAHEDRON, 8u, edge) == FVIZ_OK);
    CHECK(edge[0] == 0u && edge[1] == 4u);
    CHECK(fviz_cell_type_face(FVIZ_CELL_HEXAHEDRON, 0u, face, 4u, &face_count) == FVIZ_OK);
    CHECK(face_count == 4u && face[0] == 0u && face[1] == 3u && face[2] == 2u && face[3] == 1u);

    {
        double weights[8] = {0.0};
        FVizSize weight_count = 0u;
        FVizVec3 parametric = fviz_vec3(0.25f, -0.5f, 0.5f);
        double sum = 0.0;
        FVizSize wi;
        CHECK(fviz_cell_type_linear_weights(FVIZ_CELL_HEXAHEDRON, parametric, weights, 8u, &weight_count) == FVIZ_OK);
        CHECK(weight_count == 8u);
        for (wi = 0u; wi < weight_count; ++wi) sum += weights[wi];
        CHECK(sum > 0.999999 && sum < 1.000001);
        CHECK(fviz_cell_type_linear_weights(FVIZ_CELL_TETRA, fviz_vec3(0.2f,0.3f,0.1f), weights, 8u, &weight_count) == FVIZ_OK);
        CHECK(weight_count == 4u);
        CHECK(weights[0] > 0.399999 && weights[0] < 0.400001);
    }

    fviz_release(copy);
    fviz_release(cells);
    return 0;
}
