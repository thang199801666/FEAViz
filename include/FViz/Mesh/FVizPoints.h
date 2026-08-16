#ifndef FVIZ_MESH_POINTS_H
#define FVIZ_MESH_POINTS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPoints FVizPoints;
#define FVIZ_TYPE_POINTS UINT64_C(0xA1C7B52D8E304F69)

FVIZ_DATA_API FVizResult fviz_points_create(FVizPoints** out_points);
FVIZ_DATA_API void fviz_points_clear(FVizPoints* points);
FVIZ_DATA_API FVizResult fviz_points_reserve(FVizPoints* points, FVizSize capacity);
FVIZ_DATA_API FVizResult fviz_points_append(FVizPoints* points, FVizVec3 point, uint32_t* out_id);
FVIZ_DATA_API FVizResult fviz_points_append_many(FVizPoints* points, const FVizVec3* values, FVizSize count,
                                            uint32_t* out_first_id);
FVIZ_DATA_API FVizResult fviz_points_append_id(FVizPoints* points, FVizVec3 point, FVizId* out_id);
FVIZ_DATA_API FVizResult fviz_points_append_many_ids(FVizPoints* points, const FVizVec3* values, FVizSize count,
                                                FVizId* out_first_id);
/* Update existing coordinates without reallocating topology storage.  Batch updates
 * emit one ModifiedEvent and lazily refresh the cached bounds on demand. */
FVIZ_DATA_API FVizResult fviz_points_set(FVizPoints* points, FVizSize index, FVizVec3 value);
FVIZ_DATA_API FVizResult fviz_points_set_many(FVizPoints* points, FVizSize first, const FVizVec3* values, FVizSize count);
FVIZ_DATA_API FVizSize fviz_points_count(const FVizPoints* points);
FVIZ_DATA_API const FVizVec3* fviz_points_data(const FVizPoints* points);
FVIZ_DATA_API FVizBounds fviz_points_bounds(const FVizPoints* points);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_POINTS_H */
