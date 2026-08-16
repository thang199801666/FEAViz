#ifndef FVIZ_MATH_TENSOR_H
#define FVIZ_MATH_TENSOR_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>

FVIZ_EXTERN_C_BEGIN

/* Domain-neutral symmetric 3x3 tensor in compact component order:
 * [xx, yy, zz, xy, yz, xz]. */
typedef struct FVizSymmetricTensor3d
{
    double xx;
    double yy;
    double zz;
    double xy;
    double yz;
    double xz;
} FVizSymmetricTensor3d;

/* Accepts either six compact components or nine row-major matrix components.
 * A full matrix is symmetrized by averaging opposite off-diagonal entries. */
FVIZ_API FVizResult fviz_symmetric_tensor3d_from_components(
    const double* components,
    uint32_t component_count,
    FVizSymmetricTensor3d* out_tensor);
FVIZ_API double fviz_symmetric_tensor3d_trace(const FVizSymmetricTensor3d* tensor);
FVIZ_API double fviz_symmetric_tensor3d_mean(const FVizSymmetricTensor3d* tensor);
FVIZ_API FVizResult fviz_symmetric_tensor3d_deviatoric(
    const FVizSymmetricTensor3d* tensor,
    FVizSymmetricTensor3d* out_tensor);
/* Eigenvalues are returned in descending order. Eigenvectors are optional and,
 * when requested, contain three matching contiguous vectors [v0, v1, v2]. */
FVIZ_API FVizResult fviz_symmetric_tensor3d_eigensystem(
    const FVizSymmetricTensor3d* tensor,
    double out_values[3],
    double out_vectors[9]);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_TENSOR_H */
