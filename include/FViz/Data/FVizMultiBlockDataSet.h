#ifndef FVIZ_DATA_MULTI_BLOCK_DATA_SET_H
#define FVIZ_DATA_MULTI_BLOCK_DATA_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizMultiBlockDataSet FVizMultiBlockDataSet;
#define FVIZ_TYPE_MULTI_BLOCK_DATA_SET UINT64_C(0xA68E349CC95142D7)

/* Pre-order composite traversal callback. parent/index/name identify the direct
 * slot containing block; depth is 0 for direct children of the visited root.
 * The hierarchy must not be mutated from inside the callback. Returning a
 * non-FVIZ_OK result stops traversal and propagates that result. */
typedef FVizResult (*FVizMultiBlockVisitFn)(const FVizMultiBlockDataSet* parent, FVizSize index,
                                            const FVizDataObject* block, const char* name, FVizSize depth,
                                            void* user_data);

FVIZ_DATA_API FVizResult fviz_multi_block_data_set_create(FVizMultiBlockDataSet** out_data_set);
FVIZ_DATA_API FVizSize fviz_multi_block_data_set_count(const FVizMultiBlockDataSet* data_set);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_reserve(FVizMultiBlockDataSet* data_set, FVizSize capacity);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_resize(FVizMultiBlockDataSet* data_set, FVizSize count);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_add_block(FVizMultiBlockDataSet* data_set, FVizDataObject* block,
                                                        const char* name, FVizSize* out_index);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_set_block(FVizMultiBlockDataSet* data_set, FVizSize index,
                                                        FVizDataObject* block);
FVIZ_DATA_API FVizDataObject* fviz_multi_block_data_set_block(FVizMultiBlockDataSet* data_set, FVizSize index);
FVIZ_DATA_API const FVizDataObject* fviz_multi_block_data_set_const_block(const FVizMultiBlockDataSet* data_set,
                                                                     FVizSize index);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_set_block_name(FVizMultiBlockDataSet* data_set, FVizSize index,
                                                             const char* name);
FVIZ_DATA_API const char* fviz_multi_block_data_set_block_name(const FVizMultiBlockDataSet* data_set, FVizSize index);
FVIZ_DATA_API FVizBool fviz_multi_block_data_set_find_block(const FVizMultiBlockDataSet* data_set, const char* name,
                                                       FVizSize* out_index);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_remove_block(FVizMultiBlockDataSet* data_set, FVizSize index);
FVIZ_DATA_API void fviz_multi_block_data_set_clear(FVizMultiBlockDataSet* data_set);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_validate(const FVizMultiBlockDataSet* data_set);
FVIZ_DATA_API FVizResult fviz_multi_block_data_set_visit(const FVizMultiBlockDataSet* data_set, FVizBool recursive,
                                                    FVizBool leaves_only, FVizMultiBlockVisitFn visitor,
                                                    void* user_data);
FVIZ_DATA_API FVizSize fviz_multi_block_data_set_leaf_count(const FVizMultiBlockDataSet* data_set, FVizBool recursive);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_MULTI_BLOCK_DATA_SET_H */
