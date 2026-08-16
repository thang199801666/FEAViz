#ifndef FVIZ_FEA_DISPLAY_GROUP_H
#define FVIZ_FEA_DISPLAY_GROUP_H

#include <stdint.h>

#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEADisplayGroup FVizFEADisplayGroup;
#define FVIZ_TYPE_FEA_DISPLAY_GROUP UINT64_C(0x5D8C1E2A93F470B6)

/* A display group is a named, boolean-combined set of FEA entities (nodes,
 * elements, faces) that controls which geometry is visible. It answers
 * point/cell membership against an unstructured grid so a renderer or filter
 * can hide everything outside the group. */
typedef enum FVizFEADisplayGroupEntity
{
    FVIZ_FEA_DISPLAY_GROUP_NODES = 0,
    FVIZ_FEA_DISPLAY_GROUP_ELEMENTS = 1,
    FVIZ_FEA_DISPLAY_GROUP_FACES = 2
} FVizFEADisplayGroupEntity;

typedef enum FVizFEADisplayGroupOperation
{
    FVIZ_FEA_DISPLAY_GROUP_REPLACE = 0,
    FVIZ_FEA_DISPLAY_GROUP_ADD = 1,
    FVIZ_FEA_DISPLAY_GROUP_REMOVE = 2,
    FVIZ_FEA_DISPLAY_GROUP_INTERSECT = 3
} FVizFEADisplayGroupOperation;

typedef struct FVizFEADisplayGroupStatistics
{
    FVizSize node_count;
    FVizSize element_count;
    FVizSize face_count;
    FVizSize visible_points;
    FVizSize visible_cells;
} FVizFEADisplayGroupStatistics;

FVIZ_FEA_API FVizResult fviz_fea_display_group_create(const char* name, FVizFEADisplayGroup** out_group);
FVIZ_FEA_API const char* fviz_fea_display_group_name(const FVizFEADisplayGroup* group);
FVIZ_FEA_API void fviz_fea_display_group_clear(FVizFEADisplayGroup* group);
FVIZ_FEA_API void fviz_fea_display_group_set_visible(FVizFEADisplayGroup* group, FVizBool visible);
FVIZ_FEA_API FVizBool fviz_fea_display_group_visible(const FVizFEADisplayGroup* group);

/* Adds entity labels (node/element/face native labels). Empty id arrays clear
 * the corresponding set. */
FVIZ_FEA_API FVizResult fviz_fea_display_group_set_nodes(FVizFEADisplayGroup* group, const uint64_t* node_labels,
                                                         FVizSize count);
FVIZ_FEA_API FVizResult fviz_fea_display_group_set_elements(FVizFEADisplayGroup* group, const uint64_t* element_labels,
                                                            FVizSize count);
FVIZ_FEA_API FVizResult fviz_fea_display_group_set_faces(FVizFEADisplayGroup* group, const uint64_t* face_labels,
                                                         FVizSize count);

/* Applies a boolean operation from a source group into this group. */
FVIZ_FEA_API FVizResult fviz_fea_display_group_combine(FVizFEADisplayGroup* group, const FVizFEADisplayGroup* source,
                                                       FVizFEADisplayGroupOperation operation);

/* Resolves visibility against a grid. Point and cell masks are returned as
 * UInt8 arrays (1 = in the group). */
FVIZ_FEA_API FVizResult fviz_fea_display_group_create_masks(const FVizFEADisplayGroup* group,
                                                            const FVizUnstructuredGrid* grid,
                                                            FVizDataArray** out_point_mask,
                                                            FVizDataArray** out_cell_mask);

/* Copies visible points/cells of a PolyData surface based on cell ids present
 * in the group's face/element set. */
FVIZ_FEA_API FVizResult fviz_fea_display_group_apply_to_surface(const FVizFEADisplayGroup* group,
                                                                const FVizPolyData* surface,
                                                                FVizPolyData** out_surface);

FVIZ_FEA_API void fviz_fea_display_group_get_statistics(const FVizFEADisplayGroup* group,
                                                        const FVizUnstructuredGrid* grid,
                                                        FVizFEADisplayGroupStatistics* out_statistics);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_DISPLAY_GROUP_H */
