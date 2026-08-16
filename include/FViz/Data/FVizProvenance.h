#ifndef FVIZ_DATA_PROVENANCE_H
#define FVIZ_DATA_PROVENANCE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>

FVIZ_EXTERN_C_BEGIN

#define FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME "FVizOriginalPointIds"
#define FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME "FVizOriginalCellIds"
#define FVIZ_ORIGINAL_FACE_IDS_ARRAY_NAME "FVizOriginalFaceIds"

typedef enum FVizProvenanceEntity
{
    FVIZ_PROVENANCE_POINT = 0,
    FVIZ_PROVENANCE_CELL = 1,
    FVIZ_PROVENANCE_FACE = 2
} FVizProvenanceEntity;

FVIZ_API const char* fviz_provenance_array_name(FVizProvenanceEntity entity);
FVIZ_API FVizResult fviz_provenance_create_identity(
    FVizSize tuple_count,
    FVizDataArray** out_ids);
FVIZ_API FVizResult fviz_provenance_validate(
    const FVizDataArray* ids,
    FVizSize expected_tuple_count);
/* Resolves a local tuple through a provenance array. When the array is absent,
 * fallback is returned and out_persistent is false. */
FVIZ_API FVizResult fviz_provenance_resolve(
    const FVizAttributeSet* attributes,
    FVizProvenanceEntity entity,
    FVizSize local_id,
    FVizId fallback,
    FVizId* out_source_id,
    FVizBool* out_persistent);
FVIZ_API FVizResult fviz_provenance_find(
    const FVizAttributeSet* attributes,
    FVizProvenanceEntity entity,
    FVizId source_id,
    FVizSize* out_local_id);
/* local_to_upstream contains tuple indices into upstream_ids. The output is a
 * UInt64 mapping to the ultimate source IDs and can be composed repeatedly. */
FVIZ_API FVizResult fviz_provenance_compose(
    const FVizDataArray* upstream_ids,
    const FVizDataArray* local_to_upstream,
    FVizDataArray** out_ids);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_PROVENANCE_H */
