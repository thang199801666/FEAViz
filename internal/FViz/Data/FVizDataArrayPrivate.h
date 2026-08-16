#ifndef FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataArray.h>

#define FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY 16u

typedef struct FVizDataArrayDirtyRecord
{
    FVizMTime mtime;
    FVizSize first;
    FVizSize count;
    FVizBool full;
} FVizDataArrayDirtyRecord;

struct FVizDataArray
{
    FVizObject base;
    FVizDataType type;
    uint32_t components;
    FVizSize tuple_stride;
    FVizArray* storage;
    void* external_data;
    FVizSize external_tuple_count;
    FVizDataArrayExternalReleaseCallback external_release_callback;
    void* external_release_user_data;
    FVizBool external;
    FVizBool mutable_data;
    FVizBool range_cache_valid;
    FVizBool range_cache_ignore_non_finite;
    int32_t range_cache_component;
    FVizMTime range_cache_mtime;
    double range_cache_minimum;
    double range_cache_maximum;
    FVizDataArrayDirtyRecord dirty_history[FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY];
    uint32_t dirty_history_begin;
    uint32_t dirty_history_count;
};

#endif /* FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H */
