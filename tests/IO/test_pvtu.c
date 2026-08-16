#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static int write_piece(const char* path, float x_offset, uint8_t ghost_flag)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* value = NULL;
    FVizDataArray* ghost = NULL;
    FVizVTUWriterOptions options;
    const FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
    };
    FVizVec3 shifted[4];
    const uint32_t ids[4] = {0u, 1u, 2u, 3u};
    float scalar = 10.0f + x_offset;
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i)
        shifted[i] = fviz_vec3(points[i].x + x_offset, points[i].y, points[i].z);
    CHECK(fviz_unstructured_grid_add_points(grid, shifted, 4u, NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &value) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(value, &scalar) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "pieceValue", value) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &ghost) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(ghost, &ghost_flag) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), FVIZ_GHOST_ARRAY_NAME, ghost) == FVIZ_OK);
    fviz_vtu_writer_options_initialize(&options);
    options.output_mode = FVIZ_VTU_OUTPUT_ASCII;
    CHECK(fviz_vtu_write(path, grid, &options) == FVIZ_OK);
    fviz_release(ghost);
    fviz_release(value);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    const char* piece0 = "fviz_pvtu_piece0.vtu";
    const char* piece1 = "fviz_pvtu_piece1.vtu";
    const char* manifest = "fviz_pvtu_test.pvtu";
    FILE* file;
    FVizPartitionedDataSet* data = NULL;
    FVizPVTUReaderOptions options;
    const FVizUnstructuredGrid* grid0;
    const FVizUnstructuredGrid* grid1;
    const FVizDataArray* ghost1;
    const FVizDataArray* value1;

    CHECK(write_piece(piece0, 0.0f, (uint8_t)FVIZ_GHOST_NONE) == 0);
    CHECK(write_piece(piece1, 2.0f, (uint8_t)FVIZ_GHOST_DUPLICATE) == 0);
    file = fopen(manifest, "wb");
    CHECK(file != NULL);
    CHECK(fprintf(file,
        "<?xml version=\"1.0\"?>\n"
        "<VTKFile type=\"PUnstructuredGrid\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
        "  <PUnstructuredGrid GhostLevel=\"1\">\n"
        "    <PPoints><PDataArray type=\"Float32\" NumberOfComponents=\"3\"/></PPoints>\n"
        "    <Piece Source=\"%s\"/>\n"
        "    <Piece Source=\"%s\"/>\n"
        "  </PUnstructuredGrid>\n"
        "</VTKFile>\n", piece0, piece1) > 0);
    CHECK(fclose(file) == 0);

    fviz_pvtu_reader_options_initialize(&options);
    CHECK(options.maximum_pieces >= 2u);
    CHECK(fviz_pvtu_read_with_options(manifest, &options, &data) == FVIZ_OK);
    CHECK(data != NULL);
    CHECK(fviz_partitioned_data_set_count(data) == 2u);
    CHECK(strcmp(fviz_partitioned_data_set_partition_name(data, 0u), piece0) == 0);
    CHECK(strcmp(fviz_partitioned_data_set_partition_name(data, 1u), piece1) == 0);
    grid0 = (const FVizUnstructuredGrid*)fviz_partitioned_data_set_const_partition(data, 0u);
    grid1 = (const FVizUnstructuredGrid*)fviz_partitioned_data_set_const_partition(data, 1u);
    CHECK(grid0 != NULL && grid1 != NULL);
    CHECK(fviz_unstructured_grid_cell_count(grid0) == 1u);
    CHECK(fviz_unstructured_grid_cell_count(grid1) == 1u);
    CHECK(fviz_unstructured_grid_point_count(grid1) == 4u);
    CHECK(fviz_unstructured_grid_bounds(grid1).min.x == 2.0f);
    ghost1 = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid1), FVIZ_GHOST_ARRAY_NAME);
    value1 = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid1), "pieceValue");
    CHECK(ghost1 != NULL && value1 != NULL);
    CHECK(((const uint8_t*)fviz_data_array_const_data(ghost1))[0] == (uint8_t)FVIZ_GHOST_DUPLICATE);
    CHECK(((const float*)fviz_data_array_const_data(value1))[0] == 12.0f);

    {
        FVizPVTUReader* lazy_reader = NULL;
        FVizUnstructuredGrid* lazy_piece = NULL;
        FVizPartitionedDataSet* lazy_all = NULL;
        FVizPVTUCacheStatistics cache_stats;
        CHECK(fviz_pvtu_reader_create(&lazy_reader) == FVIZ_OK);
        CHECK(fviz_pvtu_reader_set_file_name(lazy_reader, manifest) == FVIZ_OK);
        CHECK(fviz_pvtu_reader_piece_count(lazy_reader) == 2u);
        CHECK(fviz_pvtu_reader_ghost_level(lazy_reader) == 1u);
        CHECK(strcmp(fviz_pvtu_reader_piece_source(lazy_reader, 0u), piece0) == 0);
        CHECK(fviz_pvtu_reader_load_piece(lazy_reader, 1u, &lazy_piece) == FVIZ_OK);
        CHECK(lazy_piece != NULL && fviz_unstructured_grid_bounds(lazy_piece).min.x == 2.0f);
        fviz_release(lazy_piece); lazy_piece = NULL;
        cache_stats = fviz_pvtu_reader_cache_statistics(lazy_reader);
        CHECK(cache_stats.misses == 1u && cache_stats.size == 1u);
        CHECK(fviz_pvtu_reader_load_piece(lazy_reader, 1u, &lazy_piece) == FVIZ_OK);
        fviz_release(lazy_piece); lazy_piece = NULL;
        cache_stats = fviz_pvtu_reader_cache_statistics(lazy_reader);
        CHECK(cache_stats.hits == 1u);
        CHECK(fviz_pvtu_reader_set_cache_capacity(lazy_reader, 1u) == FVIZ_OK);
        CHECK(fviz_pvtu_reader_prefetch_piece(lazy_reader, 0u) == FVIZ_OK);
        CHECK(fviz_pvtu_reader_prefetch_piece(lazy_reader, 1u) == FVIZ_OK);
        cache_stats = fviz_pvtu_reader_cache_statistics(lazy_reader);
        CHECK(cache_stats.capacity == 1u && cache_stats.size == 1u && cache_stats.evictions >= 1u);
        CHECK(fviz_pvtu_reader_materialize(lazy_reader, &lazy_all) == FVIZ_OK);
        CHECK(lazy_all != NULL && fviz_partitioned_data_set_count(lazy_all) == 2u);
        fviz_release(lazy_all); lazy_all = NULL;
        /* Byte-budgeted caching keeps large piece streams bounded independently
         * from the entry-count limit. */
        CHECK(fviz_pvtu_reader_set_cache_capacity(lazy_reader, 4u) == FVIZ_OK);
        CHECK(fviz_pvtu_reader_load_piece(lazy_reader, 0u, &lazy_piece) == FVIZ_OK);
        {
            const FVizSize piece_bytes = fviz_data_object_memory_size((const FVizDataObject*)lazy_piece);
            CHECK(piece_bytes > 0u);
            fviz_release(lazy_piece); lazy_piece = NULL;
            fviz_pvtu_reader_clear_cache(lazy_reader);
            CHECK(fviz_pvtu_reader_set_cache_byte_capacity(lazy_reader, piece_bytes) == FVIZ_OK);
            CHECK(fviz_pvtu_reader_prefetch_piece(lazy_reader, 0u) == FVIZ_OK);
            CHECK(fviz_pvtu_reader_prefetch_piece(lazy_reader, 1u) == FVIZ_OK);
            cache_stats = fviz_pvtu_reader_cache_statistics(lazy_reader);
            CHECK(cache_stats.byte_capacity == piece_bytes);
            CHECK(cache_stats.bytes <= piece_bytes);
            CHECK(cache_stats.size == 1u);
            CHECK(cache_stats.evictions >= 1u);
            fviz_pvtu_reader_clear_cache(lazy_reader);
            CHECK(fviz_pvtu_reader_set_cache_byte_capacity(lazy_reader, piece_bytes - 1u) == FVIZ_OK);
            CHECK(fviz_pvtu_reader_prefetch_piece(lazy_reader, 0u) == FVIZ_OK);
            cache_stats = fviz_pvtu_reader_cache_statistics(lazy_reader);
            CHECK(cache_stats.size == 0u);
            CHECK(cache_stats.oversize_skips >= 1u);
        }
        fviz_release(lazy_reader);
    }

    {
        FVizPVTUWriterOptions writer_options;
        FVizPartitionedDataSet* roundtrip = NULL;
        fviz_pvtu_writer_options_initialize(&writer_options);
        writer_options.piece_options.output_mode = FVIZ_VTU_OUTPUT_ASCII;
        CHECK(fviz_pvtu_write("fviz_pvtu_roundtrip.pvtu", data, &writer_options) == FVIZ_OK);
        {
            char manifest_text[4096];
            FILE* manifest_file = fopen("fviz_pvtu_roundtrip.pvtu", "rb");
            size_t bytes;
            CHECK(manifest_file != NULL);
            bytes = fread(manifest_text, 1u, sizeof(manifest_text) - 1u, manifest_file);
            CHECK(fclose(manifest_file) == 0);
            manifest_text[bytes] = '\0';
            CHECK(strstr(manifest_text, "GhostLevel=\"1\"") != NULL);
            CHECK(strstr(manifest_text, "<PCellData") != NULL);
            CHECK(strstr(manifest_text, "Name=\"vtkGhostType\"") != NULL);
        }
        CHECK(fviz_pvtu_read("fviz_pvtu_roundtrip.pvtu", &roundtrip) == FVIZ_OK);
        CHECK(roundtrip != NULL && fviz_partitioned_data_set_count(roundtrip) == 2u);
        CHECK(fviz_object_is_type(
            (const FVizObject*)fviz_partitioned_data_set_const_partition(roundtrip, 1u),
            FVIZ_TYPE_UNSTRUCTURED_GRID));
        ghost1 = fviz_attribute_set_const_get(
            fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)
                fviz_partitioned_data_set_partition(roundtrip, 1u)), FVIZ_GHOST_ARRAY_NAME);
        CHECK(ghost1 != NULL && ((const uint8_t*)fviz_data_array_const_data(ghost1))[0] ==
            (uint8_t)FVIZ_GHOST_DUPLICATE);
        fviz_release(roundtrip);
        {
            FVizDataArray* extra = NULL;
            float extra_value = 1.0f;
            CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &extra) == FVIZ_OK);
            CHECK(fviz_data_array_append_tuple(extra, &extra_value) == FVIZ_OK);
            CHECK(fviz_attribute_set_add(
                fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)
                    fviz_partitioned_data_set_partition(data, 1u)), "extraOnlyOnOnePiece", extra) == FVIZ_OK);
            CHECK(fviz_pvtu_write("fviz_pvtu_bad_schema.pvtu", data, &writer_options) == FVIZ_ERROR_INVALID_ARGUMENT);
            fviz_release(extra);
            (void)remove("fviz_pvtu_bad_schema.pvtu");
            (void)remove("fviz_pvtu_bad_schema_piece00000.vtu");
            (void)remove("fviz_pvtu_bad_schema_piece00001.vtu");
        }
        (void)remove("fviz_pvtu_roundtrip.pvtu");
        (void)remove("fviz_pvtu_roundtrip_piece00000.vtu");
        (void)remove("fviz_pvtu_roundtrip_piece00001.vtu");
    }

    fviz_release(data);
    (void)remove(manifest);
    (void)remove(piece1);
    (void)remove(piece0);
    return 0;
}
