#ifndef FVIZ_DATA_DATA_SET_H
#define FVIZ_DATA_DATA_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataSet FVizDataSet;
#define FVIZ_TYPE_DATA_SET UINT64_C(0x5F342A9C7D18E602)

FVIZ_API FVizResult fviz_data_set_create(FVizDataSet** out_data_set);
FVIZ_API FVizSize fviz_data_set_point_count(const FVizDataSet* data_set);
FVIZ_API FVizSize fviz_data_set_cell_count(const FVizDataSet* data_set);
FVIZ_API FVizResult fviz_data_set_set_point_count(FVizDataSet* data_set, FVizSize count);
FVIZ_API FVizResult fviz_data_set_set_cell_count(FVizDataSet* data_set, FVizSize count);
FVIZ_API FVizAttributeSet* fviz_data_set_point_data(FVizDataSet* data_set);
FVIZ_API FVizAttributeSet* fviz_data_set_cell_data(FVizDataSet* data_set);
FVIZ_API FVizAttributeSet* fviz_data_set_field_data(FVizDataSet* data_set);
FVIZ_API FVizResult fviz_data_set_validate(const FVizDataSet* data_set);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_SET_H */
