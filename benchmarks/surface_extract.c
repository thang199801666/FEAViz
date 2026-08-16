#include <FViz/FViz.h>
#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    return timespec_get(&value,TIME_UTC)==TIME_UTC?(double)value.tv_sec+(double)value.tv_nsec*1.0e-9:0.0;
}
static FVizSize point_id(uint32_t x,uint32_t y,uint32_t z,uint32_t edge)
{ return (FVizSize)x+(FVizSize)edge*((FVizSize)y+(FVizSize)edge*z); }
static int run_case(uint32_t n)
{
    const uint32_t edge=n+1u;
    const FVizSize point_count=(FVizSize)edge*edge*edge;
    const FVizSize cell_count=(FVizSize)n*n*n;
    FVizVec3* points=(FVizVec3*)fviz_alloc(point_count*sizeof(*points));
    uint32_t* ids=(uint32_t*)fviz_alloc(cell_count*8u*sizeof(*ids));
    FVizUnstructuredGrid* grid=NULL;
    FVizPolyData* surface=NULL;
    FVizSize i=0u,c=0u;
    uint32_t x,y,z;
    double start,seconds;
    if (points==NULL || ids==NULL) return 2;
    for (z=0u;z<edge;++z) for (y=0u;y<edge;++y) for (x=0u;x<edge;++x)
        points[i++]=fviz_vec3((float)x,(float)y,(float)z);
    for (z=0u;z<n;++z) for (y=0u;y<n;++y) for (x=0u;x<n;++x)
    {
        ids[c++]=(uint32_t)point_id(x,y,z,edge); ids[c++]=(uint32_t)point_id(x+1u,y,z,edge);
        ids[c++]=(uint32_t)point_id(x+1u,y+1u,z,edge); ids[c++]=(uint32_t)point_id(x,y+1u,z,edge);
        ids[c++]=(uint32_t)point_id(x,y,z+1u,edge); ids[c++]=(uint32_t)point_id(x+1u,y,z+1u,edge);
        ids[c++]=(uint32_t)point_id(x+1u,y+1u,z+1u,edge); ids[c++]=(uint32_t)point_id(x,y+1u,z+1u,edge);
    }
    if (fviz_unstructured_grid_create(&grid)!=FVIZ_OK ||
        fviz_unstructured_grid_reserve(grid,point_count,cell_count,cell_count*8u)!=FVIZ_OK ||
        fviz_unstructured_grid_add_points(grid,points,point_count,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cells_fixed(grid,FVIZ_CELL_HEXAHEDRON,8u,cell_count,ids)!=FVIZ_OK) return 3;
    start=wall_seconds();
    if (fviz_unstructured_grid_extract_surface(grid,&surface)!=FVIZ_OK) return 4;
    seconds=wall_seconds()-start;
    printf("%u,%llu,%llu,%llu,%.6f,%.3f\n",n,(unsigned long long)cell_count,(unsigned long long)point_count,
        (unsigned long long)fviz_poly_data_triangle_count(surface),seconds,seconds>0.0?(double)cell_count/seconds:0.0);
    fviz_release(surface); fviz_release(grid); fviz_free(ids); fviz_free(points); return 0;
}
int main(void)
{
    const uint32_t cases[]={18u,32u,50u};
    FVizSize i;
    puts("edge_cells,cells,points,surface_triangles,seconds,cells_per_second");
    for (i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) if (run_case(cases[i])!=0) return 1;
    return 0;
}
