#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/FEA/FVizVisualization.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizFEAContourVertex
{
    FVizVec3 point;
    double scalar;
} FVizFEAContourVertex;

typedef struct FVizFEAEdgeUse
{
    uint64_t cell;
    uint64_t face;
    uint32_t first;
    uint32_t second;
} FVizFEAEdgeUse;

static void fviz_fea_abaqus_rainbow(double normalized, float color[3])
{
    double hue;
    double h;
    int sector;
    float fraction;
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    hue = (1.0 - normalized) * 240.0;
    h = hue / 60.0;
    sector = (int)h;
    if (sector > 5) sector = 5;
    fraction = (float)(h - (double)sector);
    switch (sector)
    {
        case 0: color[0]=1.0f; color[1]=fraction; color[2]=0.0f; break;
        case 1: color[0]=1.0f-fraction; color[1]=1.0f; color[2]=0.0f; break;
        case 2: color[0]=0.0f; color[1]=1.0f; color[2]=fraction; break;
        case 3: color[0]=0.0f; color[1]=1.0f-fraction; color[2]=1.0f; break;
        case 4: color[0]=fraction; color[1]=0.0f; color[2]=1.0f; break;
        default: color[0]=1.0f; color[1]=0.0f; color[2]=1.0f-fraction; break;
    }
}

FVizResult fviz_fea_configure_abaqus_contour_lut(
    FVizLookupTable* table, uint32_t interval_count)
{
    FVizSize i;
    FVizSize size;
    if (table == NULL || interval_count < 2u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "Abaqus contour LUT requires a table and at least two intervals");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    size = fviz_lookup_table_size(table);
    if (size == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "Abaqus contour LUT cannot be empty");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i=0u;i<size;++i)
    {
        uint32_t band = (uint32_t)(((uint64_t)i * interval_count) / size);
        float color[3];
        if (band >= interval_count) band = interval_count - 1u;
        fviz_fea_abaqus_rainbow(((double)band + 0.5) / (double)interval_count, color);
        if (fviz_lookup_table_set_color(table,i,color[0],color[1],color[2])!=FVIZ_OK)
            return fviz_last_error_code();
    }
    fviz_lookup_table_set_nan_color(table,0.55f,0.55f,0.55f);
    return FVIZ_OK;
}

