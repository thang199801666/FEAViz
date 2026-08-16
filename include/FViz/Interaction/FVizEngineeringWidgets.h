#ifndef FVIZ_INTERACTION_ENGINEERING_WIDGETS_H
#define FVIZ_INTERACTION_ENGINEERING_WIDGETS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Interaction/FVizWidget.h>
#include <FViz/Interaction/FVizWidgetManipulator.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizTextActor.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizHandleWidget FVizHandleWidget;
typedef struct FVizPlaneWidget FVizPlaneWidget;
typedef struct FVizLineWidget FVizLineWidget;
typedef struct FVizBoxWidget FVizBoxWidget;
typedef struct FVizDistanceWidget FVizDistanceWidget;
typedef struct FVizAngleWidget FVizAngleWidget;
typedef struct FVizSectionCutWidget FVizSectionCutWidget;
typedef struct FVizProbeWidget FVizProbeWidget;

#define FVIZ_TYPE_HANDLE_WIDGET UINT64_C(0x51B8D2A793CE640F)
#define FVIZ_TYPE_PLANE_WIDGET UINT64_C(0x82AB01D4F69735CE)
#define FVIZ_TYPE_BOX_WIDGET UINT64_C(0x9C0436E17A52B8FD)
#define FVIZ_TYPE_LINE_WIDGET UINT64_C(0xC4A9D8326F175BE0)
#define FVIZ_TYPE_DISTANCE_WIDGET UINT64_C(0xD17420AE6C3958FB)
#define FVIZ_TYPE_ANGLE_WIDGET UINT64_C(0xA731D84E95C206BF)
#define FVIZ_TYPE_SECTION_CUT_WIDGET UINT64_C(0x7F0536B2E41A98CD)
#define FVIZ_TYPE_PROBE_WIDGET UINT64_C(0xBD20F1537A4689CE)

FVIZ_API FVizResult fviz_handle_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizHandleWidget** out_widget);
FVIZ_API FVizWidget* fviz_handle_widget_widget(FVizHandleWidget* widget);
FVIZ_API void fviz_handle_widget_set_position(FVizHandleWidget* widget, FVizVec3 position);
FVIZ_API FVizVec3 fviz_handle_widget_position(const FVizHandleWidget* widget);
FVIZ_API void fviz_handle_widget_set_size(FVizHandleWidget* widget, float pixels);
FVIZ_API float fviz_handle_widget_size(const FVizHandleWidget* widget);
FVIZ_API void fviz_handle_widget_set_pick_tolerance(FVizHandleWidget* widget, float pixels);
FVIZ_API float fviz_handle_widget_pick_tolerance(const FVizHandleWidget* widget);
FVIZ_API void fviz_handle_widget_set_color(
    FVizHandleWidget* widget, float red, float green, float blue);
FVIZ_API void fviz_handle_widget_set_manipulator_mode(
    FVizHandleWidget* widget, FVizWidgetManipulatorMode mode);
FVIZ_API FVizWidgetManipulatorMode fviz_handle_widget_manipulator_mode(
    const FVizHandleWidget* widget);
FVIZ_API FVizResult fviz_handle_widget_set_axis(FVizHandleWidget* widget, FVizVec3 axis);
FVIZ_API FVizResult fviz_handle_widget_set_plane_normal(FVizHandleWidget* widget, FVizVec3 normal);

FVIZ_API FVizResult fviz_plane_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizPlaneWidget** out_widget);
FVIZ_API FVizWidget* fviz_plane_widget_widget(FVizPlaneWidget* widget);
FVIZ_API void fviz_plane_widget_set_origin(FVizPlaneWidget* widget, FVizVec3 origin);
FVIZ_API FVizVec3 fviz_plane_widget_origin(const FVizPlaneWidget* widget);
FVIZ_API FVizResult fviz_plane_widget_set_normal(FVizPlaneWidget* widget, FVizVec3 normal);
FVIZ_API FVizVec3 fviz_plane_widget_normal(const FVizPlaneWidget* widget);
FVIZ_API FVizPlane fviz_plane_widget_plane(const FVizPlaneWidget* widget);
FVIZ_API void fviz_plane_widget_set_size(FVizPlaneWidget* widget, float size);
FVIZ_API float fviz_plane_widget_size(const FVizPlaneWidget* widget);
FVIZ_API void fviz_plane_widget_set_color(FVizPlaneWidget* widget, float red, float green, float blue);
FVIZ_API FVizResult fviz_plane_widget_update_representation(FVizPlaneWidget* widget);

FVIZ_API FVizResult fviz_box_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizBoxWidget** out_widget);
FVIZ_API FVizWidget* fviz_box_widget_widget(FVizBoxWidget* widget);
FVIZ_API FVizResult fviz_box_widget_set_bounds(FVizBoxWidget* widget, const FVizBounds* bounds);
FVIZ_API FVizBounds fviz_box_widget_bounds(const FVizBoxWidget* widget);
FVIZ_API void fviz_box_widget_set_color(FVizBoxWidget* widget, float red, float green, float blue);
FVIZ_API void fviz_box_widget_set_handle_size(FVizBoxWidget* widget, float pixels);
FVIZ_API void fviz_box_widget_set_pick_tolerance(FVizBoxWidget* widget, float pixels);
FVIZ_API FVizResult fviz_box_widget_update_representation(FVizBoxWidget* widget);

