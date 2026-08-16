#ifndef FVIZ_INTERACTION_SELECTION_MODEL_H
#define FVIZ_INTERACTION_SELECTION_MODEL_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Interaction/FVizSelection.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizSelectionModel FVizSelectionModel;
typedef struct FVizRenderWindow FVizRenderWindow;

#define FVIZ_TYPE_SELECTION_MODEL UINT64_C(0x5F8E23A19BC746D2)

FVIZ_INTERACTION_API FVizResult fviz_selection_model_create(FVizSelectionModel** out_model);
FVIZ_INTERACTION_API FVizSelection* fviz_selection_model_selection(FVizSelectionModel* model);
FVIZ_INTERACTION_API const FVizSelection* fviz_selection_model_const_selection(const FVizSelectionModel* model);
FVIZ_INTERACTION_API FVizSelection* fviz_selection_model_hover_selection(FVizSelectionModel* model);
FVIZ_INTERACTION_API const FVizSelection* fviz_selection_model_const_hover_selection(const FVizSelectionModel* model);
FVIZ_INTERACTION_API void fviz_selection_model_clear(FVizSelectionModel* model);
FVIZ_INTERACTION_API void fviz_selection_model_clear_hover(FVizSelectionModel* model);
FVIZ_INTERACTION_API void fviz_selection_model_set_association(FVizSelectionModel* model, FVizSelectionAssociation association);
FVIZ_INTERACTION_API FVizSelectionAssociation fviz_selection_model_association(const FVizSelectionModel* model);
FVIZ_INTERACTION_API void fviz_selection_model_set_modifier(FVizSelectionModel* model, FVizSelectionModifier modifier);
FVIZ_INTERACTION_API FVizSelectionModifier fviz_selection_model_modifier(const FVizSelectionModel* model);
FVIZ_INTERACTION_API FVizSelectionModifier fviz_selection_modifier_from_event(const FVizInteractionEvent* event);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_apply(FVizSelectionModel* model, const FVizSelection* incoming,
                                               FVizSelectionModifier modifier);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_select_at(FVizSelectionModel* model, FVizRenderWindow* window, int x, int y,
                                                   FVizSelectionModifier modifier);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_select_rectangle(FVizSelectionModel* model, FVizRenderWindow* window,
                                                          int start_x, int start_y, int end_x, int end_y,
                                                          FVizSelectionModifier modifier);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_select_polygon(FVizSelectionModel* model, FVizRenderWindow* window,
                                                        const int* xy_points, FVizSize point_count,
                                                        FVizSelectionModifier modifier);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_select_frustum(FVizSelectionModel* model, FVizRenderer* renderer,
                                                        const FVizFrustum* frustum, FVizSelectionModifier modifier);
FVIZ_INTERACTION_API void fviz_selection_model_set_hover_update_rate(FVizSelectionModel* model, float updates_per_second);
FVIZ_INTERACTION_API float fviz_selection_model_hover_update_rate(const FVizSelectionModel* model);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_process_hover_event(FVizSelectionModel* model, FVizRenderWindow* window,
                                                             const FVizInteractionEvent* event);
FVIZ_INTERACTION_API FVizResult fviz_selection_model_update_hover(FVizSelectionModel* model, FVizRenderWindow* window, int x,
                                                      int y);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_SELECTION_MODEL_H */
