#include <stdio.h>
#include <FViz/FViz.h>

int main(void)
{
    FVizArray* array = NULL;
    int value;
    if (fviz_array_create(sizeof(int), &array) != FVIZ_OK) return 1;
    for (value = 0; value < 16; ++value)
    {
        if (fviz_array_push(array, &value) != FVIZ_OK) return 2;
    }
    printf("array count=%zu capacity=%zu\n", (size_t)fviz_array_count(array), (size_t)fviz_array_capacity(array));
    fviz_release(array);
    return 0;
}
