#ifndef FVIZ_FEA_PRIMARY_VARIABLE_H
#define FVIZ_FEA_PRIMARY_VARIABLE_H

#include <stdint.h>
#include <limits.h>

#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/FEA/FVizIntegrationPointData.h>
#include <FViz/FEA/FVizResultField.h>
#include <FViz/Data/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEAPrimaryVariableEvaluator FVizFEAPrimaryVariableEvaluator;
typedef struct FVizFEAPrimaryVariableResult FVizFEAPrimaryVariableResult;

#define FVIZ_TYPE_FEA_PRIMARY_VARIABLE_EVALUATOR UINT64_C(0x27AC9741573D04A1)
#define FVIZ_TYPE_FEA_PRIMARY_VARIABLE_RESULT UINT64_C(0x0BF9F25A8C6E72D3)
#define FVIZ_FEA_SECTION_POINT_ANY INT32_MIN
#define FVIZ_FEA_COMPONENT_BY_LABEL ((FVizSize) - 1)

typedef enum FVizFEAPrimaryVariableOperation
{
    FVIZ_FEA_PRIMARY_COMPONENT = 0,
    FVIZ_FEA_PRIMARY_INVARIANT = 1
} FVizFEAPrimaryVariableOperation;

typedef enum FVizFEALocalIdBase
{
    FVIZ_FEA_LOCAL_ID_AUTO = 0,
    FVIZ_FEA_LOCAL_ID_ZERO_BASED = 1,
    FVIZ_FEA_LOCAL_ID_ONE_BASED = 2
} FVizFEALocalIdBase;

typedef enum FVizFEADisplayAssociation
{
    FVIZ_FEA_DISPLAY_ASSOCIATION_NONE = 0,
    FVIZ_FEA_DISPLAY_ASSOCIATION_POINT = 1,
    FVIZ_FEA_DISPLAY_ASSOCIATION_CELL = 2,
    FVIZ_FEA_DISPLAY_ASSOCIATION_ELEMENT_NODE = 3,
    FVIZ_FEA_DISPLAY_ASSOCIATION_RAW = 4
} FVizFEADisplayAssociation;

/* Solver-neutral result-selection descriptor.  An Abaqus/ODB bridge can map
 * its Primary Variable dialog state into this structure without leaking any
 * Abaqus runtime dependency into FEAViz Core. */
typedef struct FVizFEAPrimaryVariable
{
    uint32_t struct_size;
    const char* instance_name;             /* NULL/empty accepts every instance block. */
    FVizFEAResultPosition source_position; /* UNKNOWN selects a compatible source automatically. */
    FVizFEAResultPosition target_position;
    int32_t section_point_number; /* FVIZ_FEA_SECTION_POINT_ANY accepts all section points. */

    FVizFEAPrimaryVariableOperation operation;
    FVizSize component; /* ignored for invariants; BY_LABEL resolves component_label. */
    const char* component_label;
    FVizFEAInvariant invariant;

    FVizBool averaging_enabled;
    FVizBool average_across_blocks;
    /* Relative spread threshold in percent.  A contributing node is marked
     * discontinuous when 100*(max-min)/max(|min|,|max|,eps) exceeds this
     * value.  Negative disables threshold rejection. */
    double averaging_threshold_percent;
    FVizFEALocalIdBase local_id_base;
    FVizIntegrationPointFallbackPolicy integration_point_fallback;

    /* Optional integer one-component label whitelist.  This is intentionally
     * generic; named sets/surfaces introduced later can supply their labels. */
    const FVizDataArray* entity_filter_ids;
} FVizFEAPrimaryVariable;

typedef struct FVizFEAPrimaryVariableCacheStatistics
{
    uint64_t hits;
    uint64_t misses;
    uint64_t clears;
    FVizSize entries;
} FVizFEAPrimaryVariableCacheStatistics;

FVIZ_FEA_API void fviz_fea_primary_variable_initialize(FVizFEAPrimaryVariable* variable);

FVIZ_FEA_API FVizResult fviz_fea_primary_variable_evaluator_create(FVizFEAPrimaryVariableEvaluator** out_evaluator);
FVIZ_FEA_API void fviz_fea_primary_variable_evaluator_clear_cache(FVizFEAPrimaryVariableEvaluator* evaluator);
FVIZ_FEA_API FVizFEAPrimaryVariableCacheStatistics
fviz_fea_primary_variable_evaluator_cache_statistics(const FVizFEAPrimaryVariableEvaluator* evaluator);

/* Evaluates one scalar primary variable against an UnstructuredGrid instance.
 * The result keeps both source-order raw scalar values and the display-ready
 * representation.  Nodal averaging never mutates the source field. */
FVIZ_FEA_API FVizResult fviz_fea_primary_variable_evaluate(FVizFEAPrimaryVariableEvaluator* evaluator,
                                                           const FVizFEAField* field, const FVizUnstructuredGrid* grid,
                                                           const FVizFEAPrimaryVariable* variable,
                                                           FVizFEAPrimaryVariableResult** out_result);

FVIZ_FEA_API FVizFEAResultPosition
fviz_fea_primary_variable_result_source_position(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API FVizFEAResultPosition
fviz_fea_primary_variable_result_target_position(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API FVizFEADisplayAssociation
fviz_fea_primary_variable_result_association(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_raw_values(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_raw_entity_ids(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_raw_local_ids(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_display_values(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_display_entity_ids(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_display_local_ids(const FVizFEAPrimaryVariableResult* result);
/* UInt8, one tuple per display tuple.  Non-zero means that averaging was
 * deliberately rejected because of a block boundary, disabled averaging, or
 * the configured relative-discontinuity threshold. */
FVIZ_FEA_API const FVizDataArray*
fviz_fea_primary_variable_result_discontinuity_mask(const FVizFEAPrimaryVariableResult* result);
FVIZ_FEA_API FVizBool fviz_fea_primary_variable_result_raw_range(const FVizFEAPrimaryVariableResult* result,
                                                                 double* out_minimum, double* out_maximum);
FVIZ_FEA_API FVizBool fviz_fea_primary_variable_result_display_range(const FVizFEAPrimaryVariableResult* result,
                                                                     double* out_minimum, double* out_maximum);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_PRIMARY_VARIABLE_H */
