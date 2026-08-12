#ifndef FVIZ_INTERACTION_SELECTION_H
#define FVIZ_INTERACTION_SELECTION_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizActor FVizActor;
typedef struct FVizSelection FVizSelection;

#define FVIZ_TYPE_SELECTION UINT64_C(0x618FC3A4D927E05B)

typedef enum FVizSelectionAssociation
{
    FVIZ_SELECTION_ACTOR = 0,
    FVIZ_SELECTION_POINT = 1,
    FVIZ_SELECTION_CELL = 2
} FVizSelectionAssociation;

typedef enum FVizSelectionState
{
    FVIZ_SELECTION_VALID = 0,
    FVIZ_SELECTION_INVALID = 1
} FVizSelectionState;

typedef struct FVizSelectionRecord
{
    uint32_t struct_size;
    FVizActor* actor;
    FVizSelectionAssociation association;
    FVizSize rendered_id;
    FVizId original_point_id;
    FVizId original_cell_id;
    FVizId original_face_id;
    FVizMTime output_mtime;
    FVizBool persistent;
    FVizSelectionState state;
    FVizBool has_world_position;
    FVizVec3 world_position;
    uint32_t scalar_component_count;
    double scalar_tuple[4];
} FVizSelectionRecord;

FVIZ_API FVizResult fviz_selection_create(FVizSelection** out_selection);
FVIZ_API void fviz_selection_record_initialize(FVizSelectionRecord* record);
FVIZ_API void fviz_selection_clear(FVizSelection* selection);
FVIZ_API FVizResult fviz_selection_add(
    FVizSelection* selection,
    FVizActor* actor,
    FVizSelectionAssociation association,
    FVizSize id);
FVIZ_API FVizResult fviz_selection_add_record(
    FVizSelection* selection,
    const FVizSelectionRecord* record);
FVIZ_API FVizResult fviz_selection_get_record(
    const FVizSelection* selection,
    FVizSize index,
    FVizSelectionRecord* out_record);
FVIZ_API FVizResult fviz_selection_refresh(FVizSelection* selection);
FVIZ_API FVizResult fviz_selection_probe(
    FVizSelection* selection,
    FVizSize index,
    const char* array_name);
FVIZ_API FVizSelectionState fviz_selection_state(
    const FVizSelection* selection,
    FVizSize index);
FVIZ_API FVizSize fviz_selection_count(const FVizSelection* selection);
FVIZ_API FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index);
FVIZ_API FVizSelectionAssociation fviz_selection_association(
    const FVizSelection* selection,
    FVizSize index);
FVIZ_API FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_SELECTION_H */
