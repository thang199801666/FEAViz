#ifndef FVIZ_DATA_DATA_ARRAY_H
#define FVIZ_DATA_DATA_ARRAY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataType.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataArray FVizDataArray;
#define FVIZ_TYPE_DATA_ARRAY UINT64_C(0x9DBB30DBD2611D25)

typedef enum FVizDataArrayExternalFlags
{
    FVIZ_DATA_ARRAY_EXTERNAL_IMMUTABLE = 0,
    FVIZ_DATA_ARRAY_EXTERNAL_MUTABLE = 1 << 0
} FVizDataArrayExternalFlags;

/* Called exactly once when the last reference to an external array is released.
 * The callback may be NULL when the caller guarantees that data outlives the array. */
typedef void (*FVizDataArrayExternalReleaseCallback)(void* data, void* user_data);

FVIZ_API FVizResult fviz_data_array_create(FVizDataType type, uint32_t components, FVizDataArray** out_array);
/* Creates a zero-copy, fixed-size view over caller-provided storage. External
 * arrays never resize or append. Mutable views permit in-place writes; immutable
 * views expose data only through the const accessors. */
FVIZ_API FVizResult fviz_data_array_create_external(FVizDataType type, uint32_t components, void* data,
                                                    FVizSize tuple_count, FVizDataArrayExternalFlags flags,
                                                    FVizDataArrayExternalReleaseCallback release_callback,
                                                    void* release_user_data, FVizDataArray** out_array);
FVIZ_API FVizBool fviz_data_array_is_external(const FVizDataArray* array);
FVIZ_API FVizBool fviz_data_array_is_mutable(const FVizDataArray* array);
FVIZ_API FVizDataType fviz_data_array_type(const FVizDataArray* array);
FVIZ_API uint32_t fviz_data_array_components(const FVizDataArray* array);
FVIZ_API FVizSize fviz_data_array_tuple_count(const FVizDataArray* array);
FVIZ_API FVizSize fviz_data_array_tuple_stride(const FVizDataArray* array);
FVIZ_API void* fviz_data_array_data(FVizDataArray* array);
FVIZ_API const void* fviz_data_array_const_data(const FVizDataArray* array);
FVIZ_API FVizResult fviz_data_array_resize(FVizDataArray* array, FVizSize tuple_count);
FVIZ_API FVizResult fviz_data_array_reserve(FVizDataArray* array, FVizSize tuple_capacity);
FVIZ_API FVizResult fviz_data_array_append_tuple(FVizDataArray* array, const void* tuple);
FVIZ_API FVizResult fviz_data_array_append_tuples(FVizDataArray* array, const void* tuples, FVizSize tuple_count);
FVIZ_API FVizResult fviz_data_array_set_tuple(FVizDataArray* array, FVizSize index, const void* tuple);
/* Replaces an existing contiguous tuple range and emits at most one ModifiedEvent.
 * This is the preferred path for FEA frame/result updates that preserve array shape. */
FVIZ_API FVizResult fviz_data_array_set_tuples(FVizDataArray* array, FVizSize first, const void* tuples,
                                               FVizSize tuple_count);
/* Marks a tuple range modified after writing through a mutable raw pointer.
 * Ordinary set/append/resize APIs record their dirty ranges automatically. */
FVIZ_API FVizResult fviz_data_array_mark_dirty(FVizDataArray* array, FVizSize first, FVizSize tuple_count);
/* Returns the union of changes newer than since_mtime. full is true when the
 * requested history is unavailable or the tuple layout changed. */
FVIZ_API FVizResult fviz_data_array_dirty_range_since(const FVizDataArray* array, FVizMTime since_mtime,
                                                      FVizDirtyRange* out_range);
FVIZ_API void* fviz_data_array_tuple(FVizDataArray* array, FVizSize index);
FVIZ_API const void* fviz_data_array_const_tuple(const FVizDataArray* array, FVizSize index);
FVIZ_API FVizResult fviz_data_array_deep_copy(const FVizDataArray* source, FVizDataArray** out_copy);
FVIZ_API FVizResult fviz_data_array_get_component(const FVizDataArray* array, FVizSize tuple_index, uint32_t component,
                                                  double* out_value);
FVIZ_API FVizResult fviz_data_array_set_component(FVizDataArray* array, FVizSize tuple_index, uint32_t component,
                                                  double value);
/* component >= 0 computes that component; component == -1 computes vector magnitude. */
FVIZ_API FVizResult fviz_data_array_get_range(const FVizDataArray* array, int32_t component, FVizBool ignore_non_finite,
                                              double* out_minimum, double* out_maximum);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_ARRAY_H */
