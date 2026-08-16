#include <stdio.h>
#include <FViz/FEA/FVizFEA.h>

int main(void)
{
    static const char* const stress_components[6] = {"S11","S22","S33","S12","S13","S23"};
    const double stress[2][6] = {
        {120.0, 40.0, 10.0, 15.0, 0.0, 0.0},
        {80.0, 75.0, 20.0, 5.0, 2.0, 0.0}
    };
    const uint64_t labels[2] = {101u, 102u};
    FVizFEAResultDatabase* db = NULL;
    FVizFEAStep* step = NULL;
    FVizFEAFrame* frame = NULL;
    FVizFEAField* stress_field = NULL;
    FVizDataArray *values = NULL, *ids = NULL, *mises = NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizFEAFrameInfo frame_info;
    FVizSize i;

    if (fviz_fea_result_database_create(&db) != FVIZ_OK ||
        fviz_fea_step_create("Step-1", "Static load", FVIZ_FEA_STEP_TIME, 1.0, &step) != FVIZ_OK)
        goto fail;

    fviz_fea_frame_info_initialize(&frame_info);
    frame_info.frame_id = 1;
    frame_info.increment_number = 1;
    frame_info.frame_value = 1.0;
    frame_info.description = "End of load step";
    if (fviz_fea_frame_create(&frame_info, &frame) != FVIZ_OK ||
        fviz_fea_field_create("S", "Cauchy stress", FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC, &stress_field) != FVIZ_OK ||
        fviz_fea_field_set_component_labels(stress_field, stress_components, 6u) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64, 6u, &values) != FVIZ_OK ||
        fviz_data_array_append_tuples(values, stress, 2u) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &ids) != FVIZ_OK ||
        fviz_data_array_append_tuples(ids, labels, 2u) != FVIZ_OK)
        goto fail;

    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name = "PART-1-1";
    block.position = FVIZ_FEA_POSITION_CENTROID;
    block.entity_ids = ids;
    block.values = values;
    if (fviz_fea_field_add_block(stress_field, &block, NULL) != FVIZ_OK ||
        fviz_fea_frame_add_field(frame, stress_field) != FVIZ_OK ||
        fviz_fea_step_add_frame(step, frame, NULL) != FVIZ_OK ||
        fviz_fea_result_database_add_step(db, step, NULL) != FVIZ_OK ||
        fviz_fea_field_evaluate_invariant(stress_field, 0u, FVIZ_FEA_INVARIANT_MISES, &mises) != FVIZ_OK)
        goto fail;

    printf("Database: %llu step(s), %llu frame(s), primary field %s / %s\n",
        (unsigned long long)fviz_fea_result_database_step_count(db),
        (unsigned long long)fviz_fea_step_frame_count(step),
        fviz_fea_field_name(stress_field), fviz_fea_invariant_name(FVIZ_FEA_INVARIANT_MISES));
    for (i = 0u; i < fviz_data_array_tuple_count(mises); ++i)
    {
        double value = 0.0;
        (void)fviz_data_array_get_component(mises, i, 0u, &value);
        printf("  element %llu: Mises = %.6f\n", (unsigned long long)labels[i], value);
    }

    fviz_release(mises); fviz_release(ids); fviz_release(values); fviz_release(stress_field);
    fviz_release(frame); fviz_release(step); fviz_release(db);
    return 0;
fail:
    fprintf(stderr, "FEA result model example failed: %s\n", fviz_last_error_message());
    fviz_release(mises); fviz_release(ids); fviz_release(values); fviz_release(stress_field);
    fviz_release(frame); fviz_release(step); fviz_release(db);
    return 1;
}
