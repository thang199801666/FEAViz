#ifndef FVIZ_RENDERING_MAPPER_H
#define FVIZ_RENDERING_MAPPER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizFilter.h>
#include <FViz/Rendering/FVizLookupTable.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizMapper FVizMapper;
typedef FVizId FVizClipPlaneId;
#define FVIZ_CLIP_PLANE_ID_INVALID UINT64_C(0)
#define FVIZ_TYPE_MAPPER UINT64_C(0x8A2C6F51D7B40E93)

typedef enum FVizDataAssociation
{
    FVIZ_ASSOCIATION_POINTS = 0,
    FVIZ_ASSOCIATION_CELLS = 1,
    FVIZ_ASSOCIATION_FIELD = 2
} FVizDataAssociation;

typedef enum FVizComponentMode
{
    FVIZ_COMPONENT_DIRECT = 0,
    FVIZ_COMPONENT_MAGNITUDE = 1,
    FVIZ_COMPONENT_COLOR = 2
} FVizComponentMode;

typedef enum FVizScalarInterpolation
{
    FVIZ_SCALAR_INTERPOLATION_DEFAULT = 0,
    FVIZ_SCALAR_INTERPOLATION_FLAT = 1,
    FVIZ_SCALAR_INTERPOLATION_POINT = 2
} FVizScalarInterpolation;

typedef struct FVizArraySelection
{
    uint32_t struct_size;
    FVizDataAssociation association;
    const char* name;
    FVizComponentMode component_mode;
    uint32_t component;
} FVizArraySelection;

FVIZ_API void fviz_array_selection_initialize(FVizArraySelection* selection);

FVIZ_API FVizResult fviz_mapper_create(FVizMapper** out_mapper);
FVIZ_API FVizResult fviz_mapper_set_poly_data(FVizMapper* mapper, FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_mapper_set_algorithm_connection(FVizMapper* mapper, FVizAlgorithmOutput* output);
FVIZ_API FVizAlgorithmOutput* fviz_mapper_algorithm_connection(FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_set_input_connection(FVizMapper* mapper, FVizFilter* producer);
FVIZ_API FVizFilter* fviz_mapper_input_connection(FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_update(FVizMapper* mapper);
FVIZ_API FVizPolyData* fviz_mapper_poly_data(FVizMapper* mapper);
FVIZ_API const FVizPolyData* fviz_mapper_const_poly_data(const FVizMapper* mapper);
FVIZ_API void fviz_mapper_set_lookup_table(FVizMapper* mapper, FVizLookupTable* table);
FVIZ_API FVizLookupTable* fviz_mapper_lookup_table(FVizMapper* mapper);
FVIZ_API void fviz_mapper_set_scalar_visibility(FVizMapper* mapper, FVizBool visible);
FVIZ_API FVizBool fviz_mapper_scalar_visibility(const FVizMapper* mapper);
FVIZ_API void fviz_mapper_set_scalar_range(FVizMapper* mapper, float minimum, float maximum);
FVIZ_API void fviz_mapper_get_scalar_range(const FVizMapper* mapper, float* minimum, float* maximum);
FVIZ_API FVizBool fviz_mapper_scalar_range_valid(const FVizMapper* mapper);
FVIZ_API void fviz_mapper_use_automatic_scalar_range(FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_set_array_selection(FVizMapper* mapper, const FVizArraySelection* selection);
FVIZ_API FVizResult fviz_mapper_get_array_selection(const FVizMapper* mapper, FVizArraySelection* out_selection);
FVIZ_API const FVizDataArray* fviz_mapper_selected_array(const FVizMapper* mapper);
FVIZ_API void fviz_mapper_set_scalar_interpolation(FVizMapper* mapper, FVizScalarInterpolation interpolation);
FVIZ_API FVizScalarInterpolation fviz_mapper_scalar_interpolation(const FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_set_opacity_array(FVizMapper* mapper, const char* name);
FVIZ_API const char* fviz_mapper_opacity_array(const FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_add_clipping_plane(FVizMapper* mapper, FVizPlane plane);
FVIZ_API FVizResult fviz_mapper_add_clipping_plane_with_id(FVizMapper* mapper, FVizPlane plane,
                                                           FVizClipPlaneId* out_id);
FVIZ_API FVizResult fviz_mapper_update_clipping_plane(FVizMapper* mapper, FVizClipPlaneId id, FVizPlane plane);
FVIZ_API FVizResult fviz_mapper_remove_clipping_plane(FVizMapper* mapper, FVizClipPlaneId id);
FVIZ_API FVizClipPlaneId fviz_mapper_clipping_plane_id(const FVizMapper* mapper, FVizSize index);
FVIZ_API void fviz_mapper_remove_all_clipping_planes(FVizMapper* mapper);
FVIZ_API FVizSize fviz_mapper_clipping_plane_count(const FVizMapper* mapper);
FVIZ_API FVizResult fviz_mapper_clipping_plane(const FVizMapper* mapper, FVizSize index, FVizPlane* out_plane);
/* Pinned mapper resources survive retention expiry and budget eviction until
 * explicitly unpinned or manually purged from the render window. */
FVIZ_API void fviz_mapper_set_gpu_residency_pinned(FVizMapper* mapper, FVizBool pinned);
FVIZ_API FVizBool fviz_mapper_gpu_residency_pinned(const FVizMapper* mapper);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_MAPPER_H */
