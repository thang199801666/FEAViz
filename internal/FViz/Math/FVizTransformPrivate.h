#ifndef FVIZ_INTERNAL_MATH_TRANSFORM_PRIVATE_H
#define FVIZ_INTERNAL_MATH_TRANSFORM_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Math/FVizTransform.h>

struct FVizTransform
{
    FVizObject base;
    FVizMat4 matrix;
};

#endif /* FVIZ_INTERNAL_MATH_TRANSFORM_PRIVATE_H */
