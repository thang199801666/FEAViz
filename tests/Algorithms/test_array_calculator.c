#include <math.h>
#include <stdio.h>
#include <string.h>
#include <FViz/FViz.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while(0)
int main(void)
{
    FVizDataArray *vector=NULL,*tensor=NULL,*out=NULL;
    FVizArrayCalculatorOptions o;
    const float v[6]={3,4,0, 0,0,5};
    const double s[6]={100,40,10, 20,0,0};
    double x=0.0;
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32,3u,&vector)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(vector,v,2u)==FVIZ_OK);
    fviz_array_calculator_options_initialize(&o);
    o.operation=FVIZ_ARRAY_CALC_MAGNITUDE;
    CHECK(fviz_array_calculator_compute(vector,&o,&out)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-5.0)<1e-12);
    CHECK(fviz_data_array_get_component(out,1u,0u,&x)==FVIZ_OK && fabs(x-5.0)<1e-12);
    fviz_release(out); out=NULL;
    o.operation=FVIZ_ARRAY_CALC_COMPONENT; o.component=1u;
    CHECK(fviz_array_calculator_compute(vector,&o,&out)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-4.0)<1e-12);
    fviz_release(out); out=NULL;
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,6u,&tensor)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(tensor,s)==FVIZ_OK);
    o.operation=FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC;
    CHECK(fviz_array_calculator_compute(tensor,&o,&out)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK);
    CHECK(fabs(x-sqrt(0.5*(60.0*60.0+30.0*30.0+(-90.0)*(-90.0))+3.0*400.0))<1e-10);
    fviz_release(out); out=NULL;
    {
        const double diagonal[6]={9.0,4.0,1.0,0.0,0.0,0.0};
        FVizDataArray* diag=NULL;
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,6u,&diag)==FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(diag,diagonal)==FVIZ_OK);
        o.operation=FVIZ_ARRAY_CALC_PRINCIPAL_VALUES;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_components(out)==3u);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-9.0)<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,1u,&x)==FVIZ_OK && fabs(x-4.0)<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,2u,&x)==FVIZ_OK && fabs(x-1.0)<1e-12);
        fviz_release(out); out=NULL;
        o.operation=FVIZ_ARRAY_CALC_TENSOR_MEAN;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-(14.0/3.0))<1e-12);
        fviz_release(out); out=NULL;
        o.operation=FVIZ_ARRAY_CALC_HALF_PRINCIPAL_SPAN;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-4.0)<1e-12);
        fviz_release(out); out=NULL;
        o.operation=FVIZ_ARRAY_CALC_PRINCIPAL_SPAN;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-8.0)<1e-12);
        fviz_release(out); out=NULL;
        o.operation=FVIZ_ARRAY_CALC_DEVIATORIC_TENSOR;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_components(out)==6u);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-(9.0-14.0/3.0))<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,1u,&x)==FVIZ_OK && fabs(x-(4.0-14.0/3.0))<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,2u,&x)==FVIZ_OK && fabs(x-(1.0-14.0/3.0))<1e-12);
        fviz_release(out); out=NULL;
        o.operation=FVIZ_ARRAY_CALC_PRINCIPAL_DIRECTIONS;
        CHECK(fviz_array_calculator_compute(diag,&o,&out)==FVIZ_OK);
        CHECK(fviz_data_array_components(out)==9u);
        CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(fabs(x)-1.0)<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,4u,&x)==FVIZ_OK && fabs(fabs(x)-1.0)<1e-12);
        CHECK(fviz_data_array_get_component(out,0u,8u,&x)==FVIZ_OK && fabs(fabs(x)-1.0)<1e-12);
        fviz_release(out); out=NULL;
        fviz_release(diag);
    }
    o.operation=FVIZ_ARRAY_CALC_SCALE_OFFSET; o.scale=2.0; o.offset=1.0;
    CHECK(fviz_array_calculator_compute(vector,&o,&out)==FVIZ_OK);
    CHECK(fviz_data_array_components(out)==3u);
    CHECK(fviz_data_array_get_component(out,0u,0u,&x)==FVIZ_OK && fabs(x-7.0)<1e-12);
    fviz_release(out); out=NULL;
    {
        FVizPolyData* poly=NULL;
        FVizArrayCalculatorFilter* filter=NULL;
        FVizPolyData* filtered;
        const FVizDataArray* magnitude;
        const FVizVec3 points[2]={{0,0,0},{1,0,0}};
        o.operation=FVIZ_ARRAY_CALC_MAGNITUDE; o.component=0u; o.scale=1.0; o.offset=0.0;
        CHECK(fviz_poly_data_create(&poly)==FVIZ_OK);
        CHECK(fviz_poly_data_add_points_ids(poly,points,2u,NULL)==FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(poly),"V",vector)==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_create(&filter)==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_set_input_data(filter,poly)==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_set_array(filter,FVIZ_ARRAY_CALC_POINT_DATA,"V")==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_set_result_name(filter,"Magnitude")==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_set_options(filter,&o)==FVIZ_OK);
        CHECK(fviz_array_calculator_filter_update(filter)==FVIZ_OK);
        filtered=fviz_array_calculator_filter_output(filter);
        magnitude=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(filtered),"Magnitude");
        CHECK(magnitude!=NULL && fviz_data_array_tuple_count(magnitude)==2u);
        CHECK(strcmp(fviz_attribute_set_active_name(fviz_poly_data_const_point_data(filtered),FVIZ_ATTRIBUTE_SCALARS),"Magnitude")==0);
        CHECK(fviz_data_array_get_component(magnitude,1u,0u,&x)==FVIZ_OK && fabs(x-5.0)<1e-12);
        fviz_release(filter); fviz_release(poly);
    }
    fviz_release(tensor); fviz_release(vector);
    return 0;
}
