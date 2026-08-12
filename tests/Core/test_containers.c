#include <string.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int g_release_count = 0;
static void release_external(void* data, FVizSize size, void* user_data)
{
    FVIZ_UNUSED(data);
    FVIZ_UNUSED(size);
    FVIZ_UNUSED(user_data);
    ++g_release_count;
}

int main(void)
{
    FVizBuffer* buffer = NULL;
    FVizArray* array = NULL;
    FVizString* string = NULL;
    unsigned char external[4] = {1,2,3,4};
    int i;

    CHECK(fviz_buffer_create(32u, &buffer) == FVIZ_OK);
    CHECK(fviz_buffer_size(buffer) == 32u);
    CHECK(fviz_buffer_resize(buffer, 64u) == FVIZ_OK);
    CHECK(fviz_buffer_size(buffer) == 64u);
    fviz_release(buffer);

    CHECK(fviz_buffer_wrap(external, sizeof(external), release_external, NULL, &buffer) == FVIZ_OK);
    CHECK(fviz_buffer_is_external(buffer) == FVIZ_TRUE);
    CHECK(fviz_buffer_resize(buffer, 8u) == FVIZ_ERROR_INVALID_STATE);
    fviz_release(buffer);
    CHECK(g_release_count == 1);

    CHECK(fviz_array_create(sizeof(int), &array) == FVIZ_OK);
    for (i = 0; i < 100; ++i) CHECK(fviz_array_push(array, &i) == FVIZ_OK);
    CHECK(fviz_array_count(array) == 100u);
    CHECK(*(const int*)fviz_array_const_at(array, 42u) == 42);
    CHECK(fviz_array_resize(array, 150u) == FVIZ_OK);
    CHECK(*(const int*)fviz_array_const_at(array, 120u) == 0);
    fviz_release(array);

    CHECK(fviz_string_create_from("FEA", &string) == FVIZ_OK);
    CHECK(fviz_string_append(string, "Viz") == FVIZ_OK);
    CHECK(strcmp(fviz_string_c_str(string), "FEAViz") == 0);
    CHECK(fviz_string_length(string) == 6u);
    fviz_release(string);
    return 0;
}
