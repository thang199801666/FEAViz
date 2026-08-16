#ifndef FVIZ_DATA_IMAGE_DATA_H
#define FVIZ_DATA_IMAGE_DATA_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizImageData FVizImageData;
#define FVIZ_TYPE_IMAGE_DATA UINT64_C(0xA4D0C96B74F25E31)

FVIZ_DATA_API FVizResult fviz_image_data_create(FVizImageData** out_image);
FVIZ_DATA_API void fviz_image_data_clear(FVizImageData* image);

/* Inclusive structured extent: [xmin,xmax,ymin,ymax,zmin,zmax]. */
FVIZ_DATA_API FVizResult fviz_image_data_set_extent(FVizImageData* image, const int64_t extent[6]);
FVIZ_DATA_API void fviz_image_data_extent(const FVizImageData* image, int64_t out_extent[6]);
FVIZ_DATA_API void fviz_image_data_dimensions(const FVizImageData* image, FVizSize out_dimensions[3]);
FVIZ_DATA_API uint32_t fviz_image_data_dimension(const FVizImageData* image);

/* Origin is the physical location of structured index (0,0,0), matching VTK semantics. */
FVIZ_DATA_API FVizResult fviz_image_data_set_origin(FVizImageData* image, const double origin[3]);
FVIZ_DATA_API void fviz_image_data_origin(const FVizImageData* image, double out_origin[3]);
FVIZ_DATA_API FVizResult fviz_image_data_set_spacing(FVizImageData* image, const double spacing[3]);
FVIZ_DATA_API void fviz_image_data_spacing(const FVizImageData* image, double out_spacing[3]);
/* Row-major 3x3 direction matrix. It must be finite and invertible. */
FVIZ_DATA_API FVizResult fviz_image_data_set_direction(FVizImageData* image, const double direction[9]);
FVIZ_DATA_API void fviz_image_data_direction(const FVizImageData* image, double out_direction[9]);

FVIZ_DATA_API FVizSize fviz_image_data_point_count(const FVizImageData* image);
FVIZ_DATA_API FVizSize fviz_image_data_cell_count(const FVizImageData* image);
FVIZ_DATA_API FVizCellType fviz_image_data_cell_type(const FVizImageData* image);
FVIZ_DATA_API FVizBounds fviz_image_data_bounds(const FVizImageData* image);

FVIZ_DATA_API FVizResult fviz_image_data_point_id(const FVizImageData* image, int64_t i, int64_t j, int64_t k,
                                             FVizId* out_point_id);
FVIZ_DATA_API FVizResult fviz_image_data_point_ijk(const FVizImageData* image, FVizId point_id, int64_t out_ijk[3]);
FVIZ_DATA_API FVizResult fviz_image_data_point(const FVizImageData* image, FVizId point_id, FVizVec3* out_point);
FVIZ_DATA_API FVizResult fviz_image_data_cell_id(const FVizImageData* image, int64_t i, int64_t j, int64_t k,
                                            FVizId* out_cell_id);
FVIZ_DATA_API FVizResult fviz_image_data_cell_ijk(const FVizImageData* image, FVizId cell_id, int64_t out_ijk[3]);
FVIZ_DATA_API FVizResult fviz_image_data_cell_point_ids(const FVizImageData* image, FVizId cell_id, FVizId out_point_ids[8],
                                                   uint32_t* out_point_count);
FVIZ_DATA_API FVizResult fviz_image_data_index_to_physical(const FVizImageData* image, const double index[3],
                                                      double out_physical[3]);
FVIZ_DATA_API FVizResult fviz_image_data_physical_to_continuous_index(const FVizImageData* image, const double physical[3],
                                                                 double out_index[3]);
FVIZ_DATA_API FVizResult fviz_image_data_sample_point_array(const FVizImageData* image, const char* array_name,
                                                       const double physical[3], uint32_t component, double* out_value);
FVIZ_DATA_API FVizResult fviz_image_data_sample_active_scalars(const FVizImageData* image, const double physical[3],
                                                          uint32_t component, double* out_value);

FVIZ_DATA_API FVizAttributeSet* fviz_image_data_point_data(FVizImageData* image);
FVIZ_DATA_API FVizAttributeSet* fviz_image_data_cell_data(FVizImageData* image);
FVIZ_DATA_API FVizAttributeSet* fviz_image_data_field_data(FVizImageData* image);
FVIZ_DATA_API const FVizAttributeSet* fviz_image_data_const_point_data(const FVizImageData* image);
FVIZ_DATA_API const FVizAttributeSet* fviz_image_data_const_cell_data(const FVizImageData* image);
FVIZ_DATA_API const FVizAttributeSet* fviz_image_data_const_field_data(const FVizImageData* image);

FVIZ_DATA_API FVizResult fviz_image_data_allocate_point_scalars(FVizImageData* image, const char* name, FVizDataType type,
                                                           uint32_t components, FVizDataArray** out_array);
FVIZ_DATA_API FVizResult fviz_image_data_allocate_cell_scalars(FVizImageData* image, const char* name, FVizDataType type,
                                                          uint32_t components, FVizDataArray** out_array);
FVIZ_DATA_API FVizResult fviz_image_data_validate(const FVizImageData* image);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_IMAGE_DATA_H */
