#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/FEA/FVizIntegrationPointData.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_IP_MAX_NODES 20u
#define FVIZ_IP_MAX_POINTS 27u

void fviz_integration_point_extrapolation_options_initialize(FVizIntegrationPointExtrapolationOptions* options)
{
    if (options == NULL) return;
    options->struct_size = (uint32_t)sizeof(*options);
    options->fallback_policy = FVIZ_INTEGRATION_POINT_CELL_MEAN;
}

static FVizResult fviz_ip_copy_standard(const FVizVec3* source, FVizSize count, FVizVec3* out, FVizSize capacity,
                                        FVizSize* out_count)
{
    if (out_count != NULL) *out_count = count;
    if (out == NULL)
    {
        if (capacity == 0u) return FVIZ_OK;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "integration coordinate output is NULL with non-zero capacity");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "integration coordinate output capacity is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memcpy(out, source, count * sizeof(*out));
    return FVIZ_OK;
}

FVizResult fviz_integration_point_standard_coordinates(FVizCellType type, FVizSize count, FVizVec3* out,
                                                       FVizSize capacity, FVizSize* out_count)
{
    const float g = (float)(1.0 / sqrt(3.0));
    if (out_count != NULL) *out_count = 0u;
    if (count == 1u)
    {
        FVizVec3 p = fviz_vec3(0, 0, 0);
        if (type == FVIZ_CELL_TRIANGLE || type == FVIZ_CELL_QUADRATIC_TRIANGLE)
            p = fviz_vec3(1.0f / 3.0f, 1.0f / 3.0f, 0);
        else if (type == FVIZ_CELL_TETRA || type == FVIZ_CELL_QUADRATIC_TETRA)
            p = fviz_vec3(0.25f, 0.25f, 0.25f);
        else if (type != FVIZ_CELL_LINE && type != FVIZ_CELL_QUADRATIC_EDGE && type != FVIZ_CELL_QUAD &&
                 type != FVIZ_CELL_QUADRATIC_QUAD && type != FVIZ_CELL_BIQUADRATIC_QUAD &&
                 type != FVIZ_CELL_HEXAHEDRON && type != FVIZ_CELL_QUADRATIC_HEXAHEDRON && type != FVIZ_CELL_WEDGE)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "standard one-point integration layout is not available for this cell type");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        return fviz_ip_copy_standard(&p, 1u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_LINE || type == FVIZ_CELL_QUADRATIC_EDGE) && count == 2u)
    {
        const FVizVec3 p[2] = {{-g, 0, 0}, {g, 0, 0}};
        return fviz_ip_copy_standard(p, 2u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_TRIANGLE || type == FVIZ_CELL_QUADRATIC_TRIANGLE) && count == 3u)
    {
        const FVizVec3 p[3] = {{1.0f / 6.0f, 1.0f / 6.0f, 0},
                               {2.0f / 3.0f, 1.0f / 6.0f, 0},
                               {1.0f / 6.0f, 2.0f / 3.0f, 0}};
        return fviz_ip_copy_standard(p, 3u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_QUAD || type == FVIZ_CELL_QUADRATIC_QUAD || type == FVIZ_CELL_BIQUADRATIC_QUAD) &&
        count == 4u)
    {
        const FVizVec3 p[4] = {{-g, -g, 0}, {g, -g, 0}, {g, g, 0}, {-g, g, 0}};
        return fviz_ip_copy_standard(p, 4u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_TETRA || type == FVIZ_CELL_QUADRATIC_TETRA) && count == 4u)
    {
        const float a = 0.5854101966249685f, b = 0.1381966011250105f;
        const FVizVec3 p[4] = {{b, b, b}, {a, b, b}, {b, a, b}, {b, b, a}};
        return fviz_ip_copy_standard(p, 4u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_HEXAHEDRON || type == FVIZ_CELL_QUADRATIC_HEXAHEDRON) && count == 8u)
    {
        FVizVec3 p[8];
        FVizSize i = 0u;
        int x, y, z;
        for (z = -1; z <= 1; z += 2)
            for (y = -1; y <= 1; y += 2)
                for (x = -1; x <= 1; x += 2)
                    p[i++] = fviz_vec3((float)x * g, (float)y * g, (float)z * g);
        return fviz_ip_copy_standard(p, 8u, out, capacity, out_count);
    }
    if ((type == FVIZ_CELL_HEXAHEDRON || type == FVIZ_CELL_QUADRATIC_HEXAHEDRON) && count == 27u)
    {
        const float h = (float)sqrt(3.0 / 5.0);
        const float a[3] = {-h, 0.0f, h};
        FVizVec3 p[27];
        FVizSize i = 0u;
        int x, y, z;
        for (z = 0; z < 3; ++z)
            for (y = 0; y < 3; ++y)
                for (x = 0; x < 3; ++x)
                    p[i++] = fviz_vec3(a[x], a[y], a[z]);
        return fviz_ip_copy_standard(p, 27u, out, capacity, out_count);
    }
    if (type == FVIZ_CELL_WEDGE && count == 6u)
    {
        const FVizVec3 p[6] = {{1.0f / 6.0f, 1.0f / 6.0f, -g}, {2.0f / 3.0f, 1.0f / 6.0f, -g},
                               {1.0f / 6.0f, 2.0f / 3.0f, -g}, {1.0f / 6.0f, 1.0f / 6.0f, g},
                               {2.0f / 3.0f, 1.0f / 6.0f, g},  {1.0f / 6.0f, 2.0f / 3.0f, g}};
        return fviz_ip_copy_standard(p, 6u, out, capacity, out_count);
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                            "standard integration layout is not available for this cell type/count");
    return FVIZ_ERROR_NOT_SUPPORTED;
}

