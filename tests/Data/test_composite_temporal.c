#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static FVizPolyData* make_point(float x)
{
    FVizPolyData* poly = NULL;
    if (fviz_poly_data_create(&poly) != FVIZ_OK ||
        fviz_poly_data_add_point(poly, fviz_vec3(x, 0.0f, 0.0f), NULL) != FVIZ_OK)
    {
        fviz_release(poly);
        return NULL;
    }
    return poly;
}

int main(void)
{
    FVizPartitionedDataSet* parts = NULL;
    FVizTemporalDataSet* temporal = NULL;
    FVizPolyData* a = make_point(1.0f);
    FVizPolyData* b = make_point(2.0f);
    FVizPolyData* c = make_point(3.0f);
    FVizSize index = 999u;
    FVizSize lower = 999u, upper = 999u, first = 999u;
    double alpha = -1.0;
    double tmin = 0.0, tmax = 0.0;
    FVizMTime before_mtime, after_mtime;
    CHECK(a != NULL && b != NULL && c != NULL);

    CHECK(fviz_partitioned_data_set_create(&parts) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_reserve(parts, 16u) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts, (FVizDataObject*)a, "Part-A", NULL) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts, (FVizDataObject*)b, "Part-B", NULL) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_count(parts) == 2u);
    CHECK(fviz_partitioned_data_set_partition_name(parts, 1u) != NULL);
    CHECK(fviz_partitioned_data_set_validate(parts) == FVIZ_OK);
    before_mtime = fviz_object_mtime((FVizObject*)parts);
    CHECK(fviz_poly_data_add_point(a, fviz_vec3(1.5f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    after_mtime = fviz_object_mtime((FVizObject*)parts);
    CHECK(after_mtime > before_mtime);
    CHECK(fviz_partitioned_data_set_remove_partition(parts, 0u) == FVIZ_OK);
    before_mtime = fviz_object_mtime((FVizObject*)parts);
    CHECK(fviz_poly_data_add_point(a, fviz_vec3(1.75f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)parts) == before_mtime);

    CHECK(fviz_temporal_data_set_create(&temporal) == FVIZ_OK);
    CHECK(fviz_temporal_data_set_reserve(temporal, 16u) == FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(temporal, 1.0, (FVizDataObject*)b, NULL) == FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(temporal, 0.0, (FVizDataObject*)a, NULL) == FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(temporal, 2.0, (FVizDataObject*)c, NULL) == FVIZ_OK);
    CHECK(fviz_temporal_data_set_step_count(temporal) == 3u);
    CHECK(fabs(fviz_temporal_data_set_time(temporal, 0u) - 0.0) < 1e-12);
    CHECK(fabs(fviz_temporal_data_set_time(temporal, 2u) - 2.0) < 1e-12);
    CHECK(fviz_temporal_data_set_time_range(temporal, &tmin, &tmax) == FVIZ_OK);
    CHECK(tmin == 0.0 && tmax == 2.0);
    CHECK(fviz_temporal_data_set_find_nearest(temporal, 1.6, &index) == FVIZ_OK);
    CHECK(index == 2u);
    CHECK(fviz_temporal_data_set_find_bracket(temporal, 0.25, &lower, &upper, &alpha) == FVIZ_OK);
    CHECK(lower == 0u && upper == 1u && fabs(alpha - 0.25) < 1e-12);
    CHECK(fviz_temporal_data_set_find_bracket(temporal, -1.0, &lower, &upper, &alpha) == FVIZ_OK);
    CHECK(lower == 0u && upper == 0u && alpha == 0.0);
    {
        const double times[] = {3.0, 4.0};
        FVizDataObject* frames[] = {(FVizDataObject*)a, (FVizDataObject*)b};
        CHECK(fviz_temporal_data_set_append_steps(temporal, times, frames, 2u, &first) == FVIZ_OK);
        CHECK(first == 3u && fviz_temporal_data_set_step_count(temporal) == 5u);
    }
    before_mtime = fviz_object_mtime((FVizObject*)temporal);
    CHECK(fviz_poly_data_add_point(c, fviz_vec3(3.5f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)temporal) > before_mtime);
    CHECK(fviz_temporal_data_set_remove_step(temporal, 2u) == FVIZ_OK);
    before_mtime = fviz_object_mtime((FVizObject*)temporal);
    CHECK(fviz_poly_data_add_point(c, fviz_vec3(3.75f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)temporal) == before_mtime);
    CHECK(fviz_temporal_data_set_validate(temporal) == FVIZ_OK);

    fviz_release(temporal);
    fviz_release(parts);
    fviz_release(a);
    fviz_release(b);
    fviz_release(c);
    return 0;
}
