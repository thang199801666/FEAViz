#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizAttributeSetPrivate.h>
#include <FViz/Data/FVizDataSetPrivate.h>

static void fviz_data_set_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_data_set_class = {
    FVIZ_TYPE_DATA_SET, "FVizDataSet", &g_fviz_object_class, fviz_data_set_destroy
};

static void fviz_data_set_destroy(FVizObject* object)
{
    FVizDataSet* data_set = (FVizDataSet*)object;
    fviz_release(data_set->point_data);
    fviz_release(data_set->cell_data);
    fviz_release(data_set->field_data);
}

FVizResult fviz_data_set_create(FVizDataSet** out_data_set)
{
    FVizDataSet* data_set;
    if (out_data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_data_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_data_set = NULL;
    data_set = (FVizDataSet*)fviz_internal_object_allocate(sizeof(FVizDataSet), &g_fviz_data_set_class, NULL);
    if (data_set == NULL) return fviz_last_error_code();
    if (fviz_attribute_set_create(&data_set->point_data) != FVIZ_OK ||
        fviz_attribute_set_create(&data_set->cell_data) != FVIZ_OK ||
        fviz_attribute_set_create(&data_set->field_data) != FVIZ_OK)
    {
        fviz_release(data_set);
        return fviz_last_error_code();
    }
    *out_data_set = data_set;
    return FVIZ_OK;
}

FVizSize fviz_data_set_point_count(const FVizDataSet* data_set) { return data_set != NULL ? data_set->point_count : 0u; }
FVizSize fviz_data_set_cell_count(const FVizDataSet* data_set) { return data_set != NULL ? data_set->cell_count : 0u; }

static FVizResult fviz_data_set_set_count(FVizDataSet* data_set, FVizSize count, FVizAttributeSet* attributes, FVizSize* destination)
{
    FVizSize i;
    if (data_set == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < fviz_attribute_set_count(attributes); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(attributes, i);
        if (fviz_data_array_tuple_count(array) != 0u && fviz_data_array_tuple_count(array) != count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "dataset count conflicts with attribute tuple count");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    *destination = count;
    return FVIZ_OK;
}

FVizResult fviz_data_set_set_point_count(FVizDataSet* data_set, FVizSize count)
{
    return fviz_data_set_set_count(data_set, count, data_set != NULL ? data_set->point_data : NULL, data_set != NULL ? &data_set->point_count : NULL);
}

FVizResult fviz_data_set_set_cell_count(FVizDataSet* data_set, FVizSize count)
{
    return fviz_data_set_set_count(data_set, count, data_set != NULL ? data_set->cell_data : NULL, data_set != NULL ? &data_set->cell_count : NULL);
}

FVizAttributeSet* fviz_data_set_point_data(FVizDataSet* data_set) { return data_set != NULL ? data_set->point_data : NULL; }
FVizAttributeSet* fviz_data_set_cell_data(FVizDataSet* data_set) { return data_set != NULL ? data_set->cell_data : NULL; }
FVizAttributeSet* fviz_data_set_field_data(FVizDataSet* data_set) { return data_set != NULL ? data_set->field_data : NULL; }

FVizResult fviz_data_set_validate(const FVizDataSet* data_set)
{
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_attribute_set_count(data_set->point_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(data_set->point_data, i);
        if (fviz_data_array_tuple_count(array) != data_set->point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point attribute tuple count does not match dataset");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    for (i = 0u; i < fviz_attribute_set_count(data_set->cell_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(data_set->cell_data, i);
        if (fviz_data_array_tuple_count(array) != data_set->cell_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell attribute tuple count does not match dataset");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    return FVIZ_OK;
}
