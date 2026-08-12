#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

typedef struct FillContext
{
    uint64_t* values;
} FillContext;

typedef struct NestedContext
{
    uint64_t* values;
    FVizSize count;
} NestedContext;

static void fill_range(FVizSize begin, FVizSize end, void* user_data)
{
    FillContext* context = (FillContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i) context->values[i] = (uint64_t)i * UINT64_C(17) + UINT64_C(3);
}

static void increment_range(FVizSize begin, FVizSize end, void* user_data)
{
    NestedContext* context = (NestedContext*)user_data;
    FVizSize i;
    for (i = begin; i < end && i < context->count; ++i) ++context->values[i];
}

static void nested_range(FVizSize begin, FVizSize end, void* user_data)
{
    NestedContext* context = (NestedContext*)user_data;
    (void)fviz_parallel_for(begin, end, 16u, increment_range, context);
}

int main(void)
{
    const FVizSize count = 100000u;
    uint64_t* values = (uint64_t*)fviz_alloc(count * sizeof(uint64_t));
    FillContext context;
    NestedContext nested;
    uint64_t dispatches;
    FVizSize i;
    CHECK(values != NULL);
    context.values = values;
    nested.values = values;
    nested.count = count;
    CHECK(fviz_parallel_hardware_thread_count() >= 1u);
    CHECK(fviz_parallel_worker_count() + 1u <= fviz_parallel_hardware_thread_count());
    fviz_parallel_set_thread_limit(4u);
    CHECK(fviz_parallel_thread_limit() >= 1u && fviz_parallel_thread_limit() <= 4u);
    CHECK(fviz_parallel_for(0u, count, 1024u, fill_range, &context) == FVIZ_OK);
    for (i = 0u; i < count; ++i) CHECK(values[i] == (uint64_t)i * UINT64_C(17) + UINT64_C(3));
    dispatches = fviz_parallel_dispatch_count();
    CHECK(fviz_parallel_for(0u, count, 1024u, fill_range, &context) == FVIZ_OK);
    if (fviz_parallel_worker_count() > 0u)
        CHECK(fviz_parallel_dispatch_count() == dispatches + 1u);
    fviz_parallel_set_thread_limit(1u);
    dispatches = fviz_parallel_dispatch_count();
    CHECK(fviz_parallel_for(0u, count, 1024u, fill_range, &context) == FVIZ_OK);
    CHECK(fviz_parallel_dispatch_count() == dispatches);
    for (i = 0u; i < count; ++i) values[i] = 0u;
    fviz_parallel_set_thread_limit(4u);
    CHECK(fviz_parallel_for(0u, count, 4096u, nested_range, &nested) == FVIZ_OK);
    for (i = 0u; i < count; ++i) CHECK(values[i] == 1u);
    CHECK(fviz_parallel_for(10u, 10u, 1u, fill_range, &context) == FVIZ_OK);
    CHECK(fviz_parallel_for(10u, 9u, 1u, fill_range, &context) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_parallel_for(0u, 1u, 1u, NULL, &context) == FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_parallel_set_thread_limit(0u);
    fviz_free(values);
    return 0;
}
