#include <string.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)


typedef struct VisitState
{
    int count;
    FVizSize depths[8];
    const char* names[8];
} VisitState;

static FVizResult record_visit(
    const FVizMultiBlockDataSet* parent, FVizSize index,
    const FVizDataObject* block, const char* name, FVizSize depth, void* user_data)
{
    VisitState* state = (VisitState*)user_data;
    (void)parent; (void)index; (void)block;
    if (state->count >= 8) return FVIZ_ERROR_OVERFLOW;
    state->depths[state->count] = depth;
    state->names[state->count] = name;
    ++state->count;
    return FVIZ_OK;
}

static FVizBool count_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    int* count = (int*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    ++(*count);
    return FVIZ_FALSE;
}

int main(void)
{
    FVizMultiBlockDataSet* root = NULL;
    FVizMultiBlockDataSet* assembly = NULL;
    FVizPartitionedDataSet* partitions = NULL;
    FVizImageData* image = NULL;
    FVizSize index = 99u;
    FVizMTime before;
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    int modified = 0;
    CHECK(fviz_multi_block_data_set_create(&root) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_create(&assembly) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_create(&partitions) == FVIZ_OK);
    CHECK(fviz_image_data_create(&image) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_reserve(root, 4u) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(
        assembly, (FVizDataObject*)partitions, "Partitions", NULL) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(
        root, (FVizDataObject*)assembly, "Assembly", &index) == FVIZ_OK);
    CHECK(index == 0u);
    CHECK(fviz_multi_block_data_set_add_block(
        root, (FVizDataObject*)image, "Results", NULL) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_count(root) == 2u);
    CHECK(fviz_multi_block_data_set_find_block(root, "Results", &index) == FVIZ_TRUE && index == 1u);
    CHECK(strcmp(fviz_multi_block_data_set_block_name(root, 0u), "Assembly") == 0);
    CHECK(fviz_multi_block_data_set_validate(root) == FVIZ_OK);
    {
        VisitState visit = {0};
        CHECK(fviz_multi_block_data_set_leaf_count(root, FVIZ_TRUE) == 2u);
        CHECK(fviz_multi_block_data_set_leaf_count(root, FVIZ_FALSE) == 1u);
        CHECK(fviz_multi_block_data_set_visit(
            root, FVIZ_TRUE, FVIZ_FALSE, record_visit, &visit) == FVIZ_OK);
        CHECK(visit.count == 3);
        CHECK(visit.depths[0] == 0u && strcmp(visit.names[0], "Assembly") == 0);
        CHECK(visit.depths[1] == 1u && strcmp(visit.names[1], "Partitions") == 0);
        CHECK(visit.depths[2] == 0u && strcmp(visit.names[2], "Results") == 0);
    }

    /* Deep child changes bridge through nested composite levels in O(1) MTime queries. */
    before = fviz_object_mtime((const FVizObject*)root);
    fviz_object_modified((FVizObject*)partitions);
    CHECK(fviz_object_mtime((const FVizObject*)root) > before);

    /* Retain cycles are rejected, including indirect root <- assembly <- root. */
    CHECK(fviz_multi_block_data_set_add_block(
        assembly, (FVizDataObject*)root, "Cycle", NULL) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_multi_block_data_set_add_block(
        root, (FVizDataObject*)root, "Self", NULL) == FVIZ_ERROR_INVALID_ARGUMENT);

    CHECK(fviz_object_add_observer(
        (FVizObject*)root, FVIZ_EVENT_MODIFIED, 0.0f, count_modified, &modified, &tag) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_set_block_name(root, 1u, "Results") == FVIZ_OK);
    CHECK(modified == 0);
    CHECK(fviz_multi_block_data_set_set_block_name(root, 1u, "FrameResults") == FVIZ_OK);
    CHECK(modified == 1);
    CHECK(fviz_multi_block_data_set_remove_block(root, 0u) == FVIZ_OK);
    modified = 0;
    fviz_object_modified((FVizObject*)partitions);
    CHECK(modified == 0); /* Removed subtree must not emit ghost notifications. */
    CHECK(fviz_object_remove_observer((FVizObject*)root, tag) == FVIZ_OK);

    fviz_release(image);
    fviz_release(partitions);
    fviz_release(assembly);
    fviz_release(root);
    return 0;
}