static FVizBool fviz_fea_surface_scalar(
    const FVizDataArray* array, FVizSize point, uint32_t components, double* value)
{
    uint32_t component;
    double squared=0.0;
    if (components==1u)
        return fviz_data_array_get_component(array,point,0u,value)==FVIZ_OK && isfinite(*value);
    for (component=0u;component<components;++component)
    {
        double item=0.0;
        if (fviz_data_array_get_component(array,point,component,&item)!=FVIZ_OK || !isfinite(item))
            return FVIZ_FALSE;
        squared += item*item;
    }
    *value=sqrt(squared);
    return isfinite(*value) ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizFEAContourVertex fviz_fea_interpolate(
    FVizFEAContourVertex a, FVizFEAContourVertex b, double threshold)
{
    FVizFEAContourVertex result;
    const double denominator=b.scalar-a.scalar;
    double t=denominator==0.0?0.0:(threshold-a.scalar)/denominator;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    result.point=fviz_vec3(
        (float)(a.point.x+(b.point.x-a.point.x)*t),
        (float)(a.point.y+(b.point.y-a.point.y)*t),
        (float)(a.point.z+(b.point.z-a.point.z)*t));
    result.scalar=threshold;
    return result;
}

static FVizBool fviz_fea_clip_inside(double value,double threshold,FVizBool keep_above,FVizBool inclusive)
{
    if(keep_above!=FVIZ_FALSE)return value>=threshold?FVIZ_TRUE:FVIZ_FALSE;
    return inclusive!=FVIZ_FALSE?(value<=threshold?FVIZ_TRUE:FVIZ_FALSE):
        (value<threshold?FVIZ_TRUE:FVIZ_FALSE);
}

static uint32_t fviz_fea_clip_polygon(
    const FVizFEAContourVertex* input,uint32_t input_count,FVizFEAContourVertex* output,
    double threshold,FVizBool keep_above,FVizBool inclusive)
{
    FVizFEAContourVertex previous;
    FVizBool previous_inside;
    uint32_t i;
    uint32_t output_count=0u;
    if(input_count==0u)return 0u;
    previous=input[input_count-1u];
    previous_inside=fviz_fea_clip_inside(previous.scalar,threshold,keep_above,inclusive);
    for(i=0u;i<input_count;++i)
    {
        const FVizFEAContourVertex current=input[i];
        const FVizBool current_inside=fviz_fea_clip_inside(current.scalar,threshold,keep_above,inclusive);
        if(current_inside!=previous_inside)
            output[output_count++]=fviz_fea_interpolate(previous,current,threshold);
        if(current_inside!=FVIZ_FALSE)output[output_count++]=current;
        previous=current;previous_inside=current_inside;
    }
    return output_count;
}

FVizResult fviz_fea_build_abaqus_banded_surface(
    const FVizPolyData* input,const char* scalar_array_name,uint32_t components,
    float range_minimum,float range_maximum,uint32_t interval_count,
    const char* output_color_array_name,FVizPolyData** out_surface)
{
    const FVizDataArray* scalars;
    const FVizVec3* points;
    const uint32_t* triangles;
    FVizPolyData* output=NULL;
    FVizDataArray* colors=NULL;
    FVizSize triangle;
    const double width=((double)range_maximum-(double)range_minimum)/(double)interval_count;
    if(out_surface!=NULL)*out_surface=NULL;
    if(input==NULL||scalar_array_name==NULL||output_color_array_name==NULL||out_surface==NULL||
        components==0u||interval_count<2u||!(range_maximum>range_minimum))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "Banded surface requires valid input, range, components, and intervals");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalars=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(input),scalar_array_name);
    if(scalars==NULL||fviz_data_array_tuple_count(scalars)!=fviz_poly_data_point_count(input)||
        fviz_data_array_components(scalars)<components)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "Banded surface scalar array is missing or incompatible");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if(fviz_poly_data_create(&output)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_FLOAT32,3u,&colors)!=FVIZ_OK)goto fail;
    points=fviz_poly_data_points(input);triangles=fviz_poly_data_triangle_indices(input);
    for(triangle=0u;triangle<fviz_poly_data_triangle_count(input);++triangle)
    {
        FVizFEAContourVertex original[3];
        double triangle_min=range_maximum,triangle_max=range_minimum;
        uint32_t corner;
        int first_band,last_band,band;
        for(corner=0u;corner<3u;++corner)
        {
            const uint32_t point_id=triangles[triangle*3u+corner];
            original[corner].point=points[point_id];
            if(fviz_fea_surface_scalar(scalars,point_id,components,&original[corner].scalar)==FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                    "Banded surface contains a non-finite scalar value");
                goto fail;
            }
            if(original[corner].scalar<triangle_min)triangle_min=original[corner].scalar;
            if(original[corner].scalar>triangle_max)triangle_max=original[corner].scalar;
        }
        first_band=(int)((triangle_min-range_minimum)/width);
        last_band=(int)((triangle_max-range_minimum)/width);
        if (first_band < 0) first_band = 0;
        if (last_band < 0) last_band = 0;
        if(first_band>=(int)interval_count)first_band=(int)interval_count-1;
        if(last_band>=(int)interval_count)last_band=(int)interval_count-1;
        for(band=first_band;band<=last_band;++band)
        {
            FVizFEAContourVertex clipped_a[16],clipped_b[16];
            uint32_t count,i;
            uint32_t ids[16];
            float color[3];
            const double lower=range_minimum+width*(double)band;
            const double upper=range_minimum+width*(double)(band+1);
            count=fviz_fea_clip_polygon(original,3u,clipped_a,lower,FVIZ_TRUE,FVIZ_TRUE);
            count=fviz_fea_clip_polygon(clipped_a,count,clipped_b,upper,FVIZ_FALSE,
                band+1==(int)interval_count?FVIZ_TRUE:FVIZ_FALSE);
            if(count<3u)continue;
            fviz_fea_abaqus_rainbow(((double)band+0.5)/(double)interval_count,color);
            for(i=0u;i<count;++i)
            {
                if(fviz_poly_data_add_point(output,clipped_b[i].point,&ids[i])!=FVIZ_OK||
                    fviz_data_array_append_tuple(colors,color)!=FVIZ_OK)goto fail;
            }
            for(i=1u;i+1u<count;++i)
                if(fviz_poly_data_add_triangle(output,ids[0],ids[i],ids[i+1u])!=FVIZ_OK)goto fail;
        }
    }
    if(fviz_attribute_set_add(fviz_poly_data_point_data(output),output_color_array_name,colors)!=FVIZ_OK||
        fviz_poly_data_compute_normals(output)!=FVIZ_OK||fviz_poly_data_validate(output)!=FVIZ_OK)goto fail;
    fviz_release(colors);*out_surface=output;return FVIZ_OK;
fail:
    fviz_release(colors);fviz_release(output);return fviz_last_error_code();
}

