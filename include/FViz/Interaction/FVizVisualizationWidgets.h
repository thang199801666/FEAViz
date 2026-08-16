#ifndef FVIZ_INTERACTION_VISUALIZATION_WIDGETS_H
#define FVIZ_INTERACTION_VISUALIZATION_WIDGETS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Rendering/FVizRenderWindow.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizSelectionHighlight FVizSelectionHighlight;
typedef struct FVizOrientationAxesWidget FVizOrientationAxesWidget;

#define FVIZ_TYPE_SELECTION_HIGHLIGHT UINT64_C(0xB9F746E213AC508D)
#define FVIZ_TYPE_ORIENTATION_AXES_WIDGET UINT64_C(0x73E18B49D2A6CF05)

FVIZ_INTERACTION_API FVizResult fviz_selection_highlight_create(FVizRenderer* renderer, FVizSelection* selection,
                                                    FVizSelectionHighlight** out_highlight);
FVIZ_INTERACTION_API FVizResult fviz_selection_highlight_update(FVizSelectionHighlight* highlight);
FVIZ_INTERACTION_API void fviz_selection_highlight_set_color(FVizSelectionHighlight* highlight, float red, float green, float blue);
FVIZ_INTERACTION_API void fviz_selection_highlight_set_enabled(FVizSelectionHighlight* highlight, FVizBool enabled);
FVIZ_INTERACTION_API FVizBool fviz_selection_highlight_enabled(const FVizSelectionHighlight* highlight);
FVIZ_INTERACTION_API FVizActor* fviz_selection_highlight_actor(FVizSelectionHighlight* highlight);
FVIZ_INTERACTION_API FVizActor* fviz_selection_highlight_point_actor(FVizSelectionHighlight* highlight);

FVIZ_INTERACTION_API FVizResult fviz_orientation_axes_widget_create(FVizRenderWindow* window, FVizRenderer* target_renderer,
                                                        FVizOrientationAxesWidget** out_widget);
FVIZ_INTERACTION_API FVizResult fviz_orientation_axes_widget_update(FVizOrientationAxesWidget* widget);
FVIZ_INTERACTION_API void fviz_orientation_axes_widget_set_enabled(FVizOrientationAxesWidget* widget, FVizBool enabled);
FVIZ_INTERACTION_API FVizBool fviz_orientation_axes_widget_enabled(const FVizOrientationAxesWidget* widget);
FVIZ_INTERACTION_API FVizRenderer* fviz_orientation_axes_widget_renderer(FVizOrientationAxesWidget* widget);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_VISUALIZATION_WIDGETS_H */
