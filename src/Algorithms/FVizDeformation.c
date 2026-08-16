#include <float.h>
#include <math.h>
#include <string.h>

#include <FViz/Algorithms/FVizDeformation.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizDeformMeasurePartial
{
    double max_sq;
    double sum_sq;
    FVizSize finite_count;
} FVizDeformMeasurePartial;

typedef struct FVizDeformVectorView
{
    const unsigned char* data;
    FVizSize stride;
    FVizDataType type;
} FVizDeformVectorView;

typedef struct FVizDeformMeasureContext
{
    FVizDeformVectorView view;
    FVizDeformMeasurePartial* partials;
    FVizSize grain;
} FVizDeformMeasureContext;

typedef struct FVizDeformPointsContext
{
    const FVizVec3* source;
    FVizDeformVectorView view;
    FVizVec3* destination;
    double scale;
} FVizDeformPointsContext;


static void fviz_deformation_read3(const FVizDeformVectorView* view,FVizSize tuple,double* x,double* y,double* z)
{
    const unsigned char* p=view->data+tuple*view->stride;
    switch(view->type)
    {
        case FVIZ_DATA_INT8: { const int8_t* v=(const int8_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT8: { const uint8_t* v=(const uint8_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT16: { const int16_t* v=(const int16_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT16: { const uint16_t* v=(const uint16_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT32: { const int32_t* v=(const int32_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT32: { const uint32_t* v=(const uint32_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT64: { const int64_t* v=(const int64_t*)p; *x=(double)v[0]; *y=(double)v[1]; *z=(double)v[2]; break; }
        case FVIZ_DATA_UINT64: { const uint64_t* v=(const uint64_t*)p; *x=(double)v[0]; *y=(double)v[1]; *z=(double)v[2]; break; }
        case FVIZ_DATA_FLOAT32: { const float* v=(const float*)p; *x=(double)v[0]; *y=(double)v[1]; *z=(double)v[2]; break; }
        case FVIZ_DATA_FLOAT64: { const double* v=(const double*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        default: *x=0.0; *y=0.0; *z=0.0; break;
    }
}

static FVizDeformVectorView fviz_deformation_vector_view(const FVizDataArray* vectors)
{
    FVizDeformVectorView view;
    view.data=(const unsigned char*)fviz_data_array_const_data(vectors);
    view.stride=fviz_data_array_tuple_stride(vectors);
    view.type=fviz_data_array_type(vectors);
    return view;
}

static FVizResult fviz_deformation_validate_vectors(const FVizDataArray* vectors, FVizSize expected_count)
{
    if (vectors == NULL || fviz_data_array_components(vectors) != 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "deformation vectors must be a numeric three-component DataArray");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (expected_count != (FVizSize)-1 && fviz_data_array_tuple_count(vectors) != expected_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "deformation vector tuple count must match point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    switch (fviz_data_array_type(vectors))
    {
        case FVIZ_DATA_INT8: case FVIZ_DATA_UINT8:
        case FVIZ_DATA_INT16: case FVIZ_DATA_UINT16:
        case FVIZ_DATA_INT32: case FVIZ_DATA_UINT32:
        case FVIZ_DATA_INT64: case FVIZ_DATA_UINT64:
        case FVIZ_DATA_FLOAT32: case FVIZ_DATA_FLOAT64:
            return FVIZ_OK;
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "deformation vectors must use a numeric DataArray type");
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
}

static void fviz_deformation_measure_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizDeformMeasureContext* context = (FVizDeformMeasureContext*)user_data;
    const FVizSize slot = context->grain > 0u ? begin / context->grain : 0u;
    FVizDeformMeasurePartial* partial = &context->partials[slot];
    FVizSize i;
    double max_sq = 0.0;
    double sum_sq = 0.0;
    FVizSize finite_count = 0u;
    for (i = begin; i < end; ++i)
    {
        double x = 0.0, y = 0.0, z = 0.0;
        double magnitude_sq;
        fviz_deformation_read3(&context->view,i,&x,&y,&z);
        if (!isfinite(x) || !isfinite(y) || !isfinite(z)) continue;
        magnitude_sq = x*x + y*y + z*z;
        if (magnitude_sq > max_sq) max_sq = magnitude_sq;
        sum_sq += magnitude_sq;
        ++finite_count;
    }
    partial->max_sq = max_sq;
    partial->sum_sq = sum_sq;
    partial->finite_count = finite_count;
}

static void fviz_deformation_apply_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizDeformPointsContext* context = (FVizDeformPointsContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        double x = 0.0, y = 0.0, z = 0.0;
        fviz_deformation_read3(&context->view,i,&x,&y,&z);
        context->destination[i] = fviz_vec3(
            context->source[i].x + (float)(context->scale * x),
            context->source[i].y + (float)(context->scale * y),
            context->source[i].z + (float)(context->scale * z));
    }
}

void fviz_deformation_metrics_initialize(FVizDeformationMetrics* metrics)
{
    if (metrics == NULL) return;
    memset(metrics, 0, sizeof(*metrics));
    metrics->struct_size = (uint32_t)sizeof(*metrics);
}

FVizResult fviz_deformation_measure_vectors(
    const FVizDataArray* vectors,
    FVizDeformationMetrics* out_metrics)
{
    const FVizSize count = vectors != NULL ? fviz_data_array_tuple_count(vectors) : 0u;
    const FVizSize grain = 16384u;
    const FVizSize chunk_count = count > 0u ? (count + grain - 1u) / grain : 0u;
    FVizDeformMeasurePartial* partials = NULL;
    FVizDeformMeasureContext context;
    FVizSize i;
    double max_sq = 0.0;
    double sum_sq = 0.0;
    FVizSize finite_count = 0u;

    if (out_metrics == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "deformation metrics output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_deformation_metrics_initialize(out_metrics);
    if (fviz_deformation_validate_vectors(vectors, (FVizSize)-1) != FVIZ_OK)
        return fviz_last_error_code();
    out_metrics->tuple_count = count;
    if (count == 0u) return FVIZ_OK;

    partials = (FVizDeformMeasurePartial*)fviz_alloc(chunk_count * sizeof(*partials));
    if (partials == NULL) return fviz_last_error_code();
    memset(partials, 0, chunk_count * sizeof(*partials));
    context.view = fviz_deformation_vector_view(vectors);
    context.partials = partials;
    context.grain = grain;
    if (fviz_parallel_for(0u, count, grain, fviz_deformation_measure_range, &context) != FVIZ_OK)
    {
        fviz_free(partials);
        return fviz_last_error_code();
    }
    for (i = 0u; i < chunk_count; ++i)
    {
        if (partials[i].max_sq > max_sq) max_sq = partials[i].max_sq;
        sum_sq += partials[i].sum_sq;
        finite_count += partials[i].finite_count;
    }
    fviz_free(partials);
    out_metrics->finite_tuple_count = finite_count;
    out_metrics->maximum_magnitude = sqrt(max_sq);
    out_metrics->rms_magnitude = finite_count > 0u ? sqrt(sum_sq / (double)finite_count) : 0.0;
    return FVIZ_OK;
}

FVizResult fviz_deformation_compute_auto_scale(
    FVizBounds model_bounds,
    const FVizDataArray* vectors,
    double target_fraction,
    double minimum_scale,
    double maximum_scale,
    double* out_scale,
    FVizDeformationMetrics* out_metrics)
{
    FVizDeformationMetrics metrics;
    double dx, dy, dz, diagonal, scale;
    if (out_scale == NULL || !isfinite(target_fraction) || target_fraction <= 0.0 ||
        !isfinite(minimum_scale) || !isfinite(maximum_scale) || minimum_scale < 0.0 ||
        maximum_scale < minimum_scale)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid auto deformation scale arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_deformation_measure_vectors(vectors, &metrics) != FVIZ_OK) return fviz_last_error_code();
    if (model_bounds.valid == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "auto deformation scale requires valid model bounds");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    dx = (double)model_bounds.max.x - (double)model_bounds.min.x;
    dy = (double)model_bounds.max.y - (double)model_bounds.min.y;
    dz = (double)model_bounds.max.z - (double)model_bounds.min.z;
    diagonal = sqrt(dx*dx + dy*dy + dz*dz);
    if (metrics.maximum_magnitude > DBL_MIN && diagonal > DBL_MIN)
        scale = target_fraction * diagonal / metrics.maximum_magnitude;
    else
        scale = 1.0;
    if (scale < minimum_scale) scale = minimum_scale;
    if (scale > maximum_scale) scale = maximum_scale;
    *out_scale = scale;
    if (out_metrics != NULL) *out_metrics = metrics;
    return FVIZ_OK;
}

FVizResult fviz_deformation_apply_to_points(
    const FVizPoints* points,
    const FVizDataArray* vectors,
    double scale,
    FVizPoints** out_points)
{
    const FVizSize count = points != NULL ? fviz_points_count(points) : 0u;
    FVizPoints* result = NULL;
    FVizVec3* displaced = NULL;
    FVizDeformPointsContext context;
    if (points == NULL || out_points == NULL || !isfinite(scale))
    {
        if (out_points != NULL) *out_points = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point deformation requires points, finite scale and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_points = NULL;
    if (fviz_deformation_validate_vectors(vectors, count) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_points_create(&result) != FVIZ_OK) return fviz_last_error_code();
    if (count > 0u)
    {
        displaced = (FVizVec3*)fviz_alloc(count * sizeof(*displaced));
        if (displaced == NULL) { fviz_release(result); return fviz_last_error_code(); }
        context.source = fviz_points_data(points);
        context.view = fviz_deformation_vector_view(vectors);
        context.destination = displaced;
        context.scale = scale;
        if (fviz_parallel_for(0u, count, 4096u, fviz_deformation_apply_range, &context) != FVIZ_OK ||
            fviz_points_append_many_ids(result, displaced, count, NULL) != FVIZ_OK)
        {
            fviz_free(displaced);
            fviz_release(result);
            return fviz_last_error_code();
        }
        fviz_free(displaced);
    }
    *out_points = result;
    return FVIZ_OK;
}

FVizResult fviz_deformation_apply_to_poly_data(
    const FVizPolyData* poly_data,
    const FVizDataArray* vectors,
    double scale,
    FVizPolyData** out_poly_data)
{
    FVizPolyData* result = NULL;
    FVizVec3* displaced = NULL;
    FVizDeformPointsContext context;
    const FVizSize count = poly_data != NULL ? fviz_poly_data_point_count(poly_data) : 0u;
    if (poly_data == NULL || out_poly_data == NULL || !isfinite(scale))
    {
        if (out_poly_data != NULL) *out_poly_data = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly-data deformation requires input, finite scale and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_poly_data = NULL;
    if (fviz_deformation_validate_vectors(vectors, count) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_poly_data_deep_copy(poly_data, &result) != FVIZ_OK) return fviz_last_error_code();
    if (count > 0u)
    {
        displaced = (FVizVec3*)fviz_alloc(count * sizeof(*displaced));
        if (displaced == NULL) { fviz_release(result); return fviz_last_error_code(); }
        context.source = fviz_poly_data_points(poly_data);
        context.view = fviz_deformation_vector_view(vectors);
        context.destination = displaced;
        context.scale = scale;
        if (fviz_parallel_for(0u, count, 4096u, fviz_deformation_apply_range, &context) != FVIZ_OK ||
            fviz_poly_data_set_points(result, displaced, count) != FVIZ_OK)
        {
            fviz_free(displaced);
            fviz_release(result);
            return fviz_last_error_code();
        }
        fviz_free(displaced);
    }
    *out_poly_data = result;
    return FVIZ_OK;
}

FVizResult fviz_deformation_apply_to_unstructured_grid(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* vectors,
    double scale,
    FVizUnstructuredGrid** out_grid)
{
    return fviz_unstructured_grid_warp_by_array(grid, vectors, scale, out_grid);
}

static FVizResult fviz_deformation_build_point_buffer(
    const FVizVec3* source,
    FVizSize count,
    const FVizDataArray* vectors,
    double scale,
    FVizVec3** out_buffer)
{
    FVizVec3* buffer=NULL;
    FVizDeformPointsContext context;
    if (out_buffer==NULL || (count!=0u && source==NULL) || !isfinite(scale))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_buffer=NULL;
    if (fviz_deformation_validate_vectors(vectors,count)!=FVIZ_OK) return fviz_last_error_code();
    if (count==0u) return FVIZ_OK;
    buffer=(FVizVec3*)fviz_alloc(count*sizeof(*buffer));
    if (buffer==NULL) return fviz_last_error_code();
    context.source=source;
    context.view=fviz_deformation_vector_view(vectors);
    context.destination=buffer;
    context.scale=scale;
    if (fviz_parallel_for(0u,count,4096u,fviz_deformation_apply_range,&context)!=FVIZ_OK)
    {
        fviz_free(buffer);
        return fviz_last_error_code();
    }
    *out_buffer=buffer;
    return FVIZ_OK;
}

FVizResult fviz_deformation_update_points(
    FVizPoints* destination,
    const FVizPoints* base_points,
    const FVizDataArray* vectors,
    double scale)
{
    FVizVec3* buffer=NULL;
    const FVizSize count=base_points!=NULL?fviz_points_count(base_points):0u;
    if (destination==NULL || base_points==NULL || fviz_points_count(destination)!=count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"deformation point update requires matching source/destination point counts");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_deformation_build_point_buffer(fviz_points_data(base_points),count,vectors,scale,&buffer)!=FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_points_set_many(destination,0u,buffer,count)!=FVIZ_OK)
    {
        fviz_free(buffer);
        return fviz_last_error_code();
    }
    fviz_free(buffer);
    return FVIZ_OK;
}

FVizResult fviz_deformation_update_poly_data_points(
    FVizPolyData* destination,
    const FVizPolyData* base_poly_data,
    const FVizDataArray* vectors,
    double scale)
{
    FVizVec3* buffer=NULL;
    const FVizSize count=base_poly_data!=NULL?fviz_poly_data_point_count(base_poly_data):0u;
    if (destination==NULL || base_poly_data==NULL || fviz_poly_data_point_count(destination)!=count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"deformation PolyData update requires matching source/destination point counts");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_deformation_build_point_buffer(fviz_poly_data_points(base_poly_data),count,vectors,scale,&buffer)!=FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_poly_data_set_points(destination,buffer,count)!=FVIZ_OK)
    {
        fviz_free(buffer);
        return fviz_last_error_code();
    }
    fviz_free(buffer);
    return FVIZ_OK;
}

FVizResult fviz_deformation_update_unstructured_grid_points(
    FVizUnstructuredGrid* destination,
    const FVizUnstructuredGrid* base_grid,
    const FVizDataArray* vectors,
    double scale)
{
    FVizVec3* buffer=NULL;
    FVizPoints* destination_points;
    const FVizPoints* base_points;
    const FVizSize count=base_grid!=NULL?fviz_unstructured_grid_point_count(base_grid):0u;
    if (destination==NULL || base_grid==NULL || fviz_unstructured_grid_point_count(destination)!=count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"deformation grid update requires matching source/destination point counts");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    destination_points=fviz_unstructured_grid_points(destination);
    base_points=fviz_unstructured_grid_points((FVizUnstructuredGrid*)base_grid);
    if (fviz_deformation_build_point_buffer(fviz_points_data(base_points),count,vectors,scale,&buffer)!=FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_points_set_many(destination_points,0u,buffer,count)!=FVIZ_OK)
    {
        fviz_free(buffer);
        return fviz_last_error_code();
    }
    fviz_free(buffer);
    return FVIZ_OK;
}
