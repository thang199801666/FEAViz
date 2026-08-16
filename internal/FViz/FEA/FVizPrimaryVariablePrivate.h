#ifndef FVIZ_INTERNAL_FEA_PRIMARY_VARIABLE_PRIVATE_H
#define FVIZ_INTERNAL_FEA_PRIMARY_VARIABLE_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/FEA/FVizPrimaryVariable.h>

struct FVizFEAPrimaryVariableResult
{
    FVizObject base;
    FVizFEAResultPosition source_position;
    FVizFEAResultPosition target_position;
    FVizFEADisplayAssociation association;
    FVizDataArray* raw_values;
    FVizDataArray* raw_entity_ids;
    FVizDataArray* raw_local_ids;
    FVizDataArray* display_values;
    FVizDataArray* display_entity_ids;
    FVizDataArray* display_local_ids;
    FVizDataArray* discontinuity_mask;
    FVizBool raw_range_valid;
    FVizBool display_range_valid;
    double raw_minimum;
    double raw_maximum;
    double display_minimum;
    double display_maximum;
};

struct FVizFEAPrimaryVariableEvaluator
{
    FVizObject base;
    const FVizFEAField* cached_field;
    const FVizUnstructuredGrid* cached_grid;
    FVizMTime cached_field_mtime;
    FVizMTime cached_grid_mtime;
    FVizMTime cached_filter_mtime;
    FVizString* cached_instance_name;
    FVizString* cached_component_label;
    FVizFEAPrimaryVariable cached_variable;
    FVizFEAPrimaryVariableResult* cached_result;
    uint64_t hits;
    uint64_t misses;
    uint64_t clears;
};

#endif /* FVIZ_INTERNAL_FEA_PRIMARY_VARIABLE_PRIVATE_H */
