#ifndef FVIZ_INTERACTION_SELECTION_H
#define FVIZ_INTERACTION_SELECTION_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

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

FVIZ_API FVizResult fviz_selection_create(FVizSelection** out_selection);
FVIZ_API void fviz_selection_clear(FVizSelection* selection);
FVIZ_API FVizResult fviz_selection_add(
    FVizSelection* selection,
    FVizActor* actor,
    FVizSelectionAssociation association,
    FVizSize id);
FVIZ_API FVizSize fviz_selection_count(const FVizSelection* selection);
FVIZ_API FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index);
FVIZ_API FVizSelectionAssociation fviz_selection_association(
    const FVizSelection* selection,
    FVizSize index);
FVIZ_API FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_SELECTION_H */
