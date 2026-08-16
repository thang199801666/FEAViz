#ifndef FVIZ_FEA_DEFORMED_SHAPE_H
#define FVIZ_FEA_DEFORMED_SHAPE_H

#include <stdint.h>

#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Algorithms/FVizDeformation.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/FEA/FVizResultDatabase.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEADeformedShapeController FVizFEADeformedShapeController;
typedef struct FVizFEADeformedShapeResult FVizFEADeformedShapeResult;

#define FVIZ_TYPE_FEA_DEFORMED_SHAPE_CONTROLLER UINT64_C(0x563A4A5E9E54D413)
#define FVIZ_TYPE_FEA_DEFORMED_SHAPE_RESULT UINT64_C(0xA67A7E36F99C9421)

typedef enum FVizFEADeformationState
{
    FVIZ_FEA_DEFORMATION_UNDEFORMED = 0,
    FVIZ_FEA_DEFORMATION_DEFORMED = 1,
    /* Result carries both the original base grid and a deformed grid so the
     * renderer/view controller can draw both states simultaneously. */
    FVIZ_FEA_DEFORMATION_SUPERIMPOSED = 2
} FVizFEADeformationState;

typedef enum FVizFEADeformationScaleMode
{
    /* Physical displacement: scale factor = 1.0. */
    FVIZ_FEA_DEFORMATION_SCALE_TRUE = 0,
    /* User-supplied scalar factor. */
    FVIZ_FEA_DEFORMATION_SCALE_UNIFORM = 1,
    /* Generic Core auto-scale based on model diagonal/max displacement. */
    FVIZ_FEA_DEFORMATION_SCALE_AUTO = 2
} FVizFEADeformationScaleMode;

typedef struct FVizFEADeformedShapeOptions
{
    uint32_t struct_size;
    FVizFEADeformationState state;
    const char* displacement_field_name; /* Defaults to "U". */
    const char* instance_name;           /* NULL/empty accepts any single matching instance. */
    FVizFEADeformationScaleMode scale_mode;
    double uniform_scale;
    double auto_target_fraction; /* Default 0.10 of model diagonal. */
    double auto_minimum_scale;
    double auto_maximum_scale;
    /* When false, result nodes not supplied by the selected U block remain
     * undeformed and are marked 0 in coverage_mask. */
    FVizBool require_complete_nodal_coverage;
} FVizFEADeformedShapeOptions;

typedef struct FVizFEADeformedShapeCacheStatistics
{
    uint64_t hits;
    uint64_t misses;
    uint64_t clears;
    FVizBool populated;
} FVizFEADeformedShapeCacheStatistics;

FVIZ_FEA_API void fviz_fea_deformed_shape_options_initialize(
    FVizFEADeformedShapeOptions* options);
FVIZ_FEA_API FVizResult fviz_fea_deformed_shape_controller_create(
    FVizFEADeformedShapeController** out_controller);
FVIZ_FEA_API void fviz_fea_deformed_shape_controller_clear_cache(
    FVizFEADeformedShapeController* controller);
FVIZ_FEA_API FVizFEADeformedShapeCacheStatistics
fviz_fea_deformed_shape_controller_cache_statistics(
    const FVizFEADeformedShapeController* controller);

/* Evaluates an Abaqus-like deformed-shape state for one mesh instance and one
 * result frame. Only result selection/mapping lives here; actual point
 * deformation and automatic scaling are generic FEAViz::Core primitives. */
FVIZ_FEA_API FVizResult fviz_fea_deformed_shape_evaluate(
    FVizFEADeformedShapeController* controller,
    const FVizFEAFrame* frame,
    const FVizUnstructuredGrid* grid,
    const FVizFEADeformedShapeOptions* options,
    FVizFEADeformedShapeResult** out_result);

FVIZ_FEA_API FVizFEADeformationState fviz_fea_deformed_shape_result_state(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API double fviz_fea_deformed_shape_result_scale_factor(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API const FVizDeformationMetrics* fviz_fea_deformed_shape_result_metrics(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API FVizSize fviz_fea_deformed_shape_result_mapped_point_count(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API FVizSize fviz_fea_deformed_shape_result_missing_point_count(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API const FVizDataArray* fviz_fea_deformed_shape_result_displacements(
    const FVizFEADeformedShapeResult* result);
/* UInt8 one tuple per mesh point: 1 when a displacement result mapped to that
 * point, 0 when the point is intentionally left undeformed. */
FVIZ_FEA_API const FVizDataArray* fviz_fea_deformed_shape_result_coverage_mask(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API const FVizUnstructuredGrid* fviz_fea_deformed_shape_result_base_grid(
    const FVizFEADeformedShapeResult* result);
FVIZ_FEA_API const FVizUnstructuredGrid* fviz_fea_deformed_shape_result_grid(
    const FVizFEADeformedShapeResult* result);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_DEFORMED_SHAPE_H */
