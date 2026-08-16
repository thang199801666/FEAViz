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

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_VISUALIZATION_H */