static FVizBool fviz_ip_solve(FVizSize n, double a[FVIZ_IP_MAX_NODES][FVIZ_IP_MAX_NODES], double b[FVIZ_IP_MAX_NODES],
                              double x[FVIZ_IP_MAX_NODES])
{
    FVizSize i, j, k, pivot;
    for (i = 0u; i < n; ++i)
    {
        double best = fabs(a[i][i]);
        pivot = i;
        for (j = i + 1u; j < n; ++j)
            if (fabs(a[j][i]) > best)
            {
                best = fabs(a[j][i]);
                pivot = j;
            }
        if (best < 1.0e-13) return FVIZ_FALSE;
        if (pivot != i)
        {
            for (k = i; k < n; ++k)
            {
                const double t = a[i][k];
                a[i][k] = a[pivot][k];
                a[pivot][k] = t;
            }
            {
                const double t = b[i];
                b[i] = b[pivot];
                b[pivot] = t;
            }
        }
        {
            const double d = a[i][i];
            for (k = i; k < n; ++k)
                a[i][k] /= d;
            b[i] /= d;
        }
        for (j = 0u; j < n; ++j)
        {
            double f;
            if (j == i) continue;
            f = a[j][i];
            if (fabs(f) < 1.0e-30) continue;
            for (k = i; k < n; ++k)
                a[j][k] -= f * a[i][k];
            b[j] -= f * b[i];
        }
    }
    for (i = 0u; i < n; ++i)
        x[i] = b[i];
    return FVIZ_TRUE;
}

