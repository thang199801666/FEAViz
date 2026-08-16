#include <stdint.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizDataArray* identity = NULL;
    FVizDataArray* local = NULL;
    FVizDataArray* composed = NULL;
    FVizAttributeSet* attributes = NULL;
    const uint32_t local_values[3] = {3u, 1u, 0u};
    FVizId id = FVIZ_INVALID_ID;
    FVizBool persistent = FVIZ_FALSE;
    FVizSize local_id = SIZE_MAX;

    CHECK(fviz_provenance_create_identity(4u, &identity) == FVIZ_OK);
    CHECK(fviz_provenance_validate(identity, 4u) == FVIZ_OK);
    CHECK(fviz_provenance_validate(identity, 3u) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &local) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(local, local_values, 3u) == FVIZ_OK);
    CHECK(fviz_provenance_compose(identity, local, &composed) == FVIZ_OK);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(composed, 0u) == 3u);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(composed, 1u) == 1u);

    CHECK(fviz_attribute_set_create(&attributes) == FVIZ_OK);
    CHECK(fviz_provenance_resolve(
        attributes, FVIZ_PROVENANCE_POINT, 2u, 99u, &id, &persistent) == FVIZ_OK);
    CHECK(id == 99u && persistent == FVIZ_FALSE);
    CHECK(fviz_attribute_set_add(
        attributes, FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME, composed) == FVIZ_OK);
    CHECK(fviz_provenance_resolve(
        attributes, FVIZ_PROVENANCE_POINT, 1u, 99u, &id, &persistent) == FVIZ_OK);
    CHECK(id == 1u && persistent == FVIZ_TRUE);
    CHECK(fviz_provenance_find(
        attributes, FVIZ_PROVENANCE_POINT, 3u, &local_id) == FVIZ_OK);
    CHECK(local_id == 0u);
    CHECK(fviz_provenance_find(
        attributes, FVIZ_PROVENANCE_POINT, 42u, &local_id) == FVIZ_ERROR_NOT_FOUND);

    fviz_release(attributes);
    fviz_release(composed);
    fviz_release(local);
    fviz_release(identity);
    return 0;
}
