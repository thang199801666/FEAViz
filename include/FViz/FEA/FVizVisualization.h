#ifndef FVIZ_FEA_VISUALIZATION_H
#define FVIZ_FEA_VISUALIZATION_H

#include <stdint.h>

#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Rendering/FVizLookupTable.h>

FVIZ_EXTERN_C_BEGIN

/* Configures a discrete Abaqus-style blue-to-red contour lookup table. */
FVIZ_FEA_API FVizResult fviz_fea_configure_abaqus_contour_lut(
    FVizLookupTable* table, uint32_t interval_count);

/* Splits triangles at interval boundaries and stores direct RGB point colors
 * in output_color_array_name. components > 1 uses vector magnitude. */
FVIZ_FEA_API FVizResult fviz_fea_build_abaqus_banded_surface(
    const FVizPolyData* input,
    const char* scalar_array_name,
    uint32_t components,
    float range_minimum,
    float range_maximum,
    uint32_t interval_count,
    const char* output_color_array_name,
    FVizPolyData** out_surface);

/* Reconstructs original finite-element face perimeters from surface provenance,
 * suppressing the internal diagonals introduced by render triangulation. */
FVIZ_FEA_API FVizResult fviz_fea_extract_element_edges(
    const FVizPolyData* surface, FVizPolyData** out_edges);

/* Builds a continuous (smooth) contour surface: input topology is preserved
 * and every vertex receives a direct RGB color from the Abaqus rainbow of its
 * normalized scalar. components > 1 uses vector magnitude. Non-finite values
 * map to grey. */
FVIZ_FEA_API FVizResult fviz_fea_build_contour_surface(
    const FVizPolyData* input,
    const char* scalar_array_name,
    uint32_t components,
    float range_minimum,
    float range_maximum,
    const char* output_color_array_name,
    FVizPolyData** out_surface);

/* Extracts iso-value contour lines from a surface using the same interval
 * conventions as the banded builder. Mid-levels of each interval are traced
 * and each output vertex is tagged with its level (output_scalar_array_name,
 * e.g. "contour_level"). Original cell/face provenance is copied through so
 * lines can be picked and mapped back to elements. */
FVIZ_FEA_API FVizResult fviz_fea_build_contour_lines(
    const FVizPolyData* input,
    const char* scalar_array_name,
    uint32_t components,
    float range_minimum,
    float range_maximum,
    uint32_t interval_count,
    const char* output_scalar_array_name,
    FVizPolyData** out_lines);

/* Reports the minimum/maximum of a surface scalar (magnitude for vectors)
 * together with the original cell/face provenance of the extrema points, for
 * min/max markers and annotations. */
typedef struct FVizFEAExtrema
{
    uint32_t struct_size;
    FVizSize min_point_id;
    FVizSize max_point_id;
    double min_value;
    double max_value;
    uint64_t min_cell_id;
    uint64_t max_cell_id;
    uint64_t min_face_id;
    uint64_t max_face_id;
} FVizFEAExtrema;

FVIZ_FEA_API void fviz_fea_extrema_initialize(FVizFEAExtrema* extrema);
FVIZ_FEA_API FVizResult fviz_fea_find_extrema(
    const FVizPolyData* surface,
    const char* scalar_array_name,
    uint32_t components,
    FVizFEAExtrema* out_extrema);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_VISUALIZATION_H */
