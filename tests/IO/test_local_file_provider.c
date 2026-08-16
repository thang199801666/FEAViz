#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    FVizLocalFileProvider* local = NULL;
    FVizDataProviderRequest request;
    FVizDataObject* data = NULL;
    FVizDataProviderStatistics statistics;
    CHECK(fviz_local_file_provider_create(NULL, &local) == FVIZ_OK);
    CHECK(fviz_local_file_provider_format_supported(FVIZ_TESTDATA_DIR "/cube.obj") != FVIZ_FALSE);
    CHECK(fviz_local_file_provider_register(local, 42u, FVIZ_TESTDATA_DIR "/cube.obj") == FVIZ_OK);
    fviz_data_provider_request_initialize(&request);
    request.resource_key = 42u;
    CHECK(fviz_data_provider_fetch(
        fviz_local_file_provider_data_provider(local), &request, &data) == FVIZ_OK);
    CHECK(fviz_object_is_type((FVizObject*)data, FVIZ_TYPE_POLY_DATA) != FVIZ_FALSE);
    fviz_release(data); data = NULL;
    CHECK(fviz_data_provider_fetch(
        fviz_local_file_provider_data_provider(local), &request, &data) == FVIZ_OK);
    fviz_release(data); data = NULL;
    fviz_data_provider_get_statistics(
        fviz_local_file_provider_data_provider(local), &statistics);
    CHECK(statistics.cache_hits == 1u);
    request.pipeline.piece = 1u;
    request.pipeline.number_of_pieces = 2u;
    CHECK(fviz_data_provider_fetch(
        fviz_local_file_provider_data_provider(local), &request, &data) == FVIZ_ERROR_NOT_SUPPORTED);
    CHECK(fviz_local_file_provider_unregister(local, 42u) == FVIZ_OK);
    request.pipeline.piece = 0u;
    request.pipeline.number_of_pieces = 1u;
    CHECK(fviz_data_provider_fetch(
        fviz_local_file_provider_data_provider(local), &request, &data) == FVIZ_ERROR_NOT_FOUND);
    fviz_local_file_provider_destroy(local);
    return 0;
}
