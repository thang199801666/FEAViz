#ifndef FVIZ_INTERACTION_SELECTION_H
#define FVIZ_INTERACTION_SELECTION_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Math/FVizFrustum.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizActor FVizActor;
typedef struct FVizRenderer FVizRenderer;
typedef struct FVizSelection FVizSelection;
typedef struct FVizNamedSelectionCollection FVizNamedSelectionCollection;

#define FVIZ_TYPE_SELECTION UINT64_C(0x618FC3A4D927E05B)
#define FVIZ_TYPE_NAMED_SELECTION_COLLECTION UINT64_C(0x13EAC920B54D876F)

typedef enum FVizSelectionAssociation
{
    FVIZ_SELECTION_ACTOR = 0,
    FVIZ_SELECTION_POINT = 1,
    FVIZ_SELECTION_CELL = 2,
    FVIZ_SELECTION_EDGE = 3,
    FVIZ_SELECTION_GLYPH_INSTANCE = 4
} FVizSelectionAssociation;

typedef enum FVizSelectionModifier
{
    FVIZ_SELECTION_REPLACE = 0,
    FVIZ_SELECTION_ADD = 1,
    FVIZ_SELECTION_SUBTRACT = 2,
    FVIZ_SELECTION_TOGGLE = 3
} FVizSelectionModifier;

typedef enum FVizSelectionState
{
    FVIZ_SELECTION_VALID = 0,
    FVIZ_SELECTION_INVALID = 1
} FVizSelectionState;

typedef enum FVizSelectionVisibilityPolicy
{
    FVIZ_SELECTION_THROUGH = 0,
    FVIZ_SELECTION_VISIBLE_ONLY = 1
} FVizSelectionVisibilityPolicy;

typedef FVizBool (*FVizSelectionCancelCallback)(void* user_data);

typedef struct FVizSelectionRegionOptions
{
    uint32_t struct_size;
    FVizSelectionVisibilityPolicy visibility_policy;
    /* Zero means unlimited. Reaching the limit returns a valid truncated
     * selection and reports overflow in the statistics record. */
    FVizSize maximum_results;
    uint32_t cancellation_check_interval;
    FVizSelectionCancelCallback cancel;
    void* cancel_user_data;
} FVizSelectionRegionOptions;

typedef struct FVizSelectionRegionStatistics
{
    uint32_t struct_size;
    uint64_t candidates_tested;
    FVizSize results_returned;
    FVizBool overflow;
    FVizBool cancelled;
} FVizSelectionRegionStatistics;

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
FVIZ_API FVizResult fviz_selection_add(FVizSelection* selection, FVizActor* actor, FVizSelectionAssociation association,
                                       FVizSize id);
FVIZ_API FVizResult fviz_selection_add_record(FVizSelection* selection, const FVizSelectionRecord* record);
FVIZ_API FVizResult fviz_selection_get_record(const FVizSelection* selection, FVizSize index,
                                              FVizSelectionRecord* out_record);
FVIZ_API FVizResult fviz_selection_refresh(FVizSelection* selection);
FVIZ_API FVizBool fviz_selection_contains(const FVizSelection* selection, const FVizActor* actor,
                                          FVizSelectionAssociation association, FVizSize id);
FVIZ_API FVizResult fviz_selection_remove(FVizSelection* selection, const FVizActor* actor,
                                          FVizSelectionAssociation association, FVizSize id);
FVIZ_API FVizResult fviz_selection_copy(const FVizSelection* source, FVizSelection** out_selection);
FVIZ_API FVizResult fviz_selection_apply(FVizSelection* selection, const FVizSelection* incoming,
                                         FVizSelectionModifier modifier);

/* CPU region selection utilities. Coordinates use the render-window display convention. */
FVIZ_API FVizResult fviz_selection_select_frustum(FVizRenderer* renderer, const FVizFrustum* frustum,
                                                  FVizSelectionAssociation association, FVizSelection** out_selection);
FVIZ_API FVizResult fviz_selection_select_polygon(FVizRenderer* renderer, int display_width, int display_height,
                                                  const int* xy_points, FVizSize point_count,
                                                  FVizSelectionAssociation association, FVizSelection** out_selection);
FVIZ_API void fviz_selection_region_options_initialize(FVizSelectionRegionOptions* options);
FVIZ_API void fviz_selection_region_statistics_initialize(FVizSelectionRegionStatistics* statistics);
FVIZ_API FVizResult fviz_selection_select_polygon_with_options(
    FVizRenderer* renderer, int display_width, int display_height, const int* xy_points, FVizSize point_count,
    FVizSelectionAssociation association, const FVizSelectionRegionOptions* options,
    FVizSelectionRegionStatistics* statistics, FVizSelection** out_selection);
FVIZ_API FVizResult fviz_selection_select_rectangle(FVizRenderer* renderer, int display_width, int display_height,
                                                    int start_x, int start_y, int end_x, int end_y,
                                                    FVizSelectionAssociation association,
                                                    FVizSelection** out_selection);
FVIZ_API FVizResult fviz_selection_probe(FVizSelection* selection, FVizSize index, const char* array_name);
FVIZ_API FVizSelectionState fviz_selection_state(const FVizSelection* selection, FVizSize index);
FVIZ_API FVizSize fviz_selection_count(const FVizSelection* selection);
FVIZ_API FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index);
FVIZ_API FVizSelectionAssociation fviz_selection_association(const FVizSelection* selection, FVizSize index);
FVIZ_API FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index);
/* Builds a UInt8/1-component mask for records matching actor and association.
 * When inverse is true, selected entries are zero and all others are one. */
FVIZ_API FVizResult fviz_selection_create_mask(const FVizSelection* selection, const FVizActor* actor,
                                               FVizSelectionAssociation association, FVizSize value_count,
                                               FVizBool inverse, FVizDataArray** out_mask);
/* Converts point selections to incident render triangles, or triangle selections
 * to their points. Other association pairs return UNSUPPORTED. */
FVIZ_API FVizResult fviz_selection_convert_association(const FVizSelection* selection, FVizActor* actor,
                                                       FVizSelectionAssociation target_association,
                                                       FVizSelection** out_selection);
/* Extracts selected points as vertex cells, or selected render triangles as a
 * compact PolyData while gathering compatible point/cell/field attributes. */
FVIZ_API FVizResult fviz_selection_extract_poly_data(const FVizSelection* selection, const FVizActor* actor,
                                                     FVizSelectionAssociation association,
                                                     FVizPolyData** out_poly_data);

FVIZ_API FVizResult fviz_named_selection_collection_create(FVizNamedSelectionCollection** out_collection);
/* Adds or atomically replaces a named retained selection. */
FVIZ_API FVizResult fviz_named_selection_collection_set(FVizNamedSelectionCollection* collection, const char* name,
                                                        FVizSelection* selection);
FVIZ_API FVizResult fviz_named_selection_collection_remove(FVizNamedSelectionCollection* collection, const char* name);
FVIZ_API void fviz_named_selection_collection_clear(FVizNamedSelectionCollection* collection);
FVIZ_API FVizSize fviz_named_selection_collection_count(const FVizNamedSelectionCollection* collection);
FVIZ_API const char* fviz_named_selection_collection_name(const FVizNamedSelectionCollection* collection,
                                                          FVizSize index);
/* Borrowed pointer, valid while the collection entry remains present. */
FVIZ_API FVizSelection* fviz_named_selection_collection_get(FVizNamedSelectionCollection* collection, const char* name);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_SELECTION_H */
