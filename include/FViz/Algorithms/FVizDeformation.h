#ifndef FVIZ_ALGORITHMS_DEFORMATION_H
#define FVIZ_ALGORITHMS_DEFORMATION_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDeformationMetrics
{
    uint32_t struct_size;
    FVizSize tuple_count;
    FVizSize finite_tuple_count;
    double maximum_magnitude;
    double rms_magnitude;
} FVizDeformationMetrics;

FVIZ_API void fviz_deformation_metrics_initialize(FVizDeformationMetrics* metrics);

/* Measures a numeric three-component vector array. Non-finite tuples are
 * excluded from finite_tuple_count and from maximum/RMS accumulation. */
FVIZ_API FVizResult fviz_deformation_measure_vectors(
    const FVizDataArray* vectors,
    FVizDeformationMetrics* out_metrics);

/* Computes an Abaqus/CAE-friendly generic visual scale primitive without any
 * solver semantics: the largest displayed displacement becomes approximately
 * target_fraction * model diagonal. target_fraction must be > 0. A zero vector
 * field yields scale 1.0 (subject to clamping). */
FVIZ_API FVizResult fviz_deformation_compute_auto_scale(
    FVizBounds model_bounds,
    const FVizDataArray* vectors,
    double target_fraction,
    double minimum_scale,
    double maximum_scale,
    double* out_scale,
    FVizDeformationMetrics* out_metrics);

/* Tuple-aligned vector deformation primitives. The input vector array must be
 * numeric, have 3 components and match the point count exactly. */
FVIZ_API FVizResult fviz_deformation_apply_to_points(
    const FVizPoints* points,
    const FVizDataArray* vectors,
    double scale,
    FVizPoints** out_points);
FVIZ_API FVizResult fviz_deformation_apply_to_poly_data(
    const FVizPolyData* poly_data,
    const FVizDataArray* vectors,
    double scale,
    FVizPolyData** out_poly_data);
FVIZ_API FVizResult fviz_deformation_apply_to_unstructured_grid(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* vectors,
    double scale,
    FVizUnstructuredGrid** out_grid);

/* Allocation-free topology reuse path for animation. Destination point counts
 * must already match the corresponding base object. These functions mutate
 * only coordinates and emit the normal geometry ModifiedEvent. */
FVIZ_API FVizResult fviz_deformation_update_points(
    FVizPoints* destination,
    const FVizPoints* base_points,
    const FVizDataArray* vectors,
    double scale);
FVIZ_API FVizResult fviz_deformation_update_poly_data_points(
    FVizPolyData* destination,
    const FVizPolyData* base_poly_data,
    const FVizDataArray* vectors,
    double scale);
FVIZ_API FVizResult fviz_deformation_update_unstructured_grid_points(
    FVizUnstructuredGrid* destination,
    const FVizUnstructuredGrid* base_grid,
    const FVizDataArray* vectors,
    double scale);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_DEFORMATION_H */
