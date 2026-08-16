#include <FViz/FViz.h>

#include <stdio.h>
#include <string.h>
#include <time.h>


static double benchmark_wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

typedef struct WarpData
{
    const FVizVec3* input;
    FVizVec3* output;
    float scale;
} WarpData;

static FVizResult warp_range(FVizSize begin, FVizSize end, void* user_data)
{
    WarpData* data = (WarpData*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        const FVizVec3 point = data->input[i];
        const float normalized_x = point.x / 64.0f;
        data->output[i] = fviz_vec3(
            point.x,
            point.y + data->scale * normalized_x * normalized_x,
            point.z + data->scale * 0.25f * normalized_x);
    }
    return FVIZ_OK;
}

static uint64_t hash_points(const FVizVec3* points, FVizSize count)
{
    const unsigned char* bytes = (const unsigned char*)points;
    const FVizSize byte_count = count * sizeof(*points);
    uint64_t hash = UINT64_C(1469598103934665603);
    FVizSize i;
    for (i = 0u; i < byte_count; ++i)
    {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int run_case(uint32_t cells_per_axis, uint32_t thread_count)
{
    const FVizSize edge = (FVizSize)cells_per_axis + 1u;
    const FVizSize point_count = edge * edge * edge;
    const FVizSize cell_count = (FVizSize)cells_per_axis * cells_per_axis * cells_per_axis;
    FVizVec3* input = (FVizVec3*)fviz_alloc(point_count * sizeof(*input));
    FVizVec3* output = (FVizVec3*)fviz_alloc(point_count * sizeof(*output));
    FVizParallelContextOptions options;
    FVizParallelContext* context = NULL;
    FVizParallelStatistics statistics;
    WarpData data;
    double started;
    double finished;
    FVizSize i;
    uint32_t iteration;
    uint64_t hash;
    if (input == NULL || output == NULL) return 2;
    for (i = 0u; i < point_count; ++i)
    {
        const FVizSize x = i % edge;
        const FVizSize y = (i / edge) % edge;
        const FVizSize z = i / (edge * edge);
        input[i] = fviz_vec3((float)x, (float)y, (float)z);
    }
    fviz_parallel_context_options_initialize(&options);
    options.thread_count = thread_count;
    if (fviz_parallel_context_create(&options, &context) != FVIZ_OK) return 3;
    data.input = input;
    data.output = output;
    data.scale = 4.0f;
    started = benchmark_wall_seconds();
    for (iteration = 0u; iteration < 5u; ++iteration)
    {
        if (fviz_parallel_context_for(
                context, 0u, point_count, 1024u,
                warp_range, &data, NULL) != FVIZ_OK)
            return 4;
    }
    finished = benchmark_wall_seconds();
    hash = hash_points(output, point_count);
    fviz_parallel_context_get_statistics(context, &statistics);
    printf(
        "%u,%llu,%llu,%u,%.6f,%llu,%llu,%llu\n",
        cells_per_axis,
        (unsigned long long)cell_count,
        (unsigned long long)point_count,
        fviz_parallel_context_thread_count(context),
        (finished - started) / 5.0,
        (unsigned long long)hash,
        (unsigned long long)statistics.chunk_count,
        (unsigned long long)(point_count * (sizeof(*input) + sizeof(*output))));
    fviz_parallel_context_destroy(context);
    fviz_free(output);
    fviz_free(input);
    return 0;
}

int main(void)
{
    const uint32_t sizes[] = {8u, 32u, 64u};
    uint32_t thread_counts[5] = {1u, 2u, 4u, 8u, 0u};
    uint32_t size_index;
    uint32_t thread_index;
    thread_counts[4] = fviz_parallel_hardware_thread_count();
    puts("edge_cells,cells,points,threads,seconds,output_hash,chunks,transient_bytes");
    for (size_index = 0u; size_index < 3u; ++size_index)
    {
        for (thread_index = 0u; thread_index < 5u; ++thread_index)
        {
            uint32_t previous;
            FVizBool duplicate = FVIZ_FALSE;
            for (previous = 0u; previous < thread_index; ++previous)
                if (thread_counts[previous] == thread_counts[thread_index]) duplicate = FVIZ_TRUE;
            if (duplicate == FVIZ_FALSE && run_case(sizes[size_index], thread_counts[thread_index]) != 0)
                return 1;
        }
    }
    return 0;
}
