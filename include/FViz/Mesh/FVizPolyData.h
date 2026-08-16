#ifndef FVIZ_MESH_POLY_DATA_H
#define FVIZ_MESH_POLY_DATA_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPolyData FVizPolyData;
#define FVIZ_TYPE_POLY_DATA UINT64_C(0xCC28638594EC02C7)

FVIZ_API FVizResult fviz_poly_data_create(FVizPolyData** out_poly_data);
FVIZ_API FVizResult fviz_poly_data_shallow_copy(const FVizPolyData* source, FVizPolyData** out_copy);
FVIZ_API FVizResult fviz_poly_data_deep_copy(const FVizPolyData* source, FVizPolyData** out_copy);
FVIZ_API FVizResult fviz_poly_data_copy_structure(const FVizPolyData* source, FVizPolyData** out_copy);
FVIZ_API FVizSize fviz_poly_data_memory_size(const FVizPolyData* poly_data);
FVIZ_API void fviz_poly_data_clear(FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_reserve(FVizPolyData* poly_data, FVizSize point_capacity,
                                           FVizSize triangle_capacity);
FVIZ_API FVizResult fviz_poly_data_add_point(FVizPolyData* poly_data, FVizVec3 point, uint32_t* out_index);
FVIZ_API FVizResult fviz_poly_data_add_points(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count,
                                              uint32_t* out_first_index);
FVIZ_API FVizResult fviz_poly_data_add_points_ids(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count,
                                                  FVizId* out_first_id);
/* Replaces coordinates in bulk. Existing topology requires an unchanged point count. */
FVIZ_API FVizResult fviz_poly_data_set_points(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count);
FVIZ_API FVizResult fviz_poly_data_set_points_range(FVizPolyData* poly_data, FVizSize first, const FVizVec3* points,
                                                    FVizSize point_count);
FVIZ_API FVizResult fviz_poly_data_set_point(FVizPolyData* poly_data, FVizSize index, FVizVec3 point);
FVIZ_API FVizResult fviz_poly_data_get_point(const FVizPolyData* poly_data, FVizSize index, FVizVec3* out_point);
FVIZ_API FVizResult fviz_poly_data_add_triangle(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c);
FVIZ_API FVizResult fviz_poly_data_add_triangles(FVizPolyData* poly_data, const uint32_t* triangle_indices,
                                                 FVizSize triangle_count);
FVIZ_API FVizResult fviz_poly_data_add_line(FVizPolyData* poly_data, uint32_t a, uint32_t b);
FVIZ_API FVizResult fviz_poly_data_add_lines(FVizPolyData* poly_data, const uint32_t* line_indices,
                                             FVizSize line_count);
FVIZ_API FVizResult fviz_poly_data_add_vertex(FVizPolyData* poly_data, uint32_t point_id);
FVIZ_API FVizResult fviz_poly_data_add_poly_vertex(FVizPolyData* poly_data, FVizSize point_count,
                                                   const uint32_t* point_ids);
FVIZ_API FVizResult fviz_poly_data_add_poly_line(FVizPolyData* poly_data, FVizSize point_count,
                                                 const uint32_t* point_ids);
FVIZ_API FVizResult fviz_poly_data_add_polygon(FVizPolyData* poly_data, FVizSize point_count,
                                               const uint32_t* point_ids);
FVIZ_API FVizResult fviz_poly_data_add_quad(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
FVIZ_API FVizResult fviz_poly_data_add_triangle_strip(FVizPolyData* poly_data, FVizSize point_count,
                                                      const uint32_t* point_ids);
/* Native logical-cell path. LINE/TRIANGLE cells also populate the legacy render-ready
 * index cache when all IDs fit in uint32_t; wider IDs remain valid logical topology. */
FVIZ_API FVizResult fviz_poly_data_add_cell_ids(FVizPolyData* poly_data, FVizCellType type, FVizSize point_count,
                                                const FVizId* point_ids);
FVIZ_API FVizSize fviz_poly_data_point_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_triangle_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_line_count(const FVizPolyData* poly_data);
/* VTK-style logical cell counts. Legacy triangle_count/line_count remain render-ready primitive counts. */
FVIZ_API FVizSize fviz_poly_data_vert_cell_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_line_cell_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_poly_cell_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_strip_cell_count(const FVizPolyData* poly_data);
FVIZ_API FVizSize fviz_poly_data_cell_count(const FVizPolyData* poly_data);
FVIZ_API const FVizCellArray* fviz_poly_data_verts(const FVizPolyData* poly_data);
FVIZ_API const FVizCellArray* fviz_poly_data_lines(const FVizPolyData* poly_data);
FVIZ_API const FVizCellArray* fviz_poly_data_polys(const FVizPolyData* poly_data);
FVIZ_API const FVizCellArray* fviz_poly_data_strips(const FVizPolyData* poly_data);
FVIZ_API const FVizVec3* fviz_poly_data_points(const FVizPolyData* poly_data);
FVIZ_API const FVizVec3* fviz_poly_data_normals(const FVizPolyData* poly_data);
FVIZ_API const uint32_t* fviz_poly_data_triangle_indices(const FVizPolyData* poly_data);
FVIZ_API const uint32_t* fviz_poly_data_line_indices(const FVizPolyData* poly_data);
FVIZ_API FVizBool fviz_poly_data_has_normals(const FVizPolyData* poly_data);
FVIZ_API FVizBounds fviz_poly_data_bounds(const FVizPolyData* poly_data);
/* Fine-grained revisions for render/spatial caches.  Geometry tracks points and
 * normals, topology tracks connectivity, and attributes track scalar/field data.
 * The aggregate FVizObject MTime remains the authoritative all-content revision. */
FVIZ_API FVizMTime fviz_poly_data_geometry_mtime(const FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_geometry_dirty_range_since(const FVizPolyData* poly_data, FVizMTime since_mtime,
                                                              FVizDirtyRange* out_range);
FVIZ_API FVizMTime fviz_poly_data_topology_mtime(const FVizPolyData* poly_data);
FVIZ_API FVizMTime fviz_poly_data_attribute_mtime(const FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_compute_normals(FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_validate(const FVizPolyData* poly_data);
FVIZ_API FVizResult fviz_poly_data_set_scalars(FVizPolyData* poly_data, FVizDataArray* scalars);
FVIZ_API const FVizDataArray* fviz_poly_data_const_scalars(const FVizPolyData* poly_data);
FVIZ_API FVizAttributeSet* fviz_poly_data_point_data(FVizPolyData* poly_data);
FVIZ_API const FVizAttributeSet* fviz_poly_data_const_point_data(const FVizPolyData* poly_data);
FVIZ_API FVizAttributeSet* fviz_poly_data_cell_data(FVizPolyData* poly_data);
FVIZ_API const FVizAttributeSet* fviz_poly_data_const_cell_data(const FVizPolyData* poly_data);
FVIZ_API FVizAttributeSet* fviz_poly_data_field_data(FVizPolyData* poly_data);
FVIZ_API const FVizAttributeSet* fviz_poly_data_const_field_data(const FVizPolyData* poly_data);

/* Extracts every cell edge of a PolyData as line cells (vtkExtractEdges
 * compatible). Shared/manifold interior edges are emitted once. */
FVIZ_API FVizResult fviz_poly_data_extract_edges(const FVizPolyData* input, FVizPolyData** out_edges);

/* 2D Delaunay triangulation of the input points (vtkDelaunay2D compatible).
 * Points are projected to the XY plane (z is carried through as elevation);
 * the output is a triangulated PolyData. At least 3 non-collinear points are
 * required. */
FVIZ_API FVizResult fviz_poly_data_delaunay_2d(const FVizPolyData* input, FVizPolyData** out_triangulation);

/* Materializes glyphs at the input points (vtkGlyph3D compatible). For each
 * point the scalar/vector field drives scale and orientation. When a 3-D
 * vector array is supplied each glyph is an arrow oriented along the vector
 * and scaled by its magnitude * scale_factor; otherwise a fixed-direction
 * arrow scaled by a scalar array (or uniformly) is emitted. Output geometry is
 * a PolyData of lines (arrows) suitable for rendering or further processing. */
FVIZ_API FVizResult fviz_poly_data_glyph_3d(const FVizPolyData* input, const char* scale_array_name,
                                            const char* orientation_array_name, double scale_factor,
                                            FVizPolyData** out_glyphs);

/* Appends the points and cells of `other` onto `target`. Point attributes are
 * concatenated when both sides carry the same array; otherwise only target
 * arrays are retained. Cell attributes of `other` are copied when present. */
FVIZ_API FVizResult fviz_poly_data_append(FVizPolyData* target, const FVizPolyData* other);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_POLY_DATA_H */
