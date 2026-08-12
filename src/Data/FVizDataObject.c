#include <FViz/Data/FVizDataObject.h>

#include <FViz/Data/FVizDataObjectPrivate.h>

const FVizObjectClass g_fviz_data_object_class = {
    FVIZ_TYPE_DATA_OBJECT,
    "FVizDataObject",
    &g_fviz_object_class,
    NULL,
    NULL
};

FVizBool fviz_data_object_is_data_object(const FVizDataObject* data_object)
{
    return fviz_object_is_type((const FVizObject*)data_object, FVIZ_TYPE_DATA_OBJECT);
}