static FVizBool fviz_ip_extrapolate_component(double shape[FVIZ_IP_MAX_POINTS][FVIZ_IP_MAX_NODES], FVizSize m,
                                              FVizSize n, const FVizDataArray* values, FVizSize begin,
                                              uint32_t component, FVizIntegrationPointFallbackPolicy fallback,
                                              double out_nodes[FVIZ_IP_MAX_NODES])
{
    FVizSize i, j, k;
    if (m < n)
    {
        if (fallback == FVIZ_INTEGRATION_POINT_FAIL) return FVIZ_FALSE;
        {
            double mean = 0.0, v = 0.0;
            for (i = 0u; i < m; ++i)
            {
                if (fviz_data_array_get_component(values, begin + i, component, &v) != FVIZ_OK) return FVIZ_FALSE;
                mean += v;
            }
            mean = m != 0u ? mean / (double)m : 0.0;
            for (i = 0u; i < n; ++i)
                out_nodes[i] = mean;
            return FVIZ_TRUE;
        }
    }
    {
        double ata[FVIZ_IP_MAX_NODES][FVIZ_IP_MAX_NODES];
        double atb[FVIZ_IP_MAX_NODES], solution[FVIZ_IP_MAX_NODES];
        (void)memset(ata, 0, sizeof(ata));
        (void)memset(atb, 0, sizeof(atb));
        for (i = 0u; i < m; ++i)
        {
            double v = 0.0;
            if (fviz_data_array_get_component(values, begin + i, component, &v) != FVIZ_OK) return FVIZ_FALSE;
            for (j = 0u; j < n; ++j)
            {
                atb[j] += shape[i][j] * v;
                for (k = 0u; k < n; ++k)
                    ata[j][k] += shape[i][j] * shape[i][k];
            }
        }
        if (fviz_ip_solve(n, ata, atb, solution) == FVIZ_FALSE)
        {
            if (fallback == FVIZ_INTEGRATION_POINT_FAIL) return FVIZ_FALSE;
            {
                double mean = 0.0, v = 0.0;
                for (i = 0u; i < m; ++i)
                {
                    if (fviz_data_array_get_component(values, begin + i, component, &v) != FVIZ_OK) return FVIZ_FALSE;
                    mean += v;
                }
                mean = m != 0u ? mean / (double)m : 0.0;
                for (i = 0u; i < n; ++i)
                    out_nodes[i] = mean;
                return FVIZ_TRUE;
            }
        }
        for (i = 0u; i < n; ++i)
            out_nodes[i] = solution[i];
    }
    return FVIZ_TRUE;
}

