#ifndef FVIZ_FEA_INTEGRATION_POINT_DATA_H
#define FVIZ_FEA_INTEGRATION_POINT_DATA_H

#include <stdint.h>

#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizIntegrationPointFallbackPolicy
{
    FVIZ_INTEGRATION_POINT_FAIL = 0,
    FVIZ_INTEGRATION_POINT_CELL_MEAN = 1
} FVizIntegrationPointFallbackPolicy;

typedef struct FVizIntegrationPointExtrapolationOptions
{
    uint32_t struct_size;
    FVizIntegrationPointFallbackPolicy fallback_policy;
} FVizIntegrationPointExtrapolationOptions;

FVIZ_FEA_API void fviz_integration_point_extrapolation_options_initialize(
    FVizIntegrationPointExtrapolationOptions* options);

/* Returns standard FE/VTK-reference parametric integration coordinates for a
 * supported cell and integration-point count.  out_points may be NULL to
 * query the required count. */
FVIZ_FEA_API FVizResult fviz_integration_point_standard_coordinates(
    FVizCellType cell_type,
    FVizSize integration_point_count,
    FVizVec3* out_points,
    FVizSize capacity,
    FVizSize* out_point_count);

/* Extrapolates concatenated cell integration-point tuples to global nodes.
 * cell_offsets has cell_count+1 entries and indexes integration_values.
 * parametric_coordinates, when non-NULL, contains one reference coordinate
 * per integration tuple.  When NULL, standard coordinates are selected from
 * the cell type and per-cell integration-point count.  Shared-node values are
 * averaged across contributing cells.  The output is Float64 and preserves
 * the input component count. */
FVIZ_FEA_API FVizResult fviz_unstructured_grid_extrapolate_integration_point_data(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* integration_values,
    const FVizSize* cell_offsets,
    const FVizVec3* parametric_coordinates,
    const FVizIntegrationPointExtrapolationOptions* options,
    FVizDataArray** out_point_values);

/* Extrapolates integration-point tuples independently inside each cell and
 * returns concatenated element-nodal tuples in cell/local-node order.  Cells
 * with zero input integration tuples produce NaN tuples, allowing sparse
 * field blocks to preserve unaveraged element-node values before a separate
 * nodal averaging policy is applied. */
FVIZ_FEA_API FVizResult fviz_unstructured_grid_extrapolate_integration_point_data_element_nodal(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* integration_values,
    const FVizSize* cell_offsets,
    const FVizVec3* parametric_coordinates,
    const FVizIntegrationPointExtrapolationOptions* options,
    FVizDataArray** out_element_nodal_values);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_INTEGRATION_POINT_DATA_H */
