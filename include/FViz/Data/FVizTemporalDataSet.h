#ifndef FVIZ_DATA_TEMPORAL_DATA_SET_H
#define FVIZ_DATA_TEMPORAL_DATA_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTemporalDataSet FVizTemporalDataSet;
#define FVIZ_TYPE_TEMPORAL_DATA_SET UINT64_C(0xD304C3F0A9E8B572)

FVIZ_API FVizResult fviz_temporal_data_set_create(FVizTemporalDataSet** out_data_set);
FVIZ_API FVizSize fviz_temporal_data_set_step_count(const FVizTemporalDataSet* data_set);
FVIZ_API FVizResult fviz_temporal_data_set_reserve(FVizTemporalDataSet* data_set, FVizSize capacity);
FVIZ_API FVizResult fviz_temporal_data_set_add_step(FVizTemporalDataSet* data_set, double time, FVizDataObject* data,
                                                    FVizSize* out_index);
FVIZ_API FVizResult fviz_temporal_data_set_set_step(FVizTemporalDataSet* data_set, FVizSize index, double time,
                                                    FVizDataObject* data);
/* Fast append for already ordered frame streams.  Times must be finite, strictly
 * increasing, and newer than the current last step.  The dataset emits one
 * ModifiedEvent for the whole batch. */
FVIZ_API FVizResult fviz_temporal_data_set_append_steps(FVizTemporalDataSet* data_set, const double* times,
                                                        FVizDataObject* const* data, FVizSize count,
                                                        FVizSize* out_first_index);
FVIZ_API double fviz_temporal_data_set_time(const FVizTemporalDataSet* data_set, FVizSize index);
FVIZ_API FVizDataObject* fviz_temporal_data_set_data(FVizTemporalDataSet* data_set, FVizSize index);
FVIZ_API const FVizDataObject* fviz_temporal_data_set_const_data(const FVizTemporalDataSet* data_set, FVizSize index);
FVIZ_API FVizResult fviz_temporal_data_set_time_range(const FVizTemporalDataSet* data_set, double* out_minimum,
                                                      double* out_maximum);
FVIZ_API FVizResult fviz_temporal_data_set_find_nearest(const FVizTemporalDataSet* data_set, double time,
                                                        FVizSize* out_index);
/* Finds interpolation neighbors.  Outside the time range both indices clamp to
 * the nearest endpoint and alpha is zero.  Inside the range alpha is in [0,1]. */
FVIZ_API FVizResult fviz_temporal_data_set_find_bracket(const FVizTemporalDataSet* data_set, double time,
                                                        FVizSize* out_lower_index, FVizSize* out_upper_index,
                                                        double* out_alpha);
FVIZ_API FVizResult fviz_temporal_data_set_remove_step(FVizTemporalDataSet* data_set, FVizSize index);
FVIZ_API void fviz_temporal_data_set_clear(FVizTemporalDataSet* data_set);
FVIZ_API FVizResult fviz_temporal_data_set_validate(const FVizTemporalDataSet* data_set);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_TEMPORAL_DATA_SET_H */