FVizResult fviz_unstructured_grid_extrapolate_integration_point_data(
    const FVizUnstructuredGrid* grid, const FVizDataArray* values, const FVizSize* offsets, const FVizVec3* parametric,
    const FVizIntegrationPointExtrapolationOptions* user_options, FVizDataArray** out_values)
{
    FVizIntegrationPointExtrapolationOptions defaults;
    const FVizIntegrationPointExtrapolationOptions* options = user_options;
    FVizDataArray* output = NULL;
    double* sums = NULL;
    uint32_t* counts = NULL;
    FVizSize point_count, cell_count, total_values, components, sums_count, sums_bytes, count_bytes;
    FVizSize cell_id;
    if (out_values == NULL || grid == NULL || values == NULL || offsets == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "integration extrapolation requires grid, values, offsets and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_values = NULL;
    if (options == NULL)
    {
        fviz_integration_point_extrapolation_options_initialize(&defaults);
        options = &defaults;
    }
    else if (options->struct_size < sizeof(FVizIntegrationPointExtrapolationOptions))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "integration-point extrapolation options struct is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (options->fallback_policy != FVIZ_INTEGRATION_POINT_FAIL &&
        options->fallback_policy != FVIZ_INTEGRATION_POINT_CELL_MEAN)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid integration-point fallback policy");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    point_count = fviz_unstructured_grid_point_count(grid);
    cell_count = fviz_unstructured_grid_cell_count(grid);
    total_values = fviz_data_array_tuple_count(values);
    components = (FVizSize)fviz_data_array_components(values);
    if (components == 0u || offsets[0] != 0u || offsets[cell_count] != total_values)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "integration offsets do not match the value array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
        if (offsets[cell_id + 1u] < offsets[cell_id])
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "integration offsets must be nondecreasing");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    if (fviz_size_multiply(point_count, components, &sums_count) != FVIZ_OK ||
        fviz_size_multiply(sums_count, sizeof(double), &sums_bytes) != FVIZ_OK ||
        fviz_size_multiply(point_count, sizeof(uint32_t), &count_bytes) != FVIZ_OK)
        return fviz_last_error_code();
    if (sums_count != 0u)
    {
        sums = (double*)fviz_alloc(sums_bytes);
        if (sums == NULL) goto fail;
        (void)memset(sums, 0, sums_bytes);
    }
    if (point_count != 0u)
    {
        counts = (uint32_t*)fviz_alloc(count_bytes);
        if (counts == NULL) goto fail;
        (void)memset(counts, 0, count_bytes);
    }
    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
    {
        FVizCellView view;
        FVizVec3 standard[FVIZ_IP_MAX_POINTS];
        const FVizVec3* coords = parametric != NULL ? parametric + offsets[cell_id] : standard;
        const FVizSize begin = offsets[cell_id], m = offsets[cell_id + 1u] - begin;
        FVizSize standard_count = 0u, n, i, c;
        double shape[FVIZ_IP_MAX_POINTS][FVIZ_IP_MAX_NODES];
        if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell_id, &view) !=
            FVIZ_OK)
            goto fail;
        n = view.point_count;
        if (n == 0u || n > FVIZ_IP_MAX_NODES || m == 0u || m > FVIZ_IP_MAX_POINTS)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "integration extrapolation cell/IP count exceeds supported limits");
            goto fail;
        }
        if (parametric == NULL && fviz_integration_point_standard_coordinates(
                                      view.type, m, standard, FVIZ_IP_MAX_POINTS, &standard_count) != FVIZ_OK)
            goto fail;
        if (parametric == NULL && standard_count != m) goto fail;
        for (i = 0u; i < m; ++i)
        {
            FVizSize wc = 0u;
            if (fviz_cell_type_shape_weights(view.type, coords[i], shape[i], FVIZ_IP_MAX_NODES, &wc) != FVIZ_OK ||
                wc != n)
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                        "integration extrapolation lacks compatible cell shape weights");
                goto fail;
            }
        }
        for (c = 0u; c < components; ++c)
        {
            double local[FVIZ_IP_MAX_NODES];
            if (fviz_ip_extrapolate_component(shape, m, n, values, begin, (uint32_t)c, options->fallback_policy,
                                              local) == FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                        "integration-point extrapolation system is singular/underdetermined");
                goto fail;
            }
            for (i = 0u; i < n; ++i)
            {
                const FVizId id = fviz_cell_view_point_id(&view, i);
                if (id >= (FVizId)point_count)
                {
                    fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "integration cell references an invalid point");
                    goto fail;
                }
                sums[(FVizSize)id * components + c] += local[i];
            }
        }
        for (i = 0u; i < n; ++i)
        {
            const FVizId id = fviz_cell_view_point_id(&view, i);
            if (counts[(FVizSize)id] != UINT32_MAX) ++counts[(FVizSize)id];
        }
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, (uint32_t)components, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, point_count) != FVIZ_OK)
        goto fail;
    {
        double* dst = (double*)fviz_data_array_data(output);
        FVizSize i, c;
        for (i = 0u; i < point_count; ++i)
            for (c = 0u; c < components; ++c)
                dst[i * components + c] = counts[i] != 0u ? sums[i * components + c] / (double)counts[i] : 0.0;
    }
    fviz_free(sums);
    fviz_free(counts);
    *out_values = output;
    return FVIZ_OK;