FVIZ_API FVizResult fviz_line_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizLineWidget** out_widget);
FVIZ_API FVizWidget* fviz_line_widget_widget(FVizLineWidget* widget);
FVIZ_API void fviz_line_widget_set_points(
    FVizLineWidget* widget, FVizVec3 point1, FVizVec3 point2);
FVIZ_API void fviz_line_widget_get_points(
    const FVizLineWidget* widget, FVizVec3* point1, FVizVec3* point2);
FVIZ_API float fviz_line_widget_length(const FVizLineWidget* widget);
FVIZ_API void fviz_line_widget_set_color(
    FVizLineWidget* widget, float red, float green, float blue);
FVIZ_API void fviz_line_widget_set_line_width(FVizLineWidget* widget, float pixels);
FVIZ_API void fviz_line_widget_set_handle_size(FVizLineWidget* widget, float pixels);
FVIZ_API void fviz_line_widget_set_pick_tolerance(FVizLineWidget* widget, float pixels);
FVIZ_API FVizResult fviz_line_widget_update_representation(FVizLineWidget* widget);

FVIZ_API FVizResult fviz_distance_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizDistanceWidget** out_widget);
FVIZ_API FVizWidget* fviz_distance_widget_widget(FVizDistanceWidget* widget);
FVIZ_API FVizResult fviz_distance_widget_set_points(FVizDistanceWidget* widget, FVizVec3 point1, FVizVec3 point2);
FVIZ_API void fviz_distance_widget_get_points(const FVizDistanceWidget* widget, FVizVec3* point1, FVizVec3* point2);
FVIZ_API float fviz_distance_widget_distance(const FVizDistanceWidget* widget);
FVIZ_API FVizBool fviz_distance_widget_completed(const FVizDistanceWidget* widget);
FVIZ_API void fviz_distance_widget_reset(FVizDistanceWidget* widget);
FVIZ_API FVizBillboardTextActor3D* fviz_distance_widget_label(FVizDistanceWidget* widget);

FVIZ_API FVizResult fviz_angle_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizAngleWidget** out_widget);
FVIZ_API FVizWidget* fviz_angle_widget_widget(FVizAngleWidget* widget);
FVIZ_API FVizResult fviz_angle_widget_set_points(
    FVizAngleWidget* widget, FVizVec3 point1, FVizVec3 vertex, FVizVec3 point2);
FVIZ_API float fviz_angle_widget_angle_degrees(const FVizAngleWidget* widget);
FVIZ_API FVizBool fviz_angle_widget_completed(const FVizAngleWidget* widget);
FVIZ_API void fviz_angle_widget_reset(FVizAngleWidget* widget);
FVIZ_API FVizBillboardTextActor3D* fviz_angle_widget_label(FVizAngleWidget* widget);

FVIZ_API FVizResult fviz_section_cut_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizSectionCutWidget** out_widget);
FVIZ_API FVizPlaneWidget* fviz_section_cut_widget_plane_widget(FVizSectionCutWidget* widget);
FVIZ_API FVizResult fviz_section_cut_widget_add_actor(FVizSectionCutWidget* widget, FVizActor* actor);
FVIZ_API FVizResult fviz_section_cut_widget_remove_actor(FVizSectionCutWidget* widget, FVizActor* actor);
FVIZ_API void fviz_section_cut_widget_remove_all_actors(FVizSectionCutWidget* widget);
FVIZ_API FVizSize fviz_section_cut_widget_actor_count(const FVizSectionCutWidget* widget);
FVIZ_API void fviz_section_cut_widget_set_inside_out(FVizSectionCutWidget* widget, FVizBool inside_out);
FVIZ_API FVizBool fviz_section_cut_widget_inside_out(const FVizSectionCutWidget* widget);
FVIZ_API FVizResult fviz_section_cut_widget_update(FVizSectionCutWidget* widget);

FVIZ_API FVizResult fviz_probe_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizProbeWidget** out_widget);
FVIZ_API FVizWidget* fviz_probe_widget_widget(FVizProbeWidget* widget);
FVIZ_API FVizResult fviz_probe_widget_set_array_name(FVizProbeWidget* widget, const char* array_name);
FVIZ_API const char* fviz_probe_widget_array_name(const FVizProbeWidget* widget);
FVIZ_API FVizResult fviz_probe_widget_probe_at(FVizProbeWidget* widget, int x, int y);
FVIZ_API FVizSelection* fviz_probe_widget_selection(FVizProbeWidget* widget);
FVIZ_API FVizBillboardTextActor3D* fviz_probe_widget_label(FVizProbeWidget* widget);
FVIZ_API void fviz_probe_widget_clear(FVizProbeWidget* widget);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_ENGINEERING_WIDGETS_H */
