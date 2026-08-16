#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static int write_point(const char* path, float x)
{
    FVizPolyData* poly = NULL;
    FVizVTPWriterOptions options;
    if (fviz_poly_data_create(&poly) != FVIZ_OK ||
        fviz_poly_data_add_point(poly, fviz_vec3(x,0,0), NULL) != FVIZ_OK)
    { fviz_release(poly); return 0; }
    fviz_vtp_writer_options_initialize(&options);
    if (fviz_vtp_write(path, poly, &options) != FVIZ_OK) { fviz_release(poly); return 0; }
    fviz_release(poly);
    return 1;
}

static int write_tet_vtu(const char* path, float x)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizVTUWriterOptions options;
    FVizVec3 points[4] = {
        {0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f}, {0.0f,0.0f,1.0f}
    };
    const uint32_t ids[4] = {0u,1u,2u,3u};
    FVizSize i;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return 0;
    for (i=0u;i<4u;++i) points[i].x += x;
    if (fviz_unstructured_grid_add_points(grid,points,4u,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell(grid,FVIZ_CELL_TETRA,4u,ids)!=FVIZ_OK)
    { fviz_release(grid); return 0; }
    fviz_vtu_writer_options_initialize(&options);
    options.output_mode = FVIZ_VTU_OUTPUT_ASCII;
    if (fviz_vtu_write(path,grid,&options)!=FVIZ_OK) { fviz_release(grid); return 0; }
    fviz_release(grid);
    return 1;
}

