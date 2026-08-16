#ifndef FVIZ_DATA_DATA_OBJECT_H
#define FVIZ_DATA_DATA_OBJECT_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataObject FVizDataObject;
#define FVIZ_TYPE_DATA_OBJECT UINT64_C(0x7D10B5A9E4C263F1)

FVIZ_API FVizBool fviz_data_object_is_data_object(const FVizDataObject* data_object);

/* Logical resident-memory estimate for data payloads and composite trees.
 * Shared child objects are counted once, so MultiBlock/Partitioned structures
 * that reference the same data do not artificially inflate the estimate.
 * The estimate intentionally excludes allocator slack and GPU resources. */
typedef struct FVizDataObjectMemoryInfo
{
    FVizSize total_bytes;
    FVizSize object_bytes;
    FVizSize geometry_bytes;
    FVizSize topology_bytes;
    FVizSize attribute_bytes;
    FVizSize composite_bytes;
    FVizSize unique_object_count;
} FVizDataObjectMemoryInfo;

FVIZ_API FVizResult fviz_data_object_memory_info(const FVizDataObject* data_object, FVizDataObjectMemoryInfo* out_info);
FVIZ_API FVizSize fviz_data_object_memory_size(const FVizDataObject* data_object);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_OBJECT_H */
