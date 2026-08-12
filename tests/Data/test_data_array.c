#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizDataArray* array = NULL;
    double stress[6] = {1,2,3,4,5,6};
    const double* tuple;
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 6u, &array) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(array, stress) == FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(array) == 1u);
    CHECK(fviz_data_array_components(array) == 6u);
    tuple = (const double*)fviz_data_array_const_tuple(array, 0u);
    CHECK(tuple != NULL && tuple[5] == 6.0);
    fviz_release(array);
    return 0;
}
