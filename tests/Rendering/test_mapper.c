#include <FViz/FViz.h>
#include <math.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_lookup_table(void)
{
    FVizLookupTable* table = NULL;
    float r, g, b;
    float r0, g0, b0;
    CHECK(fviz_lookup_table_create(256u, &table) == FVIZ_OK);
    CHECK(fviz_lookup_table_size(table) == 256u);
    fviz_lookup_table_set_range(table, -10.0f, 10.0f);
    fviz_lookup_table_get_range(table, &r, &g);
    CHECK(r == -10.0f && g == 10.0f);
    fviz_lookup_table_map_scalar(table, -100.0f, &r, &g, &b);
    fviz_lookup_table_get_color(table, 0u, &r0, &g0, &b0);
    CHECK(r == r0 && g == g0 && b == b0);
    fviz_lookup_table_map_scalar(table, 100.0f, &r, &g, &b);
    fviz_lookup_table_get_color(table, 255u, &r0, &g0, &b0);
    CHECK(r == r0 && g == g0 && b == b0);
    fviz_lookup_table_map_scalar(table, 0.0f, &r, &g, &b);
    CHECK(r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f);
    CHECK(fviz_lookup_table_set_color(table, 0u, 1.0f, 0.0f, 0.0f) == FVIZ_OK);
    fviz_lookup_table_get_color(table, 0u, &r0, &g0, &b0);
    CHECK(r0 == 1.0f && g0 == 0.0f && b0 == 0.0f);
    fviz_lookup_table_set_nan_color(table, 0.25f, 0.5f, 0.75f);
    fviz_lookup_table_map_scalar(table, NAN, &r, &g, &b);
    CHECK(r == 0.25f && g == 0.5f && b == 0.75f);
    fviz_lookup_table_set_below_range_color(table, 0.1f, 0.2f, 0.3f, FVIZ_TRUE);
    fviz_lookup_table_map_scalar(table, -100.0f, &r, &g, &b);
    CHECK(r == 0.1f && g == 0.2f && b == 0.3f);
    fviz_lookup_table_set_above_range_color(table, 0.7f, 0.8f, 0.9f, FVIZ_TRUE);
    fviz_lookup_table_map_scalar(table, 100.0f, &r, &g, &b);
    CHECK(r == 0.7f && g == 0.8f && b == 0.9f);
    fviz_release(table);
    return 0;
}

static int test_rainbow_lookup_table(void)
{
    FVizLookupTable* table = NULL;
    float r, g, b;
    CHECK(fviz_lookup_table_create(257u, &table) == FVIZ_OK);
    CHECK(fviz_lookup_table_build_preset(table, FVIZ_COLOR_MAP_RAINBOW) == FVIZ_OK);
    fviz_lookup_table_set_range(table, 0.0f, 100.0f);
    fviz_lookup_table_map_scalar(table, 0.0f, &r, &g, &b);
    CHECK(r == 0.0f && g == 0.0f && b == 1.0f);
    fviz_lookup_table_map_scalar(table, 50.0f, &r, &g, &b);
    CHECK(r == 0.0f && g == 1.0f && b == 0.0f);
    fviz_lookup_table_map_scalar(table, 100.0f, &r, &g, &b);
    CHECK(r == 1.0f && g == 0.0f && b == 0.0f);
    CHECK(fviz_lookup_table_build_preset(table, (FVizColorMapPreset)99) == FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(table);
    return 0;
}

static int test_mapper(void)
{
    FVizMapper* mapper = NULL;
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizActor* actor = NULL;
    FVizLookupTable* table = NULL;
    float values[3] = {0.0f, 5.0f, 10.0f};
    FVizSize i;
    uint32_t a, b, c;

    CHECK(fviz_mapper_create(&mapper) == FVIZ_OK);
    CHECK(fviz_mapper_scalar_visibility(mapper) == FVIZ_FALSE);
    CHECK(fviz_mapper_scalar_range_valid(mapper) == FVIZ_FALSE);

    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0, 0, 0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1, 0, 0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0, 1, 0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, b, c) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_resize(scalars, 3u) == FVIZ_OK);
    for (i = 0u; i < 3u; ++i)
    {
        CHECK(fviz_data_array_set_tuple(scalars, i, &values[i]) == FVIZ_OK);
    }
    CHECK(fviz_poly_data_set_scalars(data, scalars) == FVIZ_OK);
    CHECK(fviz_poly_data_const_scalars(data) == scalars);

    CHECK(fviz_mapper_set_poly_data(mapper, data) == FVIZ_OK);
    CHECK(fviz_mapper_const_poly_data(mapper) == data);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    CHECK(fviz_mapper_scalar_visibility(mapper) == FVIZ_TRUE);
    fviz_mapper_set_scalar_range(mapper, 0.0f, 10.0f);
    CHECK(fviz_mapper_scalar_range_valid(mapper) == FVIZ_TRUE);
    fviz_mapper_get_scalar_range(mapper, &values[0], &values[1]);
    CHECK(values[0] == 0.0f && values[1] == 10.0f);
    CHECK(fviz_mapper_lookup_table(mapper) != NULL);
    table = fviz_mapper_lookup_table(mapper);
    fviz_lookup_table_map_scalar(table, 5.0f, &values[0], &values[1], &values[2]);
    CHECK(values[0] >= 0.0f && values[0] <= 1.0f);

    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(actor, data) == FVIZ_OK);
    CHECK(fviz_actor_const_poly_data(actor) == data);
    CHECK(fviz_actor_mapper(actor) != NULL);
    CHECK(fviz_actor_set_mapper(actor, mapper) == FVIZ_OK);
    CHECK(fviz_actor_mapper(actor) == mapper);
    CHECK(fviz_actor_const_poly_data(actor) == data);

    fviz_release(actor);
    fviz_release(mapper);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}

int main(void)
{
    CHECK(test_lookup_table() == 0);
    CHECK(test_rainbow_lookup_table() == 0);
    CHECK(test_mapper() == 0);
    return 0;
}
