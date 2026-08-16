#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int verify_watertight(const FVizPolyData* mesh)
{
    const uint32_t* triangles = fviz_poly_data_triangle_indices(mesh);
    const FVizSize triangle_count = fviz_poly_data_triangle_count(mesh);
    FVizSize triangle;
    for (triangle = 0u; triangle < triangle_count; ++triangle)
    {
        uint32_t edge;
        for (edge = 0u; edge < 3u; ++edge)
        {
            const uint32_t a = triangles[triangle * 3u + edge];
            const uint32_t b = triangles[triangle * 3u + (edge + 1u) % 3u];
            const uint32_t lo = a < b ? a : b;
            const uint32_t hi = a < b ? b : a;
            FVizSize occurrences = 0u;
            FVizSize other_triangle;
            for (other_triangle = 0u; other_triangle < triangle_count; ++other_triangle)
            {
                uint32_t other_edge;
                for (other_edge = 0u; other_edge < 3u; ++other_edge)
                {
                    const uint32_t c = triangles[other_triangle * 3u + other_edge];
                    const uint32_t d = triangles[other_triangle * 3u + (other_edge + 1u) % 3u];
                    if ((c < d ? c : d) == lo && (c < d ? d : c) == hi) ++occurrences;
                }
            }
            if (occurrences != 2u) return 0;
        }
    }
    return 1;
}

static int verify_clip(FVizClipPolyDataFilter* clip, FVizBool inside_out)
{
    FVizPolyData* output;
    const FVizDataArray* cap;
    const FVizDataArray* provenance;
    const FVizDataArray* scalars;
    const uint32_t* triangles;
    const FVizVec3* points;
    FVizSize i;
    FVizSize cap_count = 0u;
    fviz_clip_poly_data_filter_set_inside_out(clip, inside_out);
    CHECK(fviz_clip_poly_data_filter_update(clip) == FVIZ_OK);
    output = fviz_clip_poly_data_filter_output(clip);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_validate(output) == FVIZ_OK);
    CHECK(verify_watertight(output));
    cap = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "FVizClipCap");
    provenance = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "FVizOriginalCellIds");
    scalars = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output), "X");
    CHECK(cap != NULL && provenance != NULL && scalars != NULL);
    CHECK(fviz_data_array_tuple_count(cap) == fviz_poly_data_triangle_count(output));
    CHECK(fviz_data_array_tuple_count(provenance) == fviz_poly_data_triangle_count(output));
    CHECK(fviz_data_array_tuple_count(scalars) == fviz_poly_data_point_count(output));
    triangles = fviz_poly_data_triangle_indices(output);
    points = fviz_poly_data_points(output);
    for (i = 0u; i < fviz_poly_data_triangle_count(output); ++i)
    {
        double is_cap = 0.0;
        double original_id = 0.0;
        CHECK(fviz_data_array_get_component(cap, i, 0u, &is_cap) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(provenance, i, 0u, &original_id) == FVIZ_OK);
        if (is_cap != 0.0)
        {
            const FVizVec3 a = points[triangles[i * 3u]];
            const FVizVec3 b = points[triangles[i * 3u + 1u]];
            const FVizVec3 c = points[triangles[i * 3u + 2u]];
            const FVizVec3 normal = fviz_vec3_cross(fviz_vec3_sub(b, a), fviz_vec3_sub(c, a));
            CHECK(fabsf(a.x) < 1.0e-6f && fabsf(b.x) < 1.0e-6f && fabsf(c.x) < 1.0e-6f);
            CHECK((inside_out != FVIZ_FALSE && normal.x > 0.0f) ||
                  (inside_out == FVIZ_FALSE && normal.x < 0.0f));
            CHECK(original_id >= 1.0e18);
            ++cap_count;
        }
    }
    CHECK(cap_count >= 2u);
    for (i = 0u; i < fviz_poly_data_point_count(output); ++i)
    {
        double value = 0.0;
        CHECK(fviz_data_array_get_component(scalars, i, 0u, &value) == FVIZ_OK);
        CHECK(fabs(value - points[i].x) < 1.0e-6);
    }
    return 0;
}

int main(void)
{
    static const FVizVec3 points[8] = {
        {-1.0f,-1.0f,-1.0f}, {1.0f,-1.0f,-1.0f}, {1.0f,1.0f,-1.0f}, {-1.0f,1.0f,-1.0f},
        {-1.0f,-1.0f, 1.0f}, {1.0f,-1.0f, 1.0f}, {1.0f,1.0f, 1.0f}, {-1.0f,1.0f, 1.0f}
    };
    static const uint32_t triangles[36] = {
        0u,3u,2u, 0u,2u,1u, 4u,5u,6u, 4u,6u,7u,
        0u,1u,5u, 0u,5u,4u, 3u,7u,6u, 3u,6u,2u,
        0u,4u,7u, 0u,7u,3u, 1u,2u,6u, 1u,6u,5u
    };
    const float values[8] = {-1.0f,1.0f,1.0f,-1.0f,-1.0f,1.0f,1.0f,-1.0f};
    FVizPolyData* cube = NULL;
    FVizDataArray* x = NULL;
    FVizClipPolyDataFilter* clip = NULL;
    CHECK(fviz_poly_data_create(&cube) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(cube, points, 8u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangles(cube, triangles, 12u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &x) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(x, values, 8u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(cube), "X", x) == FVIZ_OK);
    fviz_release(x);
    CHECK(fviz_clip_poly_data_filter_create(&clip) == FVIZ_OK);
    CHECK(fviz_clip_poly_data_filter_generate_cap(clip) == FVIZ_FALSE);
    fviz_clip_poly_data_filter_set_generate_cap(clip, FVIZ_TRUE);
    CHECK(fviz_clip_poly_data_filter_generate_cap(clip) == FVIZ_TRUE);
    fviz_clip_poly_data_filter_set_plane(clip, fviz_plane_from_point_normal(
        fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f)));
    CHECK(fviz_clip_poly_data_filter_set_input_data(clip, cube) == FVIZ_OK);
    CHECK(verify_clip(clip, FVIZ_FALSE) == 0);
    CHECK(verify_clip(clip, FVIZ_TRUE) == 0);
    fviz_release(clip);
    fviz_release(cube);
    return 0;
}
