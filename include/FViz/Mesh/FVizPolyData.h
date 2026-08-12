#ifndef FVIZ_MESH_POLY_DATA_H
#define FVIZ_MESH_POLY_DATA_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPolyData FVizPolyData;
#define FVIZ_TYPE_POLY_DATA UINT64_C(0xCC28638594EC02C7)

FVIZ_API FVizResult fviz_poly_data_create(FVizPolyData** out_poly_data);
FVIZ_API void fviz_poly_data_clear(FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_reserve(FVizPolyData* poly_data, FVizSize point_capacity, FVizSize triangle_capacity);
FVIZ_API FVizResult fviz_poly_data_add_point(FVizPolyData* poly_data, FVizVec3 point, uint32_t* out_index);
FVIZ_API FVizResult fviz_poly_data_add_triangle(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c);
FVIZ_API FVizSize fviz_poly_data_point_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_triangle_count(const FVizPolyData* poly_data);
FVIZ_API const FVizVec3* fviz_poly_data_points(const FVizPolyData* poly_data);
FVIZ_API const FVizVec3* fviz_poly_data_normals(const FVizPolyData* poly_data);
FVIZ_API const uint32_t* fviz_poly_data_triangle_indices(const FVizPolyData* poly_data);
FVIZ_API FVizBool fviz_poly_data_has_normals(const FVizPolyData* poly_data);
FVIZ_API FVizBounds fviz_poly_data_bounds(const FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_compute_normals(FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_validate(const FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_set_scalars(FVizPolyData* poly_data, FVizDataArray* scalars);
FVIZ_API const FVizDataArray* fviz_poly_data_const_scalars(const FVizPolyData* poly_data);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_POLY_DATA_H */