static int fviz_fea_edge_use_compare(const void* left,const void* right)
{
    const FVizFEAEdgeUse* a=(const FVizFEAEdgeUse*)left;
    const FVizFEAEdgeUse* b=(const FVizFEAEdgeUse*)right;
#define FVIZ_EDGE_COMPARE(field) if(a->field<b->field)return -1;if(a->field>b->field)return 1
    FVIZ_EDGE_COMPARE(cell);FVIZ_EDGE_COMPARE(face);FVIZ_EDGE_COMPARE(first);FVIZ_EDGE_COMPARE(second);
#undef FVIZ_EDGE_COMPARE
    return 0;
}

static int fviz_fea_uint64_compare(const void* left,const void* right)
{
    const uint64_t a=*(const uint64_t*)left,b=*(const uint64_t*)right;
    return a<b?-1:(a>b?1:0);
}

FVizResult fviz_fea_extract_element_edges(const FVizPolyData* surface,FVizPolyData** out_edges)
{
    const FVizDataArray* cells;
    const FVizDataArray* faces;
    const uint32_t* triangles;
    const FVizSize triangle_count=surface!=NULL?fviz_poly_data_triangle_count(surface):0u;
    FVizSize use_count,bytes,i,perimeter_count=0u,unique_count=0u;
    FVizFEAEdgeUse* uses=NULL;
    uint64_t* perimeter=NULL;
    FVizPolyData* output=NULL;
    if(out_edges!=NULL)*out_edges=NULL;
    if(surface==NULL||out_edges==NULL||triangle_count==0u)return FVIZ_ERROR_INVALID_ARGUMENT;
    cells=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(surface),"FVizOriginalCellIds");
    faces=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(surface),"FVizOriginalFaceIds");
    if(cells==NULL||faces==NULL||fviz_data_array_type(cells)!=FVIZ_DATA_UINT64||
        fviz_data_array_type(faces)!=FVIZ_DATA_UINT64||
        fviz_data_array_tuple_count(cells)!=triangle_count||
        fviz_data_array_tuple_count(faces)!=triangle_count)return FVIZ_ERROR_INVALID_STATE;
    if(fviz_size_multiply(triangle_count,3u,&use_count)!=FVIZ_OK||
        fviz_size_multiply(use_count,sizeof(*uses),&bytes)!=FVIZ_OK)return fviz_last_error_code();
    uses=(FVizFEAEdgeUse*)fviz_alloc(bytes);
    if(uses==NULL)return fviz_last_error_code();
    triangles=fviz_poly_data_triangle_indices(surface);
    for(i=0u;i<triangle_count;++i)
    {
        const uint64_t cell=*(const uint64_t*)fviz_data_array_const_tuple(cells,i);
        const uint64_t face=*(const uint64_t*)fviz_data_array_const_tuple(faces,i);
        uint32_t edge;
        for(edge=0u;edge<3u;++edge)
        {
            FVizFEAEdgeUse* use=&uses[i*3u+edge];
            uint32_t a=triangles[i*3u+edge],b=triangles[i*3u+(edge+1u)%3u];
            if(a>b){const uint32_t swap=a;a=b;b=swap;}
            use->cell=cell;use->face=face;use->first=a;use->second=b;
        }
    }
    qsort(uses,(size_t)use_count,sizeof(*uses),fviz_fea_edge_use_compare);
    if(fviz_size_multiply(use_count,sizeof(*perimeter),&bytes)!=FVIZ_OK)goto fail;
    perimeter=(uint64_t*)fviz_alloc(bytes);if(perimeter==NULL)goto fail;
    for(i=0u;i<use_count;)
    {
        FVizSize end=i+1u;
        while(end<use_count&&fviz_fea_edge_use_compare(&uses[i],&uses[end])==0)++end;
        if(end-i==1u)perimeter[perimeter_count++]=((uint64_t)uses[i].first<<32u)|uses[i].second;
        i=end;
    }
    qsort(perimeter,(size_t)perimeter_count,sizeof(*perimeter),fviz_fea_uint64_compare);
    for(i=0u;i<perimeter_count;++i)if(i==0u||perimeter[i]!=perimeter[i-1u])perimeter[unique_count++]=perimeter[i];
    if(fviz_poly_data_create(&output)!=FVIZ_OK||
        fviz_poly_data_add_points(output,fviz_poly_data_points(surface),
            fviz_poly_data_point_count(surface),NULL)!=FVIZ_OK)goto fail;
    for(i=0u;i<unique_count;++i)
        if(fviz_poly_data_add_line(output,(uint32_t)(perimeter[i]>>32u),(uint32_t)perimeter[i])!=FVIZ_OK)goto fail;
    if(fviz_poly_data_validate(output)!=FVIZ_OK)goto fail;
    fviz_free(perimeter);fviz_free(uses);*out_edges=output;return FVIZ_OK;
fail:
    fviz_free(perimeter);fviz_free(uses);fviz_release(output);return fviz_last_error_code();
}
