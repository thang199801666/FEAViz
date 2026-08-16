#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizCellTopologyDefinition
{
    FVizCellTypeTraits traits;
    uint8_t edges[12][2];
    uint8_t faces[8][27];
    uint8_t face_sizes[8];
} FVizCellTopologyDefinition;

static FVizBool fviz_cell_topology_definition(FVizCellType type, FVizCellTopologyDefinition* out)
{
    FVizCellTopologyDefinition d;
    (void)memset(&d, 0, sizeof(d));
    switch (type)
    {
        case FVIZ_CELL_VERTEX:
            d.traits.dimension = 0u; d.traits.fixed_point_count = 1u;
            break;
        case FVIZ_CELL_POLY_VERTEX:
            d.traits.dimension = 0u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_LINE:
            d.traits.dimension = 1u; d.traits.fixed_point_count = 2u; d.traits.edge_count = 1u;
            d.edges[0][0] = 0u; d.edges[0][1] = 1u;
            break;
        case FVIZ_CELL_POLY_LINE:
            d.traits.dimension = 1u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_TRIANGLE:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 3u; d.traits.edge_count = 3u; d.traits.face_count = 1u;
            d.edges[0][0] = 0u; d.edges[0][1] = 1u;
            d.edges[1][0] = 1u; d.edges[1][1] = 2u;
            d.edges[2][0] = 2u; d.edges[2][1] = 0u;
            d.face_sizes[0] = 3u; d.faces[0][0] = 0u; d.faces[0][1] = 1u; d.faces[0][2] = 2u;
            break;
        case FVIZ_CELL_TRIANGLE_STRIP:
            d.traits.dimension = 2u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_POLYGON:
            d.traits.dimension = 2u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_QUAD:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 4u; d.traits.edge_count = 4u; d.traits.face_count = 1u;
            d.edges[0][0] = 0u; d.edges[0][1] = 1u;
            d.edges[1][0] = 1u; d.edges[1][1] = 2u;
            d.edges[2][0] = 2u; d.edges[2][1] = 3u;
            d.edges[3][0] = 3u; d.edges[3][1] = 0u;
            d.face_sizes[0] = 4u; d.faces[0][0] = 0u; d.faces[0][1] = 1u; d.faces[0][2] = 2u; d.faces[0][3] = 3u;
            break;
        case FVIZ_CELL_TETRA:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 4u; d.traits.edge_count = 6u; d.traits.face_count = 4u;
            { const uint8_t e[6][2] = {{0,1},{1,2},{2,0},{0,3},{1,3},{2,3}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=3u; d.faces[0][0]=0u; d.faces[0][1]=2u; d.faces[0][2]=1u;
            d.face_sizes[1]=3u; d.faces[1][0]=0u; d.faces[1][1]=1u; d.faces[1][2]=3u;
            d.face_sizes[2]=3u; d.faces[2][0]=1u; d.faces[2][1]=2u; d.faces[2][2]=3u;
            d.face_sizes[3]=3u; d.faces[3][0]=2u; d.faces[3][1]=0u; d.faces[3][2]=3u;
            break;
        case FVIZ_CELL_HEXAHEDRON:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 8u; d.traits.edge_count = 12u; d.traits.face_count = 6u;
            { const uint8_t e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=4u; d.faces[0][0]=0u; d.faces[0][1]=3u; d.faces[0][2]=2u; d.faces[0][3]=1u;
            d.face_sizes[1]=4u; d.faces[1][0]=4u; d.faces[1][1]=5u; d.faces[1][2]=6u; d.faces[1][3]=7u;
            d.face_sizes[2]=4u; d.faces[2][0]=0u; d.faces[2][1]=1u; d.faces[2][2]=5u; d.faces[2][3]=4u;
            d.face_sizes[3]=4u; d.faces[3][0]=1u; d.faces[3][1]=2u; d.faces[3][2]=6u; d.faces[3][3]=5u;
            d.face_sizes[4]=4u; d.faces[4][0]=2u; d.faces[4][1]=3u; d.faces[4][2]=7u; d.faces[4][3]=6u;
            d.face_sizes[5]=4u; d.faces[5][0]=3u; d.faces[5][1]=0u; d.faces[5][2]=4u; d.faces[5][3]=7u;
            break;
        case FVIZ_CELL_WEDGE:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 6u; d.traits.edge_count = 9u; d.traits.face_count = 5u;
            { const uint8_t e[9][2] = {{0,1},{1,2},{2,0},{3,4},{4,5},{5,3},{0,3},{1,4},{2,5}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=3u; d.faces[0][0]=0u; d.faces[0][1]=2u; d.faces[0][2]=1u;
            d.face_sizes[1]=3u; d.faces[1][0]=3u; d.faces[1][1]=4u; d.faces[1][2]=5u;
            d.face_sizes[2]=4u; d.faces[2][0]=0u; d.faces[2][1]=1u; d.faces[2][2]=4u; d.faces[2][3]=3u;
            d.face_sizes[3]=4u; d.faces[3][0]=1u; d.faces[3][1]=2u; d.faces[3][2]=5u; d.faces[3][3]=4u;
            d.face_sizes[4]=4u; d.faces[4][0]=2u; d.faces[4][1]=0u; d.faces[4][2]=3u; d.faces[4][3]=5u;
            break;
        case FVIZ_CELL_PYRAMID:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 5u; d.traits.edge_count = 8u; d.traits.face_count = 5u;
            { const uint8_t e[8][2] = {{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=4u; d.faces[0][0]=0u; d.faces[0][1]=3u; d.faces[0][2]=2u; d.faces[0][3]=1u;
            d.face_sizes[1]=3u; d.faces[1][0]=0u; d.faces[1][1]=1u; d.faces[1][2]=4u;
            d.face_sizes[2]=3u; d.faces[2][0]=1u; d.faces[2][1]=2u; d.faces[2][2]=4u;
            d.face_sizes[3]=3u; d.faces[3][0]=2u; d.faces[3][1]=3u; d.faces[3][2]=4u;
            d.face_sizes[4]=3u; d.faces[4][0]=3u; d.faces[4][1]=0u; d.faces[4][2]=4u;
            break;
        case FVIZ_CELL_QUADRATIC_EDGE:
            d.traits.dimension = 1u; d.traits.fixed_point_count = 3u; d.traits.edge_count = 1u;
            d.edges[0][0] = 0u; d.edges[0][1] = 1u;
            break;
        case FVIZ_CELL_QUADRATIC_TRIANGLE:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 6u; d.traits.edge_count = 3u; d.traits.face_count = 1u;
            { const uint8_t e[3][2] = {{0,1},{1,2},{2,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u;
            { const uint8_t f[6] = {0,1,2,3,4,5}; (void)memcpy(d.faces[0], f, sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_QUAD:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 8u; d.traits.edge_count = 4u; d.traits.face_count = 1u;
            { const uint8_t e[4][2] = {{0,1},{1,2},{2,3},{3,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=8u;
            { const uint8_t f[8] = {0,1,2,3,4,5,6,7}; (void)memcpy(d.faces[0], f, sizeof(f)); }
            break;
        case FVIZ_CELL_BIQUADRATIC_QUAD:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 9u; d.traits.edge_count = 4u; d.traits.face_count = 1u;
            { const uint8_t e[4][2] = {{0,1},{1,2},{2,3},{3,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=9u;
            { const uint8_t f[9] = {0,1,2,3,4,5,6,7,8}; (void)memcpy(d.faces[0], f, sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_TETRA:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 10u; d.traits.edge_count = 6u; d.traits.face_count = 4u;
            { const uint8_t e[6][2] = {{0,1},{1,2},{2,0},{0,3},{1,3},{2,3}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,2,1,6,5,4}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={0,1,3,4,8,7}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=6u; { const uint8_t f[6]={1,2,3,5,9,8}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=6u; { const uint8_t f[6]={2,0,3,6,7,9}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_HEXAHEDRON:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 20u; d.traits.edge_count = 12u; d.traits.face_count = 6u;
            { const uint8_t e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=8u; { const uint8_t f[8]={0,3,2,1,11,10,9,8}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=8u; { const uint8_t f[8]={4,5,6,7,12,13,14,15}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=8u; { const uint8_t f[8]={0,1,5,4,8,17,12,16}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=8u; { const uint8_t f[8]={1,2,6,5,9,18,13,17}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=8u; { const uint8_t f[8]={2,3,7,6,10,19,14,18}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=8u; { const uint8_t f[8]={3,0,4,7,11,16,15,19}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_WEDGE:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 15u; d.traits.edge_count = 9u; d.traits.face_count = 5u;
            { const uint8_t e[9][2] = {{0,1},{1,2},{2,0},{3,4},{4,5},{5,3},{0,3},{1,4},{2,5}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,2,1,8,7,6}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={3,4,5,9,10,11}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=8u; { const uint8_t f[8]={0,1,4,3,6,13,9,12}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=8u; { const uint8_t f[8]={1,2,5,4,7,14,10,13}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=8u; { const uint8_t f[8]={2,0,3,5,8,12,11,14}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_PYRAMID:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 13u; d.traits.edge_count = 8u; d.traits.face_count = 5u;
            { const uint8_t e[8][2] = {{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=8u; { const uint8_t f[8]={0,3,2,1,8,7,6,5}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={0,1,4,5,10,9}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=6u; { const uint8_t f[6]={1,2,4,6,11,10}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=6u; { const uint8_t f[6]={2,3,4,7,12,11}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=6u; { const uint8_t f[6]={3,0,4,8,9,12}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            break;
        case FVIZ_CELL_EMPTY:
            d.traits.dimension = 0u; d.traits.fixed_point_count = 0u;
            break;
        case FVIZ_CELL_PIXEL:
            /* VTK PIXEL: axis-aligned quad, points ordered (0,0),(1,0),(1,1),(0,1). */
            d.traits.dimension = 2u; d.traits.fixed_point_count = 4u; d.traits.edge_count = 4u; d.traits.face_count = 1u;
            { const uint8_t e[4][2] = {{0,1},{1,2},{2,3},{3,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=4u; { const uint8_t f[4]={0,1,2,3}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            break;
        case FVIZ_CELL_VOXEL:
            /* VTK VOXEL: axis-aligned hex, points (x,y,z) across 0/1 per axis. */
            d.traits.dimension = 3u; d.traits.fixed_point_count = 8u; d.traits.edge_count = 12u; d.traits.face_count = 6u;
            { const uint8_t e[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=4u; { const uint8_t f[4]={0,1,3,2}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=4u; { const uint8_t f[4]={4,5,7,6}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=4u; { const uint8_t f[4]={0,1,5,4}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=4u; { const uint8_t f[4]={1,3,7,5}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=4u; { const uint8_t f[4]={3,2,6,7}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=4u; { const uint8_t f[4]={2,0,4,6}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            break;
        case FVIZ_CELL_PENTAGONAL_PRISM:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 10u; d.traits.edge_count = 15u; d.traits.face_count = 7u;
            { const uint8_t e[15][2] = {
                {0,1},{1,2},{2,3},{3,4},{4,0},{5,6},{6,7},{7,8},{8,9},{9,5},
                {0,5},{1,6},{2,7},{3,8},{4,9}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=5u; { const uint8_t f[5]={0,1,2,3,4}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=5u; { const uint8_t f[5]={5,6,7,8,9}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=4u; { const uint8_t f[4]={0,1,6,5}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=4u; { const uint8_t f[4]={1,2,7,6}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=4u; { const uint8_t f[4]={2,3,8,7}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=4u; { const uint8_t f[4]={3,4,9,8}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            d.face_sizes[6]=4u; { const uint8_t f[4]={4,0,5,9}; (void)memcpy(d.faces[6],f,sizeof(f)); }
            break;
        case FVIZ_CELL_HEXAGONAL_PRISM:
            d.traits.dimension = 3u; d.traits.fixed_point_count = 12u; d.traits.edge_count = 18u; d.traits.face_count = 8u;
            { const uint8_t e[18][2] = {
                {0,1},{1,2},{2,3},{3,4},{4,5},{5,0},{6,7},{7,8},{8,9},{9,10},{10,11},{11,6},
                {0,6},{1,7},{2,8},{3,9},{4,10},{5,11}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,1,2,3,4,5}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={6,7,8,9,10,11}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=4u; { const uint8_t f[4]={0,1,7,6}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=4u; { const uint8_t f[4]={1,2,8,7}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=4u; { const uint8_t f[4]={2,3,9,8}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=4u; { const uint8_t f[4]={3,4,10,9}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            d.face_sizes[6]=4u; { const uint8_t f[4]={4,5,11,10}; (void)memcpy(d.faces[6],f,sizeof(f)); }
            d.face_sizes[7]=4u; { const uint8_t f[4]={5,0,6,11}; (void)memcpy(d.faces[7],f,sizeof(f)); }
            break;
        case FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON:
            /* 27-node hex: 8 corners, 12 edge mids, 6 face centers, 1 body center. */
            d.traits.dimension = 3u; d.traits.fixed_point_count = 27u; d.traits.edge_count = 12u; d.traits.face_count = 6u;
            { const uint8_t e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=9u; { const uint8_t f[9]={0,3,2,1,11,10,9,8,20}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=9u; { const uint8_t f[9]={4,5,6,7,12,13,14,15,22}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=9u; { const uint8_t f[9]={0,1,5,4,8,17,12,16,21}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=9u; { const uint8_t f[9]={1,2,6,5,9,18,13,17,23}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=9u; { const uint8_t f[9]={2,3,7,6,10,19,14,18,24}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=9u; { const uint8_t f[9]={3,0,4,7,11,16,15,19,25}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_LINEAR_QUAD:
            /* 6-node: 4 corners + 2 mids on opposite edges (VTK order). */
            d.traits.dimension = 2u; d.traits.fixed_point_count = 6u; d.traits.edge_count = 4u; d.traits.face_count = 1u;
            { const uint8_t e[4][2] = {{0,1},{1,2},{2,3},{3,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,1,2,3,4,5}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            break;
        case FVIZ_CELL_QUADRATIC_LINEAR_WEDGE:
            /* 12-node: 6 corners + 6 triangular-face mids. */
            d.traits.dimension = 3u; d.traits.fixed_point_count = 12u; d.traits.edge_count = 9u; d.traits.face_count = 5u;
            { const uint8_t e[9][2] = {{0,1},{1,2},{2,0},{3,4},{4,5},{5,3},{0,3},{1,4},{2,5}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,2,1,8,7,6}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={3,4,5,9,10,11}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=4u; { const uint8_t f[4]={0,1,4,3}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=4u; { const uint8_t f[4]={1,2,5,4}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=4u; { const uint8_t f[4]={2,0,3,5}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            break;
        case FVIZ_CELL_BIQUADRATIC_QUADRATIC_WEDGE:
            /* 18-node: 6 corners, 6 triangular mids, 6 quadrilateral mids. */
            d.traits.dimension = 3u; d.traits.fixed_point_count = 18u; d.traits.edge_count = 9u; d.traits.face_count = 5u;
            { const uint8_t e[9][2] = {{0,1},{1,2},{2,0},{3,4},{4,5},{5,3},{0,3},{1,4},{2,5}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=6u; { const uint8_t f[6]={0,2,1,8,7,6}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=6u; { const uint8_t f[6]={3,4,5,9,10,11}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=8u; { const uint8_t f[8]={0,1,4,3,12,16,9,15}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=8u; { const uint8_t f[8]={1,2,5,4,13,17,10,16}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=8u; { const uint8_t f[8]={2,0,3,5,14,15,11,17}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            break;
        case FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON:
            /* 27-node (same layout as triquadratic hex). */
            d.traits.dimension = 3u; d.traits.fixed_point_count = 27u; d.traits.edge_count = 12u; d.traits.face_count = 6u;
            { const uint8_t e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=9u; { const uint8_t f[9]={0,3,2,1,11,10,9,8,20}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            d.face_sizes[1]=9u; { const uint8_t f[9]={4,5,6,7,12,13,14,15,22}; (void)memcpy(d.faces[1],f,sizeof(f)); }
            d.face_sizes[2]=9u; { const uint8_t f[9]={0,1,5,4,8,17,12,16,21}; (void)memcpy(d.faces[2],f,sizeof(f)); }
            d.face_sizes[3]=9u; { const uint8_t f[9]={1,2,6,5,9,18,13,17,23}; (void)memcpy(d.faces[3],f,sizeof(f)); }
            d.face_sizes[4]=9u; { const uint8_t f[9]={2,3,7,6,10,19,14,18,24}; (void)memcpy(d.faces[4],f,sizeof(f)); }
            d.face_sizes[5]=9u; { const uint8_t f[9]={3,0,4,7,11,16,15,19,25}; (void)memcpy(d.faces[5],f,sizeof(f)); }
            break;
        case FVIZ_CELL_BIQUADRATIC_TRIANGLE:
            d.traits.dimension = 2u; d.traits.fixed_point_count = 7u; d.traits.edge_count = 3u; d.traits.face_count = 1u;
            { const uint8_t e[3][2] = {{0,1},{1,2},{2,0}}; (void)memcpy(d.edges, e, sizeof(e)); }
            d.face_sizes[0]=7u; { const uint8_t f[7]={0,1,2,3,4,5,6}; (void)memcpy(d.faces[0],f,sizeof(f)); }
            break;
        case FVIZ_CELL_CUBIC_LINE:
            d.traits.dimension = 1u; d.traits.fixed_point_count = 4u; d.traits.edge_count = 1u;
            d.edges[0][0] = 0u; d.edges[0][1] = 1u;
            break;
        case FVIZ_CELL_QUADRATIC_POLYGON:
            d.traits.dimension = 2u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_CONVEX_POINT_SET:
            d.traits.dimension = 3u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        case FVIZ_CELL_POLYHEDRON:
            d.traits.dimension = 3u; d.traits.variable_point_count = FVIZ_TRUE;
            break;
        default:
            return FVIZ_FALSE;
    }
    if (out != NULL) *out = d;
    return FVIZ_TRUE;
}

FVizBool fviz_cell_type_is_supported(FVizCellType type)
{
    return fviz_cell_topology_definition(type, NULL);
}

FVizCellTypeTraits fviz_cell_type_traits(FVizCellType type)
{
    FVizCellTopologyDefinition d;
    if (fviz_cell_topology_definition(type, &d) == FVIZ_FALSE)
    {
        FVizCellTypeTraits empty;
        (void)memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return d.traits;
}

FVizBool fviz_cell_type_accepts_point_count(FVizCellType type, FVizSize point_count)
{
    const FVizCellTypeTraits traits = fviz_cell_type_traits(type);
    if (fviz_cell_type_is_supported(type) == FVIZ_FALSE) return FVIZ_FALSE;
    if (traits.variable_point_count == FVIZ_FALSE) return point_count == (FVizSize)traits.fixed_point_count ? FVIZ_TRUE : FVIZ_FALSE;
    switch (type)
    {
        case FVIZ_CELL_POLY_VERTEX: return point_count >= 1u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_CELL_POLY_LINE: return point_count >= 2u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_CELL_TRIANGLE_STRIP:
        case FVIZ_CELL_POLYGON:
        case FVIZ_CELL_QUADRATIC_POLYGON: return point_count >= 3u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_CELL_CONVEX_POINT_SET:
        case FVIZ_CELL_POLYHEDRON: return point_count >= 4u ? FVIZ_TRUE : FVIZ_FALSE;
        default: return FVIZ_FALSE;
    }
}

FVizResult fviz_cell_type_edge(FVizCellType type, uint32_t edge_index, uint32_t out_local_point_ids[2])
{
    FVizCellTopologyDefinition d;
    if (out_local_point_ids == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "edge output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_cell_topology_definition(type, &d) == FVIZ_FALSE || edge_index >= d.traits.edge_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "cell edge does not exist");
        return FVIZ_ERROR_NOT_FOUND;
    }
    out_local_point_ids[0] = d.edges[edge_index][0];
    out_local_point_ids[1] = d.edges[edge_index][1];
    return FVIZ_OK;
}

FVizResult fviz_cell_type_face(
    FVizCellType type,
    uint32_t face_index,
    uint32_t* out_local_point_ids,
    uint32_t capacity,
    uint32_t* out_point_count)
{
    FVizCellTopologyDefinition d;
    uint32_t count;
    if (out_point_count != NULL) *out_point_count = 0u;
    if (fviz_cell_topology_definition(type, &d) == FVIZ_FALSE || face_index >= d.traits.face_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "cell face does not exist");
        return FVIZ_ERROR_NOT_FOUND;
    }
    count = d.face_sizes[face_index];
    if (out_point_count != NULL) *out_point_count = count;
    if (out_local_point_ids == NULL) return FVIZ_OK;
    if (capacity < count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell face output capacity is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    {
        uint32_t i;
        for (i = 0u; i < count; ++i) out_local_point_ids[i] = d.faces[face_index][i];
    }
    return FVIZ_OK;
}


FVizResult fviz_cell_type_linear_weights(
    FVizCellType type,
    FVizVec3 parametric,
    double* out_weights,
    FVizSize capacity,
    FVizSize* out_weight_count)
{
    FVizSize count = 0u;
    if (out_weight_count != NULL) *out_weight_count = 0u;
    switch (type)
    {
        case FVIZ_CELL_VERTEX: count = 1u; break;
        case FVIZ_CELL_LINE: count = 2u; break;
        case FVIZ_CELL_TRIANGLE: count = 3u; break;
        case FVIZ_CELL_QUAD:
        case FVIZ_CELL_PIXEL: count = 4u; break;
        case FVIZ_CELL_TETRA: count = 4u; break;
        case FVIZ_CELL_HEXAHEDRON:
        case FVIZ_CELL_VOXEL: count = 8u; break;
        case FVIZ_CELL_WEDGE: count = 6u; break;
        case FVIZ_CELL_PENTAGONAL_PRISM: count = 10u; break;
        case FVIZ_CELL_HEXAGONAL_PRISM: count = 12u; break;
        default:
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "linear interpolation weights are not available for this cell type");
            return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (out_weight_count != NULL) *out_weight_count = count;
    if (out_weights == NULL)
    {
        if (capacity != 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "weight output is NULL with non-zero capacity");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        return FVIZ_OK;
    }
    if (capacity < count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "linear weight output capacity is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    switch (type)
    {
        case FVIZ_CELL_VERTEX:
            out_weights[0] = 1.0;
            break;
        case FVIZ_CELL_LINE:
            out_weights[0] = 0.5 * (1.0 - (double)parametric.x);
            out_weights[1] = 0.5 * (1.0 + (double)parametric.x);
            break;
        case FVIZ_CELL_TRIANGLE:
            out_weights[0] = 1.0 - (double)parametric.x - (double)parametric.y;
            out_weights[1] = (double)parametric.x;
            out_weights[2] = (double)parametric.y;
            break;
        case FVIZ_CELL_QUAD:
        case FVIZ_CELL_PIXEL:
        {
            const double r = (double)parametric.x;
            const double q = (double)parametric.y;
            out_weights[0] = 0.25 * (1.0-r) * (1.0-q);
            out_weights[1] = 0.25 * (1.0+r) * (1.0-q);
            out_weights[2] = 0.25 * (1.0+r) * (1.0+q);
            out_weights[3] = 0.25 * (1.0-r) * (1.0+q);
            break;
        }
        case FVIZ_CELL_TETRA:
            out_weights[0] = 1.0 - (double)parametric.x - (double)parametric.y - (double)parametric.z;
            out_weights[1] = (double)parametric.x;
            out_weights[2] = (double)parametric.y;
            out_weights[3] = (double)parametric.z;
            break;
        case FVIZ_CELL_HEXAHEDRON:
        case FVIZ_CELL_VOXEL:
        {
            static const double signs[8][3] = {
                {-1.0,-1.0,-1.0}, { 1.0,-1.0,-1.0}, { 1.0, 1.0,-1.0}, {-1.0, 1.0,-1.0},
                {-1.0,-1.0, 1.0}, { 1.0,-1.0, 1.0}, { 1.0, 1.0, 1.0}, {-1.0, 1.0, 1.0}
            };
            FVizSize i;
            const double r = (double)parametric.x;
            const double q = (double)parametric.y;
            const double t = (double)parametric.z;
            for (i = 0u; i < 8u; ++i)
                out_weights[i] = 0.125 * (1.0 + signs[i][0]*r) * (1.0 + signs[i][1]*q) * (1.0 + signs[i][2]*t);
            break;
        }
        case FVIZ_CELL_WEDGE:
        {
            const double r = (double)parametric.x;
            const double q = (double)parametric.y;
            const double t = (double)parametric.z;
            const double base = 1.0 - r - q;
            const double lower = 0.5 * (1.0 - t);
            const double upper = 0.5 * (1.0 + t);
            out_weights[0] = base * lower;
            out_weights[1] = r * lower;
            out_weights[2] = q * lower;
            out_weights[3] = base * upper;
            out_weights[4] = r * upper;
            out_weights[5] = q * upper;
            break;
        }
        case FVIZ_CELL_PENTAGONAL_PRISM:
        case FVIZ_CELL_HEXAGONAL_PRISM:
        {
            /* Prism reference: polygon base in the (r,q) plane, extruded along t.
             * Compute bilinear-in-polygon weights for the base then interpolate
             * between lower/top faces. For simplicity the base uses a fan from
             * the polygon centroid so weights sum to one at any (r,q). */
            const FVizSize base_count = type == FVIZ_CELL_PENTAGONAL_PRISM ? 5u : 6u;
            const double lower = 0.5 * (1.0 - (double)parametric.z);
            const double upper = 0.5 * (1.0 + (double)parametric.z);
            FVizSize i;
            double total = 0.0;
            double base_weights[6];
            for (i = 0u; i < base_count; ++i) base_weights[i] = 1.0 / (double)base_count;
            /* Distribute with a simple radial falloff so (0,0) is the centroid. */
            for (i = 0u; i < base_count; ++i)
            {
                const double angle = 2.0 * 3.14159265358979323846 * (double)i / (double)base_count;
                const double contribution = 1.0 + (double)parametric.x * cos(angle) + (double)parametric.y * sin(angle);
                base_weights[i] = contribution < 0.0 ? 0.0 : contribution;
                total += base_weights[i];
            }
            for (i = 0u; i < base_count; ++i)
            {
                base_weights[i] /= total;
                out_weights[i] = base_weights[i] * lower;
                out_weights[i + base_count] = base_weights[i] * upper;
            }
            break;
        }
        default: break;
    }
    return FVIZ_OK;
}

FVizResult fviz_cell_type_shape_weights(
    FVizCellType type,
    FVizVec3 parametric,
    double* out_weights,
    FVizSize capacity,
    FVizSize* out_weight_count)
{
    FVizSize count = 0u;
    if (out_weight_count != NULL) *out_weight_count = 0u;
    switch (type)
    {
        case FVIZ_CELL_CUBIC_LINE: count = 4u; break;
        case FVIZ_CELL_QUADRATIC_LINEAR_QUAD: count = 6u; break;
        case FVIZ_CELL_BIQUADRATIC_TRIANGLE: count = 7u; break;
        case FVIZ_CELL_QUADRATIC_EDGE: count = 3u; break;
        case FVIZ_CELL_QUADRATIC_TRIANGLE: count = 6u; break;
        case FVIZ_CELL_QUADRATIC_QUAD: count = 8u; break;
        case FVIZ_CELL_BIQUADRATIC_QUAD: count = 9u; break;
        case FVIZ_CELL_QUADRATIC_TETRA: count = 10u; break;
        case FVIZ_CELL_QUADRATIC_LINEAR_WEDGE: count = 12u; break;
        case FVIZ_CELL_QUADRATIC_HEXAHEDRON: count = 20u; break;
        case FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON:
        case FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON: count = 27u; break;
        default:
            return fviz_cell_type_linear_weights(type, parametric, out_weights, capacity, out_weight_count);
    }
    if (out_weight_count != NULL) *out_weight_count = count;
    if (out_weights == NULL)
    {
        if (capacity != 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "shape weight output is NULL with non-zero capacity");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        return FVIZ_OK;
    }
    if (capacity < count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "shape weight output capacity is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    switch (type)
    {
        case FVIZ_CELL_QUADRATIC_EDGE:
        {
            const double r = (double)parametric.x;
            out_weights[0] = 0.5 * r * (r - 1.0);
            out_weights[1] = 0.5 * r * (r + 1.0);
            out_weights[2] = 1.0 - r * r;
            break;
        }
        case FVIZ_CELL_QUADRATIC_TRIANGLE:
        {
            const double l0 = 1.0 - (double)parametric.x - (double)parametric.y;
            const double l1 = (double)parametric.x;
            const double l2 = (double)parametric.y;
            out_weights[0] = l0 * (2.0*l0 - 1.0);
            out_weights[1] = l1 * (2.0*l1 - 1.0);
            out_weights[2] = l2 * (2.0*l2 - 1.0);
            out_weights[3] = 4.0*l0*l1;
            out_weights[4] = 4.0*l1*l2;
            out_weights[5] = 4.0*l2*l0;
            break;
        }
        case FVIZ_CELL_QUADRATIC_QUAD:
        {
            const double r = (double)parametric.x;
            const double s = (double)parametric.y;
            out_weights[0] = -0.25*(1.0-r)*(1.0-s)*(1.0+r+s);
            out_weights[1] = -0.25*(1.0+r)*(1.0-s)*(1.0-r+s);
            out_weights[2] = -0.25*(1.0+r)*(1.0+s)*(1.0-r-s);
            out_weights[3] = -0.25*(1.0-r)*(1.0+s)*(1.0+r-s);
            out_weights[4] =  0.5*(1.0-r*r)*(1.0-s);
            out_weights[5] =  0.5*(1.0+r)*(1.0-s*s);
            out_weights[6] =  0.5*(1.0-r*r)*(1.0+s);
            out_weights[7] =  0.5*(1.0-r)*(1.0-s*s);
            break;
        }
        case FVIZ_CELL_BIQUADRATIC_QUAD:
        {
            const double r = (double)parametric.x;
            const double s = (double)parametric.y;
            const double rm = 0.5*r*(r-1.0), rc = 1.0-r*r, rp = 0.5*r*(r+1.0);
            const double sm = 0.5*s*(s-1.0), sc = 1.0-s*s, sp = 0.5*s*(s+1.0);
            out_weights[0] = rm*sm; out_weights[1] = rp*sm;
            out_weights[2] = rp*sp; out_weights[3] = rm*sp;
            out_weights[4] = rc*sm; out_weights[5] = rp*sc;
            out_weights[6] = rc*sp; out_weights[7] = rm*sc;
            out_weights[8] = rc*sc;
            break;
        }
        case FVIZ_CELL_QUADRATIC_TETRA:
        {
            const double l0 = 1.0 - (double)parametric.x - (double)parametric.y - (double)parametric.z;
            const double l1 = (double)parametric.x;
            const double l2 = (double)parametric.y;
            const double l3 = (double)parametric.z;
            out_weights[0] = l0*(2.0*l0-1.0);
            out_weights[1] = l1*(2.0*l1-1.0);
            out_weights[2] = l2*(2.0*l2-1.0);
            out_weights[3] = l3*(2.0*l3-1.0);
            out_weights[4] = 4.0*l0*l1;
            out_weights[5] = 4.0*l1*l2;
            out_weights[6] = 4.0*l2*l0;
            out_weights[7] = 4.0*l0*l3;
            out_weights[8] = 4.0*l1*l3;
            out_weights[9] = 4.0*l2*l3;
            break;
        }
        case FVIZ_CELL_QUADRATIC_HEXAHEDRON:
        {
            static const double signs[8][3] = {
                {-1.0,-1.0,-1.0}, {1.0,-1.0,-1.0}, {1.0,1.0,-1.0}, {-1.0,1.0,-1.0},
                {-1.0,-1.0,1.0}, {1.0,-1.0,1.0}, {1.0,1.0,1.0}, {-1.0,1.0,1.0}
            };
            FVizSize i;
            const double r = (double)parametric.x, s = (double)parametric.y, t = (double)parametric.z;
            for (i=0u;i<8u;++i)
            {
                const double ri=signs[i][0], si=signs[i][1], ti=signs[i][2];
                out_weights[i]=0.125*(1.0+ri*r)*(1.0+si*s)*(1.0+ti*t)*(ri*r+si*s+ti*t-2.0);
            }
            out_weights[8]  = 0.25*(1.0-r*r)*(1.0-s)*(1.0-t);
            out_weights[9]  = 0.25*(1.0+r)*(1.0-s*s)*(1.0-t);
            out_weights[10] = 0.25*(1.0-r*r)*(1.0+s)*(1.0-t);
            out_weights[11] = 0.25*(1.0-r)*(1.0-s*s)*(1.0-t);
            out_weights[12] = 0.25*(1.0-r*r)*(1.0-s)*(1.0+t);
            out_weights[13] = 0.25*(1.0+r)*(1.0-s*s)*(1.0+t);
            out_weights[14] = 0.25*(1.0-r*r)*(1.0+s)*(1.0+t);
            out_weights[15] = 0.25*(1.0-r)*(1.0-s*s)*(1.0+t);
            out_weights[16] = 0.25*(1.0-r)*(1.0-s)*(1.0-t*t);
            out_weights[17] = 0.25*(1.0+r)*(1.0-s)*(1.0-t*t);
            out_weights[18] = 0.25*(1.0+r)*(1.0+s)*(1.0-t*t);
            out_weights[19] = 0.25*(1.0-r)*(1.0+s)*(1.0-t*t);
            break;
        }
        case FVIZ_CELL_CUBIC_LINE:
        {
            const double r = (double)parametric.x;
            out_weights[0] = 0.0625 * (1.0 - r) * (9.0 * r * r - 1.0);
            out_weights[1] = 0.0625 * (1.0 + r) * (9.0 * r * r - 1.0);
            out_weights[2] = 0.5625 * (1.0 - r) * (1.0 - 3.0 * r) * (1.0 + r);
            out_weights[3] = 0.5625 * (1.0 + r) * (1.0 + 3.0 * r) * (1.0 - r);
            break;
        }
        case FVIZ_CELL_QUADRATIC_LINEAR_QUAD:
        {
            /* 6-node: corners 0-3 quadratic in r, linear in s; mids 4,5 on
             * the r-mid line. Corner = rm/rp * 0.5*(1±s), mid = rc * 0.5*(1±s). */
            const double r = (double)parametric.x;
            const double s = (double)parametric.y;
            const double rm = 0.5 * r * (r - 1.0), rc = 1.0 - r * r, rp = 0.5 * r * (r + 1.0);
            const double lower = 0.5 * (1.0 - s);
            const double upper = 0.5 * (1.0 + s);
            out_weights[0] = rm * lower;
            out_weights[1] = rp * lower;
            out_weights[2] = rp * upper;
            out_weights[3] = rm * upper;
            out_weights[4] = rc * lower;
            out_weights[5] = rc * upper;
            break;
        }
        case FVIZ_CELL_BIQUADRATIC_TRIANGLE:
        {
            /* 7-node biquadratic triangle (VTK): corners + 3 edge mids + 1
             * centroid. Corner picks up a +3*l0*l1*l2 term; edge mids carry the
             * -12*l0*l1*l2 correction. */
            const double l0 = 1.0 - (double)parametric.x - (double)parametric.y;
            const double l1 = (double)parametric.x;
            const double l2 = (double)parametric.y;
            const double product = l0 * l1 * l2;
            out_weights[0] = l0 * (2.0 * l0 - 1.0) + 3.0 * product;
            out_weights[1] = l1 * (2.0 * l1 - 1.0) + 3.0 * product;
            out_weights[2] = l2 * (2.0 * l2 - 1.0) + 3.0 * product;
            out_weights[3] = 4.0 * l0 * l1 - 12.0 * product;
            out_weights[4] = 4.0 * l1 * l2 - 12.0 * product;
            out_weights[5] = 4.0 * l2 * l0 - 12.0 * product;
            out_weights[6] = 27.0 * product;
            break;
        }
        case FVIZ_CELL_QUADRATIC_LINEAR_WEDGE:
        {
            const double r = (double)parametric.x, q = (double)parametric.y, t = (double)parametric.z;
            const double l0 = 1.0 - r - q, l1 = r, l2 = q;
            const double lower = 0.5 * (1.0 - t), upper = 0.5 * (1.0 + t);
            out_weights[0] = l0 * (2.0 * l0 - 1.0) * lower;
            out_weights[1] = l1 * (2.0 * l1 - 1.0) * lower;
            out_weights[2] = l2 * (2.0 * l2 - 1.0) * lower;
            out_weights[3] = l0 * (2.0 * l0 - 1.0) * upper;
            out_weights[4] = l1 * (2.0 * l1 - 1.0) * upper;
            out_weights[5] = l2 * (2.0 * l2 - 1.0) * upper;
            out_weights[6] = 4.0 * l1 * l2 * lower;
            out_weights[7] = 4.0 * l2 * l0 * lower;
            out_weights[8] = 4.0 * l0 * l1 * lower;
            out_weights[9] = 4.0 * l1 * l2 * upper;
            out_weights[10] = 4.0 * l2 * l0 * upper;
            out_weights[11] = 4.0 * l0 * l1 * upper;
            break;
        }
        case FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON:
        case FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON:
        {
            /* 27-node hex as a full triquadratic Lagrangian product
             * N = L_i(r) * L_j(s) * L_k(t) with L_0 = -0.5r(1-r), L_1 = 1-r^2,
             * L_2 = 0.5r(1+r). VTK node ordering: 0-7 corners, 8-19 edge mids,
             * 20-25 face centers, 26 body center. */
            const double r = (double)parametric.x, s = (double)parametric.y, t = (double)parametric.z;
            const double L0r = -0.5 * r * (1.0 - r), L1r = 1.0 - r * r, L2r = 0.5 * r * (1.0 + r);
            const double L0s = -0.5 * s * (1.0 - s), L1s = 1.0 - s * s, L2s = 0.5 * s * (1.0 + s);
            const double L0t = -0.5 * t * (1.0 - t), L1t = 1.0 - t * t, L2t = 0.5 * t * (1.0 + t);
            FVizSize i;
            for (i = 0u; i < 27u; ++i) out_weights[i] = 0.0;
            out_weights[0]  = L0r * L0s * L0t; out_weights[1]  = L2r * L0s * L0t;
            out_weights[2]  = L2r * L2s * L0t; out_weights[3]  = L0r * L2s * L0t;
            out_weights[4]  = L0r * L0s * L2t; out_weights[5]  = L2r * L0s * L2t;
            out_weights[6]  = L2r * L2s * L2t; out_weights[7]  = L0r * L2s * L2t;
            out_weights[8]  = L1r * L0s * L0t; out_weights[9]  = L2r * L1s * L0t;
            out_weights[10] = L1r * L2s * L0t; out_weights[11] = L0r * L1s * L0t;
            out_weights[12] = L1r * L0s * L2t; out_weights[13] = L2r * L1s * L2t;
            out_weights[14] = L1r * L2s * L2t; out_weights[15] = L0r * L1s * L2t;
            out_weights[16] = L0r * L0s * L1t; out_weights[17] = L2r * L0s * L1t;
            out_weights[18] = L2r * L2s * L1t; out_weights[19] = L0r * L2s * L1t;
            out_weights[20] = L1r * L1s * L0t; out_weights[21] = L1r * L0s * L1t;
            out_weights[22] = L1r * L1s * L2t; out_weights[23] = L2r * L1s * L1t;
            out_weights[24] = L1r * L2s * L1t; out_weights[25] = L0r * L1s * L1t;
            out_weights[26] = L1r * L1s * L1t;
            break;
        }
        case FVIZ_CELL_QUADRATIC_WEDGE:
        case FVIZ_CELL_BIQUADRATIC_QUADRATIC_WEDGE:
            /* 15-node and 18-node wedges: topology is defined, but their
             * serendipity shape functions are not yet implemented. */
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                "higher-order wedge shape weights are not yet available");
            return FVIZ_ERROR_NOT_SUPPORTED;
        default:
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "shape weights are not available for this cell type");
            return FVIZ_ERROR_NOT_SUPPORTED;
    }
    return FVIZ_OK;
}