fail:
    fviz_free(sums);
    fviz_free(counts);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_extrapolate_integration_point_data_element_nodal(
    const FVizUnstructuredGrid* grid, const FVizDataArray* values, const FVizSize* offsets, const FVizVec3* parametric,
    const FVizIntegrationPointExtrapolationOptions* user_options, FVizDataArray** out_values)
{
    FVizIntegrationPointExtrapolationOptions defaults;
    const FVizIntegrationPointExtrapolationOptions* options = user_options;
    FVizDataArray* output = NULL;
    FVizSize cell_count, total_values, components, output_tuples = 0u, cell_id, write_tuple = 0u;
    if (out_values == NULL || grid == NULL || values == NULL || offsets == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "element-nodal integration extrapolation requires grid, values, offsets and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_values = NULL;
    if (options == NULL)
    {
        fviz_integration_point_extrapolation_options_initialize(&defaults);
        options = &defaults;
    }
    else if (options->struct_size < sizeof(FVizIntegrationPointExtrapolationOptions))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "integration-point extrapolation options struct is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (options->fallback_policy != FVIZ_INTEGRATION_POINT_FAIL &&
        options->fallback_policy != FVIZ_INTEGRATION_POINT_CELL_MEAN)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid integration-point fallback policy");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    cell_count = fviz_unstructured_grid_cell_count(grid);
    total_values = fviz_data_array_tuple_count(values);
    components = (FVizSize)fviz_data_array_components(values);
    if (components == 0u || offsets[0] != 0u || offsets[cell_count] != total_values)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "integration offsets do not match the value array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
    {
        const FVizSize n =
            fviz_cell_array_point_count(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell_id);
        if (offsets[cell_id + 1u] < offsets[cell_id] || n > (FVizSize)-1 - output_tuples)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "invalid integration offsets or element-node tuple count overflow");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        output_tuples += n;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, (uint32_t)components, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, output_tuples) != FVIZ_OK)
        goto fail;
    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
    {
        FVizCellView view;
        FVizVec3 standard[FVIZ_IP_MAX_POINTS];
        const FVizSize begin = offsets[cell_id];
        const FVizSize m = offsets[cell_id + 1u] - begin;
        FVizSize standard_count = 0u, n, i, c;
        double shape[FVIZ_IP_MAX_POINTS][FVIZ_IP_MAX_NODES];
        if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell_id, &view) !=
            FVIZ_OK)
            goto fail;
        n = view.point_count;
        if (n == 0u || n > FVIZ_IP_MAX_NODES)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "element-nodal extrapolation cell node count exceeds supported limits");
            goto fail;
        }
        if (m == 0u)
        {
            double* dst = (double*)fviz_data_array_data(output);
            for (i = 0u; i < n; ++i)
                for (c = 0u; c < components; ++c)
                    dst[(write_tuple + i) * components + c] = NAN;
            write_tuple += n;
            continue;
        }
        if (m > FVIZ_IP_MAX_POINTS)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "element-nodal extrapolation integration-point count exceeds supported limits");
            goto fail;
        }
        {
            const FVizVec3* coords = parametric != NULL ? parametric + begin : standard;
            if (parametric == NULL && fviz_integration_point_standard_coordinates(
                                          view.type, m, standard, FVIZ_IP_MAX_POINTS, &standard_count) != FVIZ_OK)
                goto fail;
            if (parametric == NULL && standard_count != m) goto fail;
            for (i = 0u; i < m; ++i)
            {
                FVizSize wc = 0u;
                if (fviz_cell_type_shape_weights(view.type, coords[i], shape[i], FVIZ_IP_MAX_NODES, &wc) != FVIZ_OK ||
                    wc != n)
                {
                    fviz_internal_set_error(
                        FVIZ_ERROR_NOT_SUPPORTED,
                        "element-nodal integration extrapolation lacks compatible cell shape weights");
                    goto fail;
                }
            }
        }
        for (c = 0u; c < components; ++c)
        {
            double local[FVIZ_IP_MAX_NODES];
            double* dst = (double*)fviz_data_array_data(output);
            if (fviz_ip_extrapolate_component(shape, m, n, values, begin, (uint32_t)c, options->fallback_policy,
                                              local) == FVIZ_FALSE)
            {
                fviz_internal_set_error(
                    FVIZ_ERROR_INVALID_STATE,
                    "element-nodal integration-point extrapolation system is singular/underdetermined");
                goto fail;
            }
            for (i = 0u; i < n; ++i)
                dst[(write_tuple + i) * components + c] = local[i];
        }
        write_tuple += n;
    }
    *out_values = output;
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}
