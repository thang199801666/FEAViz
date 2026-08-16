#ifndef FVIZ_ALGORITHMS_RESAMPLE_WITH_DATA_SET_H
#define FVIZ_ALGORITHMS_RESAMPLE_WITH_DATA_SET_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizResampleWithDataSet FVizResampleWithDataSet;
#define FVIZ_TYPE_RESAMPLE_WITH_DATA_SET UINT64_C(0x8C71A5F0D9E2346B)

/* Samples source point-data arrays onto the points of a PolyData geometry.
 * 0.27 supports ImageData directly and UnstructuredGrid through the accelerated probe path. */
FVIZ_API FVizResult fviz_resample_with_data_set_create(FVizResampleWithDataSet** out_filter);
FVIZ_API FVizResult fviz_resample_with_data_set_set_input_data(FVizResampleWithDataSet* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_resample_with_data_set_set_input_connection(FVizResampleWithDataSet* filter,
                                                                     FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_resample_with_data_set_set_source_data(FVizResampleWithDataSet* filter,
                                                                FVizDataObject* source);
FVIZ_API FVizResult fviz_resample_with_data_set_set_source_connection(FVizResampleWithDataSet* filter,
                                                                      FVizAlgorithmOutput* source);
FVIZ_API FVizAlgorithm* fviz_resample_with_data_set_algorithm(FVizResampleWithDataSet* filter);
FVIZ_API FVizAlgorithmOutput* fviz_resample_with_data_set_output_port(FVizResampleWithDataSet* filter);
FVIZ_API FVizPolyData* fviz_resample_with_data_set_output(FVizResampleWithDataSet* filter);
FVIZ_API FVizResult fviz_resample_with_data_set_update(FVizResampleWithDataSet* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_RESAMPLE_WITH_DATA_SET_H */
