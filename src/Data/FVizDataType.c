#include <FViz/Data/FVizDataType.h>

FVizSize fviz_data_type_size(FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            return 1u;
        case FVIZ_DATA_UINT8:
            return 1u;
        case FVIZ_DATA_INT16:
            return 2u;
        case FVIZ_DATA_UINT16:
            return 2u;
        case FVIZ_DATA_INT32:
            return 4u;
        case FVIZ_DATA_UINT32:
            return 4u;
        case FVIZ_DATA_INT64:
            return 8u;
        case FVIZ_DATA_UINT64:
            return 8u;
        case FVIZ_DATA_FLOAT32:
            return 4u;
        case FVIZ_DATA_FLOAT64:
            return 8u;
        default:
            return 0u;
    }
}

const char* fviz_data_type_name(FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            return "int8";
        case FVIZ_DATA_UINT8:
            return "uint8";
        case FVIZ_DATA_INT16:
            return "int16";
        case FVIZ_DATA_UINT16:
            return "uint16";
        case FVIZ_DATA_INT32:
            return "int32";
        case FVIZ_DATA_UINT32:
            return "uint32";
        case FVIZ_DATA_INT64:
            return "int64";
        case FVIZ_DATA_UINT64:
            return "uint64";
        case FVIZ_DATA_FLOAT32:
            return "float32";
        case FVIZ_DATA_FLOAT64:
            return "float64";
        default:
            return "unknown";
    }
}
