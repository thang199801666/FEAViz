#ifndef FVIZ_INTERNAL_FEA_RESULT_FIELD_PRIVATE_H
#define FVIZ_INTERNAL_FEA_RESULT_FIELD_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/FEA/FVizResultField.h>

typedef struct FVizFEAFieldBlock
{
    FVizString* instance_name;
    FVizFEAResultPosition position;
    int32_t section_point_number;
    FVizString* section_point_label;
    FVizDataArray* entity_ids;
    FVizDataArray* local_ids;
    FVizDataArray* values;
    FVizObserverTag entity_ids_tag;
    FVizObserverTag local_ids_tag;
    FVizObserverTag values_tag;
} FVizFEAFieldBlock;

struct FVizFEAField
{
    FVizObject base;
    FVizString* name;
    FVizString* description;
    FVizFEAFieldType field_type;
    FVizArray* component_labels; /* FVizString* */
    FVizFEAInvariantMask valid_invariants;
    FVizArray* blocks; /* FVizFEAFieldBlock */
};

#endif /* FVIZ_INTERNAL_FEA_RESULT_FIELD_PRIVATE_H */
