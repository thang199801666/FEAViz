#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static void release_external(void* data, void* user_data)
{
    int* releases = (int*)user_data;
    (void)data;
    ++*releases;
}

int main(void)
{
    FVizDataArray* array = NULL;
    FVizDataArray* copy = NULL;
    FVizDataArray* dirty = NULL;
    double stress[6] = {1,2,3,4,5,6};
    const double* tuple;
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    FVizMTime mtime;
    FVizDirtyRange dirty_range;
    {
        float backing[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        FVizDataArray* immutable = NULL;
        FVizDataArray* mutable_view = NULL;
        FVizDataArray* detached = NULL;
        float replacement = 8.0f;
        int releases = 0;
        CHECK(fviz_data_array_create_external(
            FVIZ_DATA_FLOAT32, 1u, backing, 4u,
            FVIZ_DATA_ARRAY_EXTERNAL_IMMUTABLE, release_external, &releases,
            &immutable) == FVIZ_OK);
        CHECK(fviz_data_array_is_external(immutable) != FVIZ_FALSE);
        CHECK(fviz_data_array_is_mutable(immutable) == FVIZ_FALSE);
        CHECK(fviz_data_array_const_data(immutable) == backing);
        CHECK(fviz_data_array_data(immutable) == NULL);
        CHECK(fviz_data_array_tuple(immutable, 0u) == NULL);
        CHECK(fviz_data_array_set_tuple(immutable, 0u, &replacement) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_data_array_set_component(immutable, 0u, 0u, 8.0) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_data_array_mark_dirty(immutable, 0u, 1u) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_data_array_resize(immutable, 3u) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_data_array_append_tuple(immutable, &replacement) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_data_array_deep_copy(immutable, &detached) == FVIZ_OK);
        CHECK(fviz_data_array_is_external(detached) == FVIZ_FALSE);
        fviz_release(immutable);
        CHECK(releases == 1);
        CHECK(((const float*)fviz_data_array_const_data(detached))[2] == 3.0f);
        fviz_release(detached);

        CHECK(fviz_data_array_create_external(
            FVIZ_DATA_FLOAT32, 1u, backing, 4u,
            FVIZ_DATA_ARRAY_EXTERNAL_MUTABLE, release_external, &releases,
            &mutable_view) == FVIZ_OK);
        CHECK(fviz_data_array_data(mutable_view) == backing);
        mtime = fviz_object_mtime((FVizObject*)mutable_view);
        CHECK(fviz_data_array_set_tuple(mutable_view, 2u, &replacement) == FVIZ_OK);
        CHECK(backing[2] == 8.0f);
        CHECK(fviz_data_array_dirty_range_since(mutable_view, mtime, &dirty_range) == FVIZ_OK);
        CHECK(dirty_range.full == FVIZ_FALSE && dirty_range.first == 2u && dirty_range.count == 1u);
        CHECK(fviz_data_array_resize(mutable_view, 4u) == FVIZ_OK);
        CHECK(fviz_data_array_reserve(mutable_view, 4u) == FVIZ_OK);
        CHECK(fviz_data_array_reserve(mutable_view, 5u) == FVIZ_ERROR_INVALID_STATE);
        fviz_release(mutable_view);
        CHECK(releases == 2);
    }
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 6u, &array) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(array, stress) == FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(array) == 1u);
    CHECK(fviz_data_array_components(array) == 6u);
    tuple = (const double*)fviz_data_array_const_tuple(array, 0u);
    CHECK(tuple != NULL && tuple[5] == 6.0);
    CHECK(fviz_data_array_get_component(array, 0u, 2u, &value) == FVIZ_OK);
    CHECK(value == 3.0);
    CHECK(fviz_data_array_set_component(array, 0u, 2u, 9.0) == FVIZ_OK);
    CHECK(fviz_data_array_get_component(array, 0u, 2u, &value) == FVIZ_OK && value == 9.0);
    {
        const double updated_tuple[6] = {1.0, 2.0, 9.0, 4.0, 5.0, 6.0};
        CHECK(fviz_data_array_set_tuples(array, 0u, updated_tuple, 1u) == FVIZ_OK);
        CHECK(fviz_data_array_set_tuples(array, 1u, updated_tuple, 1u) == FVIZ_ERROR_INVALID_ARGUMENT);
    }
    mtime = fviz_object_mtime((FVizObject*)array);
    CHECK(fviz_data_array_set_component(array, 0u, 2u, 9.0) == FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)array) == mtime);
    {
        const double same_tuple[6] = {1.0, 2.0, 9.0, 4.0, 5.0, 6.0};
        CHECK(fviz_data_array_set_tuple(array, 0u, same_tuple) == FVIZ_OK);
        CHECK(fviz_object_mtime((FVizObject*)array) == mtime);
    }
    CHECK(fviz_data_array_get_range(array, 2, FVIZ_TRUE, &minimum, &maximum) == FVIZ_OK);
    CHECK(minimum == 9.0 && maximum == 9.0);
    /* Repeating the same query may use the MTime-keyed cache. */
    CHECK(fviz_data_array_get_range(array, 2, FVIZ_TRUE, &minimum, &maximum) == FVIZ_OK);
    CHECK(minimum == 9.0 && maximum == 9.0);
    CHECK(fviz_data_array_set_component(array, 0u, 2u, 8.0) == FVIZ_OK);
    CHECK(fviz_data_array_get_range(array, 2, FVIZ_TRUE, &minimum, &maximum) == FVIZ_OK);
    CHECK(minimum == 8.0 && maximum == 8.0);
    ((double*)fviz_data_array_data(array))[2] = 7.0;
    fviz_object_modified((FVizObject*)array);
    CHECK(fviz_data_array_get_range(array, 2, FVIZ_TRUE, &minimum, &maximum) == FVIZ_OK);
    CHECK(minimum == 7.0 && maximum == 7.0);
    CHECK(fviz_data_array_deep_copy(array, &copy) == FVIZ_OK);
    CHECK(copy != array);
    CHECK(fviz_data_array_const_data(copy) != fviz_data_array_const_data(array));
    CHECK(fviz_data_array_get_component(copy, 0u, 2u, &value) == FVIZ_OK && value == 7.0);
    {
        const float initial[4] = {0.0f, 1.0f, 2.0f, 3.0f};
        const float changed = 9.0f;
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &dirty) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(dirty, initial, 4u) == FVIZ_OK);
        mtime = fviz_object_mtime((FVizObject*)dirty);
        CHECK(fviz_data_array_set_tuple(dirty, 2u, &changed) == FVIZ_OK);
        CHECK(fviz_data_array_dirty_range_since(dirty, mtime, &dirty_range) == FVIZ_OK);
        CHECK(dirty_range.full == FVIZ_FALSE && dirty_range.first == 2u && dirty_range.count == 1u);
        mtime = fviz_object_mtime((FVizObject*)dirty);
        ((float*)fviz_data_array_data(dirty))[1] = 8.0f;
        CHECK(fviz_data_array_mark_dirty(dirty, 1u, 1u) == FVIZ_OK);
        CHECK(fviz_data_array_dirty_range_since(dirty, mtime, &dirty_range) == FVIZ_OK);
        CHECK(dirty_range.full == FVIZ_FALSE && dirty_range.first == 1u && dirty_range.count == 1u);
        mtime = fviz_object_mtime((FVizObject*)dirty);
        fviz_object_modified((FVizObject*)dirty);
        CHECK(fviz_data_array_dirty_range_since(dirty, mtime, &dirty_range) == FVIZ_OK);
        CHECK(dirty_range.full != FVIZ_FALSE && dirty_range.count == 4u);
    }
    {
        FVizDataArrayTupleIterator it;
        FVizDataArrayMutableIterator mit;
        FVizSize count = 0u;
        double sum = 0.0;
        CHECK(fviz_data_array_iter_begin(array, &it) == FVIZ_OK);
        for (; fviz_data_array_iter_valid(&it); fviz_data_array_iter_next(&it))
        {
            const double* t = (const double*)fviz_data_array_iter_tuple(&it);
            CHECK(t != NULL);
            CHECK(fviz_data_array_iter_index(&it) == count);
            sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5];
            ++count;
        }
        CHECK(count == fviz_data_array_tuple_count(array));
        CHECK(sum == 25.0);
        CHECK(fviz_data_array_iter_next(&it) == FVIZ_FALSE);
        CHECK(fviz_data_array_iter_valid(&it) == FVIZ_FALSE);
        CHECK(fviz_data_array_iter_tuple(&it) == NULL);

        CHECK(fviz_data_array_mut_iter_begin(dirty, &mit) == FVIZ_OK);
        {
            float* t = (float*)fviz_data_array_mut_iter_tuple(&mit);
            CHECK(t != NULL);
            t[0] = 42.0f;
            CHECK(fviz_data_array_mut_iter_index(&mit) == 0u);
        }
        CHECK(fviz_data_array_mut_iter_next(&mit) != FVIZ_FALSE);
        CHECK(fviz_data_array_mut_iter_valid(&mit) != FVIZ_FALSE);
        CHECK(fviz_data_array_mark_dirty(dirty, 0u, 1u) == FVIZ_OK);
        CHECK(((const float*)fviz_data_array_const_tuple(dirty, 0u))[0] == 42.0f);

        {
            float ro_backing[2] = {5.0f, 6.0f};
            FVizDataArray* read_only = NULL;
            CHECK(fviz_data_array_create_external(FVIZ_DATA_FLOAT32, 1u, ro_backing, 2u,
                                                  FVIZ_DATA_ARRAY_EXTERNAL_IMMUTABLE, NULL, NULL,
                                                  &read_only) == FVIZ_OK);
            CHECK(fviz_data_array_mut_iter_begin(read_only, &mit) == FVIZ_ERROR_INVALID_STATE);
            CHECK(fviz_data_array_iter_begin(read_only, &it) == FVIZ_OK);
            CHECK(((const float*)fviz_data_array_iter_tuple(&it))[0] == 5.0f);
            fviz_release(read_only);
        }
    }
    fviz_release(dirty);
    fviz_release(copy);
    fviz_release(array);
    return 0;
}