int main(void)
{
    FVizPVDCollection* collection = NULL;
    FVizPVDReader* reader = NULL;
    FVizPVDCollection* parsed = NULL;
    FVizAlgorithm* algorithm;
    FVizDataObject* output;
    const double* times;
    FVizSize count = 0u;

    CHECK(write_point("pvd_t0.vtp", 0.0f));
    CHECK(write_point("pvd_t1_a.vtp", 10.0f));
    CHECK(write_point("pvd_t1_b.vtp", 20.0f));
    CHECK(fviz_pvd_collection_create(&collection) == FVIZ_OK);
    CHECK(fviz_pvd_collection_add(collection, 0.0, 0u, "Main & Model", "pvd_t0.vtp", NULL) == FVIZ_OK);
    CHECK(fviz_pvd_collection_add(collection, 1.0, 0u, "Part-A", "pvd_t1_a.vtp", NULL) == FVIZ_OK);
    CHECK(fviz_pvd_collection_add(collection, 1.0, 1u, "Part-B", "pvd_t1_b.vtp", NULL) == FVIZ_OK);
    CHECK(fviz_pvd_write("series.pvd", collection) == FVIZ_OK);
    CHECK(fviz_pvd_read("series.pvd", &parsed) == FVIZ_OK);
    CHECK(strcmp(fviz_pvd_collection_group(parsed, 0u), "Main & Model") == 0);
    fviz_release(parsed); parsed = NULL;

    CHECK(fviz_pvd_reader_create(&reader) == FVIZ_OK);
    CHECK(fviz_pvd_reader_set_file_name(reader, "series.pvd") == FVIZ_OK);
    CHECK(fviz_pvd_reader_update_time(reader, 0.1) == FVIZ_OK);
    CHECK(fabs(fviz_pvd_reader_selected_time(reader) - 0.0) < 1e-12);
    output = fviz_pvd_reader_output(reader);
    CHECK(output != NULL && fviz_object_is_type((FVizObject*)output, FVIZ_TYPE_POLY_DATA));
    CHECK(fabs((double)fviz_poly_data_points((FVizPolyData*)output)[0].x - 0.0) < 1e-6);

    algorithm = fviz_pvd_reader_algorithm(reader);
    times = fviz_algorithm_output_time_steps(algorithm, 0u, &count);
    CHECK(times != NULL && count == 2u && times[0] == 0.0 && times[1] == 1.0);

    {
        uint32_t piece_count = 0u;
        CHECK(fviz_pvd_reader_piece_count_at_time(reader, 1.0, &piece_count) == FVIZ_OK);
        CHECK(piece_count == 2u);
        CHECK(fviz_pvd_reader_update_piece_time(reader, 1.0, 1u, 2u, 0u) == FVIZ_OK);
        output = fviz_pvd_reader_output(reader);
        CHECK(output != NULL && fviz_object_is_type((FVizObject*)output, FVIZ_TYPE_POLY_DATA));
        CHECK(fabs((double)fviz_poly_data_points((FVizPolyData*)output)[0].x - 20.0) < 1e-6);
    }
    CHECK(fviz_pvd_reader_update_time(reader, 0.8) == FVIZ_OK);
    CHECK(fabs(fviz_pvd_reader_selected_time(reader) - 1.0) < 1e-12);
    output = fviz_pvd_reader_output(reader);
    CHECK(output != NULL && fviz_object_is_type((FVizObject*)output, FVIZ_TYPE_PARTITIONED_DATA_SET));
    CHECK(fviz_partitioned_data_set_count((FVizPartitionedDataSet*)output) == 2u);
    CHECK(fabs((double)fviz_poly_data_points((FVizPolyData*)fviz_partitioned_data_set_partition((FVizPartitionedDataSet*)output, 0u))[0].x - 10.0) < 1e-6);
    CHECK(fabs((double)fviz_poly_data_points((FVizPolyData*)fviz_partitioned_data_set_partition((FVizPartitionedDataSet*)output, 1u))[0].x - 20.0) < 1e-6);
    {
        FVizDataObject* cached_output = output;
        FVizPVDCacheStatistics stats;
        CHECK(fviz_pvd_reader_update_time(reader, 1.2) == FVIZ_OK);
        CHECK(fabs(fviz_pvd_reader_selected_time(reader) - 1.0) < 1e-12);
        CHECK(fviz_pvd_reader_output(reader) == cached_output);
        stats=fviz_pvd_reader_cache_statistics(reader);
        CHECK(stats.capacity==3u && stats.size==2u && stats.hits>=1u && stats.misses>=2u);
        CHECK(fviz_pvd_reader_set_cache_capacity(reader,1u)==FVIZ_OK);
        CHECK(fviz_pvd_reader_cache_capacity(reader)==1u);
        CHECK(fviz_pvd_reader_update_time(reader,0.0)==FVIZ_OK);
        CHECK(fviz_pvd_reader_update_time(reader,1.0)==FVIZ_OK);
        CHECK(fviz_pvd_reader_update_time(reader,0.0)==FVIZ_OK);
        stats=fviz_pvd_reader_cache_statistics(reader);
        CHECK(stats.capacity==1u && stats.size==1u && stats.evictions>=2u);
        fviz_pvd_reader_clear_cache(reader);
        CHECK(fviz_pvd_reader_cache_statistics(reader).size==0u);
        CHECK(fviz_pvd_reader_set_cache_capacity(reader,3u)==FVIZ_OK);
        {
            const double selected_before=fviz_pvd_reader_selected_time(reader);
            CHECK(fviz_pvd_reader_prefetch_time(reader,1.0)==FVIZ_OK);
            CHECK(fviz_pvd_reader_cache_statistics(reader).size==1u);
            CHECK(fabs(fviz_pvd_reader_selected_time(reader)-selected_before)<1e-12);
            CHECK(fviz_pvd_reader_update_time(reader,1.0)==FVIZ_OK);
            CHECK(fviz_pvd_reader_cache_statistics(reader).hits>=1u);
        }
        {
            FVizPVDCacheStatistics budget_stats;
            FVizSize single_frame_bytes;
            CHECK(fviz_pvd_reader_update_time(reader, 0.0) == FVIZ_OK);
            single_frame_bytes = fviz_data_object_memory_size(fviz_pvd_reader_output(reader));
            CHECK(single_frame_bytes > 0u);
            fviz_pvd_reader_clear_cache(reader);
            CHECK(fviz_pvd_reader_set_cache_byte_capacity(reader, single_frame_bytes) == FVIZ_OK);
            CHECK(fviz_pvd_reader_cache_byte_capacity(reader) == single_frame_bytes);
            CHECK(fviz_pvd_reader_update_time(reader, 0.0) == FVIZ_OK);
            CHECK(fviz_pvd_reader_update_time(reader, 1.0) == FVIZ_OK);
            budget_stats = fviz_pvd_reader_cache_statistics(reader);
            CHECK(budget_stats.byte_capacity == single_frame_bytes);
            CHECK(budget_stats.bytes <= single_frame_bytes);
            CHECK(budget_stats.oversize_skips >= 1u);
            CHECK(fviz_pvd_reader_set_cache_byte_capacity(reader, 0u) == FVIZ_OK);
        }
    }

    fviz_release(reader);
    reader = NULL;

    /* PVD may reference a parallel PVTU frame, yielding a PartitionedDataSet
       whose children are the materialized VTU pieces. */
    CHECK(write_tet_vtu("pvd_parallel_0.vtu", 0.0f));
    CHECK(write_tet_vtu("pvd_parallel_1.vtu", 2.0f));
    {
        FILE* manifest = fopen("pvd_parallel.pvtu", "wb");
        CHECK(manifest != NULL);
        CHECK(fprintf(manifest,
            "<?xml version=\"1.0\"?>\n"
            "<VTKFile type=\"PUnstructuredGrid\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
            "  <PUnstructuredGrid GhostLevel=\"0\">\n"
            "    <Piece Source=\"pvd_parallel_0.vtu\"/>\n"
            "    <Piece Source=\"pvd_parallel_1.vtu\"/>\n"
            "  </PUnstructuredGrid>\n"
            "</VTKFile>\n") > 0);
        CHECK(fclose(manifest) == 0);
    }
    {
        FVizPVDCollection* parallel_collection = NULL;
        CHECK(fviz_pvd_collection_create(&parallel_collection) == FVIZ_OK);
        CHECK(fviz_pvd_collection_add(
            parallel_collection, 0.0, 0u, "Parallel", "pvd_parallel.pvtu", NULL) == FVIZ_OK);
        CHECK(fviz_pvd_write("parallel_series.pvd", parallel_collection) == FVIZ_OK);
        fviz_release(parallel_collection);
    }
    CHECK(fviz_pvd_reader_create(&reader) == FVIZ_OK);
    CHECK(fviz_pvd_reader_set_file_name(reader, "parallel_series.pvd") == FVIZ_OK);
    {
        uint32_t piece_count = 0u;
        const double selected_before = fviz_pvd_reader_selected_time(reader);
        CHECK(fviz_pvd_reader_piece_count_at_time(reader, 0.0, &piece_count) == FVIZ_OK);
        CHECK(piece_count == 2u);
        CHECK(fviz_pvd_reader_prefetch_piece_time(reader, 0.0, 1u, 2u, 0u) == FVIZ_OK);
        CHECK(fabs(fviz_pvd_reader_selected_time(reader) - selected_before) < 1e-12);
        CHECK(fviz_pvd_reader_update_piece_time(reader, 0.0, 1u, 2u, 0u) == FVIZ_OK);
        output = fviz_pvd_reader_output(reader);
        CHECK(output != NULL && fviz_object_is_type((FVizObject*)output, FVIZ_TYPE_UNSTRUCTURED_GRID));
        CHECK(fabs((double)fviz_unstructured_grid_bounds((FVizUnstructuredGrid*)output).min.x - 2.0) < 1e-6);
        CHECK(fviz_pvd_reader_update_piece_time(reader, 0.0, 0u, 3u, 0u) == FVIZ_ERROR_INVALID_ARGUMENT);
    }
    CHECK(fviz_pvd_reader_update_time(reader, 0.0) == FVIZ_OK);
    output = fviz_pvd_reader_output(reader);
    CHECK(output != NULL && fviz_object_is_type((FVizObject*)output, FVIZ_TYPE_PARTITIONED_DATA_SET));
    CHECK(fviz_partitioned_data_set_count((FVizPartitionedDataSet*)output) == 2u);
    CHECK(fviz_object_is_type(
        (FVizObject*)fviz_partitioned_data_set_partition((FVizPartitionedDataSet*)output,0u),
        FVIZ_TYPE_UNSTRUCTURED_GRID));
    CHECK(fabs((double)fviz_unstructured_grid_bounds(
        (FVizUnstructuredGrid*)fviz_partitioned_data_set_partition((FVizPartitionedDataSet*)output,1u)).min.x - 2.0) < 1e-6);
    fviz_release(reader);
    reader = NULL;
    (void)remove("parallel_series.pvd");
    (void)remove("pvd_parallel.pvtu");
    (void)remove("pvd_parallel_0.vtu");
    (void)remove("pvd_parallel_1.vtu");

    fviz_release(parsed);
    fviz_release(collection);
    (void)remove("series.pvd");
    (void)remove("pvd_t0.vtp");
    (void)remove("pvd_t1_a.vtp");
    (void)remove("pvd_t1_b.vtp");
    return 0;
}
