#ifndef FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PIECE_FILTER_H
#define FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PIECE_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizUnstructuredGridPieceFilter FVizUnstructuredGridPieceFilter;
#define FVIZ_TYPE_UNSTRUCTURED_GRID_PIECE_FILTER UINT64_C(0x8F34C721D9AB560E)

FVIZ_API FVizResult fviz_unstructured_grid_piece_filter_create(
    FVizUnstructuredGridPieceFilter** out_filter);
FVIZ_API FVizResult fviz_unstructured_grid_piece_filter_set_input_data(
    FVizUnstructuredGridPieceFilter* filter, FVizUnstructuredGrid* input);
FVIZ_API FVizResult fviz_unstructured_grid_piece_filter_set_input_connection(
    FVizUnstructuredGridPieceFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_unstructured_grid_piece_filter_algorithm(
    FVizUnstructuredGridPieceFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_unstructured_grid_piece_filter_output_port(
    FVizUnstructuredGridPieceFilter* filter);
FVIZ_API FVizUnstructuredGrid* fviz_unstructured_grid_piece_filter_output(
    FVizUnstructuredGridPieceFilter* filter);
FVIZ_API FVizResult fviz_unstructured_grid_piece_filter_update(
    FVizUnstructuredGridPieceFilter* filter);
FVIZ_API FVizResult fviz_unstructured_grid_piece_filter_update_piece(
    FVizUnstructuredGridPieceFilter* filter,
    uint32_t piece,
    uint32_t number_of_pieces,
    uint32_t ghost_levels);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PIECE_FILTER_H */
