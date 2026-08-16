// FEAViz C++ binding - pipeline filters.
//
// RAII wrappers over the FEAViz filter API: threshold, warp, surface
// extraction, slicing, cell-data-to-point smoothing, and transforms. Filters
// are stateful objects; call update() and read the output.

#ifndef FVIZ_CPP_FILTER_HPP
#define FVIZ_CPP_FILTER_HPP

#include <FViz/Pipeline/FVizFilter.h>

#include "FVizCppObject.hpp"
#include "FVizCppData.hpp"

#include <string>

namespace fviz {

// ---------------------------------------------------------------------------
// Filter - base wrapper over the filter pipeline objects.
// ---------------------------------------------------------------------------
class Filter : public Object<FVizFilter> {
public:
    Filter() = default;
    explicit Filter(FVizFilter* owned) : Object<FVizFilter>(owned) {}
    explicit Filter(void* owned) : Object<FVizFilter>(owned) {}

    // Set the input grid directly.
    void setInput(UnstructuredGrid& grid) { detail::checkResult(fviz_filter_set_input(ptr_, grid.get())); }
    void setInputConnection(Filter& upstream) { detail::checkResult(fviz_filter_set_input_connection(ptr_, upstream.get())); }
    Filter inputConnection() const
    {
        FVizFilter* filter = ptr_ ? fviz_filter_input_connection(ptr_) : nullptr;
        return Filter(filter != nullptr ? static_cast<FVizFilter*>(fviz_retain(filter)) : nullptr);
    }

    FVizFilterOutputType outputType() const noexcept
    {
        return ptr_ ? fviz_filter_output_type(ptr_) : FVIZ_FILTER_OUTPUT_NONE;
    }

    // Runs the filter. Throws on failure.
    void update() { detail::checkResult(fviz_filter_update(ptr_)); }

    // Output accessors. The returned wrapper owns a reference.
    UnstructuredGrid outputGrid() const
    {
        FVizUnstructuredGrid* grid = ptr_ ? fviz_filter_output(ptr_) : nullptr;
        return UnstructuredGrid(grid != nullptr ? static_cast<FVizUnstructuredGrid*>(fviz_retain(grid)) : nullptr);
    }
    PolyData outputPolyData() const
    {
        FVizPolyData* poly = ptr_ ? fviz_filter_poly_data_output(ptr_) : nullptr;
        return PolyData(poly != nullptr ? static_cast<FVizPolyData*>(fviz_retain(poly)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// ThresholdFilter - keeps cells whose scalar is within [min, max].
// ---------------------------------------------------------------------------
class ThresholdFilter : public Filter {
public:
    ThresholdFilter() = default;
    explicit ThresholdFilter(FVizFilter* owned) : Filter(owned) {}
    explicit ThresholdFilter(void* owned) : Filter(owned) {}

    static ThresholdFilter create(const std::string& scalar_name, double minimum, double maximum)
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_threshold_filter_create(scalar_name.c_str(), minimum, maximum, &filter));
        return ThresholdFilter(filter);
    }

    void setScalarName(const std::string& name) { detail::checkResult(fviz_threshold_filter_set_scalar_name(ptr_, name.c_str())); }
    void setRange(double minimum, double maximum) { detail::checkResult(fviz_threshold_filter_set_range(ptr_, minimum, maximum)); }
};

// ---------------------------------------------------------------------------
// WarpFilter - displaces points by a vector field.
// ---------------------------------------------------------------------------
class WarpFilter : public Filter {
public:
    WarpFilter() = default;
    explicit WarpFilter(FVizFilter* owned) : Filter(owned) {}
    explicit WarpFilter(void* owned) : Filter(owned) {}

    static WarpFilter create(const std::string& vector_name, double scale)
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_warp_filter_create(vector_name.c_str(), scale, &filter));
        return WarpFilter(filter);
    }

    void setVectorName(const std::string& name) { detail::checkResult(fviz_warp_filter_set_vector_name(ptr_, name.c_str())); }
    void setScale(double scale) { detail::checkResult(fviz_warp_filter_set_scale(ptr_, scale)); }
};

// ---------------------------------------------------------------------------
// CellDataToPointFilter - averages cell fields onto points.
// ---------------------------------------------------------------------------
class CellDataToPointFilter : public Filter {
public:
    CellDataToPointFilter() = default;
    explicit CellDataToPointFilter(FVizFilter* owned) : Filter(owned) {}
    explicit CellDataToPointFilter(void* owned) : Filter(owned) {}

    static CellDataToPointFilter create()
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_cell_data_to_point_filter_create(&filter));
        return CellDataToPointFilter(filter);
    }
};

// ---------------------------------------------------------------------------
// SurfaceFilter - extracts the boundary surface of an unstructured grid.
// ---------------------------------------------------------------------------
class SurfaceFilter : public Filter {
public:
    SurfaceFilter() = default;
    explicit SurfaceFilter(FVizFilter* owned) : Filter(owned) {}
    explicit SurfaceFilter(void* owned) : Filter(owned) {}

    static SurfaceFilter create(bool transfer_scalars = true)
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_surface_filter_create(detail::fbool(transfer_scalars), &filter));
        return SurfaceFilter(filter);
    }

    void setTransferScalars(bool transfer) { detail::checkResult(fviz_surface_filter_set_transfer_scalars(ptr_, detail::fbool(transfer))); }
};

// ---------------------------------------------------------------------------
// SliceFilter - cuts a grid with a plane, producing a PolyData surface.
// ---------------------------------------------------------------------------
class SliceFilter : public Filter {
public:
    SliceFilter() = default;
    explicit SliceFilter(FVizFilter* owned) : Filter(owned) {}
    explicit SliceFilter(void* owned) : Filter(owned) {}

    static SliceFilter create(Plane plane)
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_slice_filter_create(plane, &filter));
        return SliceFilter(filter);
    }

    void setPlane(Plane plane) { detail::checkResult(fviz_slice_filter_set_plane(ptr_, plane)); }
};

// ---------------------------------------------------------------------------
// TransformFilter - applies an affine transform to grid points.
// ---------------------------------------------------------------------------
class TransformFilter : public Filter {
public:
    TransformFilter() = default;
    explicit TransformFilter(FVizFilter* owned) : Filter(owned) {}
    explicit TransformFilter(void* owned) : Filter(owned) {}

    static TransformFilter create(Transform& transform)
    {
        FVizFilter* filter = nullptr;
        detail::checkResult(fviz_transform_filter_create(transform.get(), &filter));
        return TransformFilter(filter);
    }

    void setTransform(Transform& transform) { detail::checkResult(fviz_transform_filter_set_transform(ptr_, transform.get())); }
    Transform transform() const
    {
        FVizTransform* t = ptr_ ? fviz_transform_filter_transform(ptr_) : nullptr;
        return Transform(t != nullptr ? static_cast<FVizTransform*>(fviz_retain(t)) : nullptr);
    }
};

} // namespace fviz

#endif // FVIZ_CPP_FILTER_HPP
