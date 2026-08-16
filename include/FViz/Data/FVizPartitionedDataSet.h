#ifndef FVIZ_DATA_PARTITIONED_DATA_SET_H
#define FVIZ_DATA_PARTITIONED_DATA_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPartitionedDataSet FVizPartitionedDataSet;
#define FVIZ_TYPE_PARTITIONED_DATA_SET UINT64_C(0xE971A55A3D920F61)

FVIZ_API FVizResult fviz_partitioned_data_set_create(FVizPartitionedDataSet** out_data_set);
FVIZ_API FVizSize fviz_partitioned_data_set_count(const FVizPartitionedDataSet* data_set);
FVIZ_API FVizResult fviz_partitioned_data_set_reserve(FVizPartitionedDataSet* data_set, FVizSize capacity);
FVIZ_API FVizResult fviz_partitioned_data_set_resize(FVizPartitionedDataSet* data_set, FVizSize count);
FVIZ_API FVizResult fviz_partitioned_data_set_add_partition(FVizPartitionedDataSet* data_set, FVizDataObject* partition,
                                                            const char* name, FVizSize* out_index);
FVIZ_API FVizResult fviz_partitioned_data_set_set_partition(FVizPartitionedDataSet* data_set, FVizSize index,
                                                            FVizDataObject* partition);
FVIZ_API FVizDataObject* fviz_partitioned_data_set_partition(FVizPartitionedDataSet* data_set, FVizSize index);
FVIZ_API const FVizDataObject* fviz_partitioned_data_set_const_partition(const FVizPartitionedDataSet* data_set,
                                                                         FVizSize index);
FVIZ_API FVizResult fviz_partitioned_data_set_set_partition_name(FVizPartitionedDataSet* data_set, FVizSize index,
                                                                 const char* name);
FVIZ_API const char* fviz_partitioned_data_set_partition_name(const FVizPartitionedDataSet* data_set, FVizSize index);
FVIZ_API FVizResult fviz_partitioned_data_set_remove_partition(FVizPartitionedDataSet* data_set, FVizSize index);
FVIZ_API void fviz_partitioned_data_set_clear(FVizPartitionedDataSet* data_set);
FVIZ_API FVizResult fviz_partitioned_data_set_validate(const FVizPartitionedDataSet* data_set);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_PARTITIONED_DATA_SET_H */
