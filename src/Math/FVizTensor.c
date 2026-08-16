#include <math.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizTensor.h>

#include <FViz/Core/FVizErrorInternal.h>

static void fviz_tensor_swap_columns(double matrix[3][3], uint32_t a, uint32_t b)
{
    uint32_t row;
    for (row = 0u; row < 3u; ++row)
    {
        const double value = matrix[row][a];
        matrix[row][a] = matrix[row][b];
        matrix[row][b] = value;
    }
}

FVizResult fviz_symmetric_tensor3d_from_components(const double* components, uint32_t component_count,
                                                   FVizSymmetricTensor3d* out_tensor)
{
    if (components == NULL || out_tensor == NULL || (component_count != 6u && component_count != 9u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "symmetric tensor requires six compact or nine matrix components");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (component_count == 6u)
    {
        out_tensor->xx = components[0];
        out_tensor->yy = components[1];
        out_tensor->zz = components[2];
        out_tensor->xy = components[3];
        out_tensor->yz = components[4];
        out_tensor->xz = components[5];
    }
    else
    {
        out_tensor->xx = components[0];
        out_tensor->yy = components[4];
        out_tensor->zz = components[8];
        out_tensor->xy = 0.5 * (components[1] + components[3]);
        out_tensor->xz = 0.5 * (components[2] + components[6]);
        out_tensor->yz = 0.5 * (components[5] + components[7]);
    }
    return FVIZ_OK;
}

double fviz_symmetric_tensor3d_trace(const FVizSymmetricTensor3d* tensor)
{
    return tensor != NULL ? tensor->xx + tensor->yy + tensor->zz : 0.0;
}

double fviz_symmetric_tensor3d_mean(const FVizSymmetricTensor3d* tensor)
{
    return fviz_symmetric_tensor3d_trace(tensor) / 3.0;
}

FVizResult fviz_symmetric_tensor3d_deviatoric(const FVizSymmetricTensor3d* tensor, FVizSymmetricTensor3d* out_tensor)
{
    double mean;
    if (tensor == NULL || out_tensor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "deviatoric tensor arguments must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    mean = fviz_symmetric_tensor3d_mean(tensor);
    *out_tensor = *tensor;
    out_tensor->xx -= mean;
    out_tensor->yy -= mean;
    out_tensor->zz -= mean;
    return FVIZ_OK;
}

FVizResult fviz_symmetric_tensor3d_eigensystem(const FVizSymmetricTensor3d* tensor, double out_values[3],
                                               double out_vectors[9])
{
    double a[3][3];
    double vectors[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    uint32_t iteration;
    uint32_t i;
    if (tensor == NULL || out_values == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "tensor eigensystem arguments must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    a[0][0] = tensor->xx;
    a[0][1] = tensor->xy;
    a[0][2] = tensor->xz;
    a[1][0] = tensor->xy;
    a[1][1] = tensor->yy;
    a[1][2] = tensor->yz;
    a[2][0] = tensor->xz;
    a[2][1] = tensor->yz;
    a[2][2] = tensor->zz;
    for (iteration = 0u; iteration < 32u; ++iteration)
    {
        uint32_t p = 0u;
        uint32_t q = 1u;
        double maximum = fabs(a[0][1]);
        double scale;
        double tau;
        double tangent;
        double cosine;
        double sine;
        uint32_t k;
        if (fabs(a[0][2]) > maximum)
        {
            p = 0u;
            q = 2u;
            maximum = fabs(a[0][2]);
        }
        if (fabs(a[1][2]) > maximum)
        {
            p = 1u;
            q = 2u;
            maximum = fabs(a[1][2]);
        }
        scale = fabs(a[0][0]) + fabs(a[1][1]) + fabs(a[2][2]) + 1.0;
        if (maximum <= 1.0e-15 * scale) break;
        tau = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
        tangent = (tau >= 0.0 ? 1.0 : -1.0) / (fabs(tau) + sqrt(1.0 + tau * tau));
        cosine = 1.0 / sqrt(1.0 + tangent * tangent);
        sine = tangent * cosine;
        {
            const double app = a[p][p];
            const double aqq = a[q][q];
            const double apq = a[p][q];
            a[p][p] = app - tangent * apq;
            a[q][q] = aqq + tangent * apq;
            a[p][q] = 0.0;
            a[q][p] = 0.0;
        }
        for (k = 0u; k < 3u; ++k)
        {
            if (k != p && k != q)
            {
                const double akp = a[k][p];
                const double akq = a[k][q];
                a[k][p] = cosine * akp - sine * akq;
                a[p][k] = a[k][p];
                a[k][q] = sine * akp + cosine * akq;
                a[q][k] = a[k][q];
            }
            {
                const double vkp = vectors[k][p];
                const double vkq = vectors[k][q];
                vectors[k][p] = cosine * vkp - sine * vkq;
                vectors[k][q] = sine * vkp + cosine * vkq;
            }
        }
    }
    out_values[0] = a[0][0];
    out_values[1] = a[1][1];
    out_values[2] = a[2][2];
    for (i = 0u; i < 2u; ++i)
    {
        uint32_t best = i;
        uint32_t j;
        for (j = i + 1u; j < 3u; ++j)
            if (out_values[j] > out_values[best]) best = j;
        if (best != i)
        {
            const double value = out_values[i];
            out_values[i] = out_values[best];
            out_values[best] = value;
            fviz_tensor_swap_columns(vectors, i, best);
        }
    }
    if (out_vectors != NULL)
    {
        for (i = 0u; i < 3u; ++i)
        {
            uint32_t axis = 0u;
            uint32_t row;
            if (fabs(vectors[1][i]) > fabs(vectors[axis][i])) axis = 1u;
            if (fabs(vectors[2][i]) > fabs(vectors[axis][i])) axis = 2u;
            if (vectors[axis][i] < 0.0)
                for (row = 0u; row < 3u; ++row)
                    vectors[row][i] = -vectors[row][i];
            out_vectors[i * 3u + 0u] = vectors[0][i];
            out_vectors[i * 3u + 1u] = vectors[1][i];
            out_vectors[i * 3u + 2u] = vectors[2][i];
        }
    }
    return FVIZ_OK;
}
