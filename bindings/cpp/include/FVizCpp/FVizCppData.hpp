// FEAViz C++ binding - data objects.
//
// RAII wrappers over the FEAViz data classes used by a typical viewer:
// DataArray, AttributeSet, UnstructuredGrid, PolyData, Points, CellArray.
// All creation functions return an owning wrapper; accessors that return
// borrowed C pointers are wrapped with retained views where needed.

#ifndef FVIZ_CPP_DATA_HPP
#define FVIZ_CPP_DATA_HPP

#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Math/FVizTransform.h>

#include "FVizCppObject.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fviz {

// PolyData is defined later in this header; UnstructuredGrid methods that
// return it need the forward declaration.
class PolyData;

// ---------------------------------------------------------------------------
// DataArray
// ---------------------------------------------------------------------------
class DataArray : public Object<FVizDataArray> {
public:
    DataArray() = default;
    explicit DataArray(FVizDataArray* owned) : Object<FVizDataArray>(owned) {}
    explicit DataArray(void* owned) : Object<FVizDataArray>(owned) {}

    static DataArray create(FVizDataType type, uint32_t components)
    {
        FVizDataArray* array = nullptr;
        detail::checkResult(fviz_data_array_create(type, components, &array));
        return DataArray(array);
    }

    static DataArray createFloat32(uint32_t components = 1u) { return create(FVIZ_DATA_FLOAT32, components); }
    static DataArray createFloat64(uint32_t components = 1u) { return create(FVIZ_DATA_FLOAT64, components); }
    static DataArray createInt32(uint32_t components = 1u) { return create(FVIZ_DATA_INT32, components); }
    static DataArray createUint32(uint32_t components = 1u) { return create(FVIZ_DATA_UINT32, components); }
    static DataArray createUint64(uint32_t components = 1u) { return create(FVIZ_DATA_UINT64, components); }

    FVizDataType type() const noexcept { return ptr_ != nullptr ? fviz_data_array_type(ptr_) : FVIZ_DATA_FLOAT32; }
    uint32_t components() const noexcept { return ptr_ != nullptr ? fviz_data_array_components(ptr_) : 0u; }
    FVizSize size() const noexcept { return ptr_ != nullptr ? fviz_data_array_tuple_count(ptr_) : 0u; }
    FVizSize tupleCount() const noexcept { return size(); }
    bool empty() const noexcept { return size() == 0u; }

    void resize(FVizSize tuple_count)
    {
        detail::checkResult(fviz_data_array_resize(ptr_, tuple_count));
    }

    void setTuple(FVizSize index, const void* tuple)
    {
        detail::checkResult(fviz_data_array_set_tuple(ptr_, index, tuple));
    }

    void setComponent(FVizSize tuple_index, uint32_t component, double value)
    {
        detail::checkResult(fviz_data_array_set_component(ptr_, tuple_index, component, value));
    }

    double component(FVizSize tuple_index, uint32_t component = 0u) const
    {
        double value = 0.0;
        detail::checkResult(fviz_data_array_get_component(ptr_, tuple_index, component, &value));
        return value;
    }

    // Range over one component; component == -1 computes vector magnitude.
    void range(int32_t component, double& out_minimum, double& out_maximum, bool ignore_non_finite = true) const
    {
        detail::checkResult(fviz_data_array_get_range(
            ptr_, component, detail::fbool(ignore_non_finite), &out_minimum, &out_maximum));
    }

    void* data() noexcept { return ptr_ != nullptr ? fviz_data_array_data(ptr_) : nullptr; }
    const void* data() const noexcept { return ptr_ != nullptr ? fviz_data_array_const_data(ptr_) : nullptr; }

    // Typed access for simple single-component arrays.
    template <typename T>
    T value(FVizSize index) const
    {
        T out{};
        if (sizeof(T) == 4u)
            out = static_cast<T>(component(index, 0u));
        else
            out = static_cast<T>(component(index, 0u));
        return out;
    }
};

// ---------------------------------------------------------------------------
// AttributeSet
// ---------------------------------------------------------------------------
class AttributeSet : public Object<FVizAttributeSet> {
public:
    AttributeSet() = default;
    explicit AttributeSet(FVizAttributeSet* owned) : Object<FVizAttributeSet>(owned) {}
    explicit AttributeSet(void* owned) : Object<FVizAttributeSet>(owned) {}

    static AttributeSet create()
    {
        FVizAttributeSet* set = nullptr;
        detail::checkResult(fviz_attribute_set_create(&set));
        return AttributeSet(set);
    }

    FVizSize count() const noexcept { return ptr_ != nullptr ? fviz_attribute_set_count(ptr_) : 0u; }

    const char* nameAt(FVizSize index) const noexcept
    {
        return ptr_ != nullptr ? fviz_attribute_set_name_at(ptr_, index) : nullptr;
    }

    // Retained array view (owns its own reference).
    DataArray arrayAt(FVizSize index) const
    {
        FVizDataArray* array = ptr_ != nullptr ? fviz_attribute_set_array_at(ptr_, index) : nullptr;
        return DataArray(array != nullptr ? static_cast<FVizDataArray*>(fviz_retain(array)) : nullptr);
    }

    // Retained lookup by name.
    DataArray get(const char* name) const
    {
        FVizDataArray* array = ptr_ != nullptr ? fviz_attribute_set_get(ptr_, name) : nullptr;
        return DataArray(array != nullptr ? static_cast<FVizDataArray*>(fviz_retain(array)) : nullptr);
    }

    bool has(const char* name) const noexcept { return get(name).get() != nullptr; }

    void add(const char* name, FVizDataArray* array)
    {
        detail::checkResult(fviz_attribute_set_add(ptr_, name, array));
    }

    void remove(const char* name)
    {
        detail::checkResult(fviz_attribute_set_remove(ptr_, name));
    }

    void clear() noexcept
    {
        if (ptr_ != nullptr) fviz_attribute_set_clear(ptr_);
    }

    void setActive(FVizAttributeRole role, const char* name)
    {
        detail::checkResult(fviz_attribute_set_set_active(ptr_, role, name));
    }
    DataArray active(FVizAttributeRole role) const
    {
        FVizDataArray* array = ptr_ != nullptr
            ? const_cast<FVizDataArray*>(fviz_attribute_set_const_active(ptr_, role))
            : nullptr;
        return DataArray(array != nullptr ? static_cast<FVizDataArray*>(fviz_retain(array)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// Points
// ---------------------------------------------------------------------------
class Points : public Object<FVizPoints> {
public:
    Points() = default;
    explicit Points(FVizPoints* owned) : Object<FVizPoints>(owned) {}
    explicit Points(void* owned) : Object<FVizPoints>(owned) {}

    static Points create()
    {
        FVizPoints* points = nullptr;
        detail::checkResult(fviz_points_create(&points));
        return Points(points);
    }

    FVizSize count() const noexcept { return ptr_ != nullptr ? fviz_points_count(ptr_) : 0u; }

    void append(Vec3 point)
    {
        detail::checkResult(fviz_points_append(ptr_, point, nullptr));
    }

    void append(Vec3 point, uint32_t& out_id)
    {
        detail::checkResult(fviz_points_append(ptr_, point, &out_id));
    }

    const FVizVec3* data() const noexcept { return ptr_ != nullptr ? fviz_points_data(ptr_) : nullptr; }

    Bounds bounds() const noexcept { return ptr_ != nullptr ? Bounds(fviz_points_bounds(ptr_)) : Bounds(); }
};

// ---------------------------------------------------------------------------
// CellArray
// ---------------------------------------------------------------------------
class CellArray : public Object<FVizCellArray> {
public:
    CellArray() = default;
    explicit CellArray(FVizCellArray* owned) : Object<FVizCellArray>(owned) {}
    explicit CellArray(void* owned) : Object<FVizCellArray>(owned) {}

    static CellArray create()
    {
        FVizCellArray* cells = nullptr;
        detail::checkResult(fviz_cell_array_create(&cells));
        return CellArray(cells);
    }

    FVizSize count() const noexcept { return ptr_ != nullptr ? fviz_cell_array_count(ptr_) : 0u; }

    void append(FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
    {
        detail::checkResult(fviz_cell_array_append(ptr_, type, point_count, point_ids));
    }

    FVizCellType cellType(FVizSize cell_id) const noexcept
    {
        return ptr_ != nullptr ? fviz_cell_array_type(ptr_, cell_id) : FVIZ_CELL_VERTEX;
    }
};

// ---------------------------------------------------------------------------
// UnstructuredGrid
// ---------------------------------------------------------------------------
class UnstructuredGrid : public Object<FVizUnstructuredGrid> {
public:
    UnstructuredGrid() = default;
    explicit UnstructuredGrid(FVizUnstructuredGrid* owned) : Object<FVizUnstructuredGrid>(owned) {}
    explicit UnstructuredGrid(void* owned) : Object<FVizUnstructuredGrid>(owned) {}

    static UnstructuredGrid create()
    {
        FVizUnstructuredGrid* grid = nullptr;
        detail::checkResult(fviz_unstructured_grid_create(&grid));
        return UnstructuredGrid(grid);
    }

    FVizSize pointCount() const noexcept { return ptr_ != nullptr ? fviz_unstructured_grid_point_count(ptr_) : 0u; }
    FVizSize cellCount() const noexcept { return ptr_ != nullptr ? fviz_unstructured_grid_cell_count(ptr_) : 0u; }

    void addPoint(Vec3 point)
    {
        detail::checkResult(fviz_unstructured_grid_add_point(ptr_, point, nullptr));
    }

    void addPoint(Vec3 point, uint32_t& out_id)
    {
        detail::checkResult(fviz_unstructured_grid_add_point(ptr_, point, &out_id));
    }

    void addCell(FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
    {
        detail::checkResult(fviz_unstructured_grid_add_cell(ptr_, type, point_count, point_ids));
    }

    // Retained point/cell data sets.
    AttributeSet pointData() const
    {
        FVizAttributeSet* set = ptr_ != nullptr ? fviz_unstructured_grid_point_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }
    AttributeSet cellData() const
    {
        FVizAttributeSet* set = ptr_ != nullptr ? fviz_unstructured_grid_cell_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }
    AttributeSet fieldData() const
    {
        FVizAttributeSet* set = ptr_ != nullptr ? fviz_unstructured_grid_field_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }

    Bounds bounds() const noexcept { return ptr_ != nullptr ? Bounds(fviz_unstructured_grid_bounds(ptr_)) : Bounds(); }

    void validate() { detail::checkResult(fviz_unstructured_grid_validate(ptr_)); }

    void clear() noexcept
    {
        if (ptr_ != nullptr) fviz_unstructured_grid_clear(ptr_);
    }

    // Computes per-point gradients (VTK vtkGradientFilter compatible). Returns
    // a shallow grid with the gradient array added to point data.
    UnstructuredGrid gradient(const std::string& scalar_array_name, const std::string& output_name) const
    {
        FVizUnstructuredGrid* result = nullptr;
        detail::checkResult(fviz_unstructured_grid_gradient(ptr_, scalar_array_name.c_str(), output_name.c_str(), &result));
        return UnstructuredGrid(result);
    }

    // Per-cell derivatives (vtkCellDerivatives compatible): stores a 3*N
    // gradient per cell in cell data.
    UnstructuredGrid cellDerivatives(const std::string& scalar_array_name, const std::string& output_name) const
    {
        FVizUnstructuredGrid* result = nullptr;
        detail::checkResult(fviz_unstructured_grid_cell_derivatives(ptr_, scalar_array_name.c_str(), output_name.c_str(), &result));
        return UnstructuredGrid(result);
    }

    // Warps points by a scalar field (vtkWarpScalar compatible).
    UnstructuredGrid warpScalar(const std::string& scalar_array_name, double scale,
        const std::string& normal_array_name = "") const
    {
        FVizUnstructuredGrid* result = nullptr;
        detail::checkResult(fviz_unstructured_grid_warp_scalar(ptr_, scalar_array_name.c_str(), scale,
            normal_array_name.empty() ? nullptr : normal_array_name.c_str(), &result));
        return UnstructuredGrid(result);
    }

    // Integrates streamlines (vtkStreamTracer compatible). Returns a PolyData
    // of polylines. Defined after PolyData below.
    PolyData streamTracer(const std::string& vector_array_name,
        const std::vector<Vec3>& seed_points, double step_length, FVizSize max_steps) const;

    // Cuts a grid with multiple planes (vtkCutter compatible).
    PolyData cutter(const std::vector<Plane>& planes) const;

    // Extracts a 3-D iso-surface (vtkContourFilter compatible, marching tetra).
    PolyData isoSurface(const std::string& scalar_array_name, double iso_value) const;

    // Extracts the exterior surface (vtkDataSetSurfaceFilter compatible).
    PolyData extractSurface() const;
};

// ---------------------------------------------------------------------------
// PolyData
// ---------------------------------------------------------------------------
class PolyData : public Object<FVizPolyData> {
public:
    PolyData() = default;
    explicit PolyData(FVizPolyData* owned) : Object<FVizPolyData>(owned) {}
    explicit PolyData(void* owned) : Object<FVizPolyData>(owned) {}

    static PolyData create()
    {
        FVizPolyData* poly = nullptr;
        detail::checkResult(fviz_poly_data_create(&poly));
        return PolyData(poly);
    }

    FVizSize pointCount() const noexcept { return ptr_ != nullptr ? fviz_poly_data_point_count(ptr_) : 0u; }
    FVizSize triangleCount() const noexcept { return ptr_ != nullptr ? fviz_poly_data_triangle_count(ptr_) : 0u; }
    FVizSize lineCount() const noexcept { return ptr_ != nullptr ? fviz_poly_data_line_count(ptr_) : 0u; }
    FVizSize cellCount() const noexcept { return ptr_ != nullptr ? fviz_poly_data_cell_count(ptr_) : 0u; }

    void addPoint(Vec3 point)
    {
        detail::checkResult(fviz_poly_data_add_point(ptr_, point, nullptr));
    }

    void addTriangle(uint32_t a, uint32_t b, uint32_t c)
    {
        detail::checkResult(fviz_poly_data_add_triangle(ptr_, a, b, c));
    }

    void addQuad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        detail::checkResult(fviz_poly_data_add_quad(ptr_, a, b, c, d));
    }

    void addLine(uint32_t a, uint32_t b)
    {
        detail::checkResult(fviz_poly_data_add_line(ptr_, a, b));
    }

    void computeNormals() { detail::checkResult(fviz_poly_data_compute_normals(ptr_)); }

    void validate() { detail::checkResult(fviz_poly_data_validate(ptr_)); }

    const FVizVec3* points() const noexcept { return ptr_ != nullptr ? fviz_poly_data_points(ptr_) : nullptr; }
    const uint32_t* triangleIndices() const noexcept
    {
        return ptr_ != nullptr ? fviz_poly_data_triangle_indices(ptr_) : nullptr;
    }
    const uint32_t* lineIndices() const noexcept
    {
        return ptr_ != nullptr ? fviz_poly_data_line_indices(ptr_) : nullptr;
    }

    Bounds bounds() const noexcept { return ptr_ != nullptr ? Bounds(fviz_poly_data_bounds(ptr_)) : Bounds(); }

    AttributeSet pointData() const
    {
        FVizAttributeSet* set = ptr_ != nullptr ? fviz_poly_data_point_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }
    AttributeSet cellData() const
    {
        FVizAttributeSet* set = ptr_ != nullptr ? fviz_poly_data_cell_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }

    // Extracts all cell edges as line cells (vtkExtractEdges compatible).
    PolyData extractEdges() const
    {
        FVizPolyData* edges = nullptr;
        detail::checkResult(fviz_poly_data_extract_edges(ptr_, &edges));
        return PolyData(edges);
    }

    // 2D Delaunay triangulation of the points (vtkDelaunay2D compatible).
    PolyData delaunay2D() const
    {
        FVizPolyData* tris = nullptr;
        detail::checkResult(fviz_poly_data_delaunay_2d(ptr_, &tris));
        return PolyData(tris);
    }

    // Materializes glyphs at the points (vtkGlyph3D compatible).
    PolyData glyph3D(const std::string& scale_array_name = "",
        const std::string& orientation_array_name = "", double scale_factor = 1.0) const
    {
        FVizPolyData* glyphs = nullptr;
        detail::checkResult(fviz_poly_data_glyph_3d(ptr_,
            scale_array_name.empty() ? nullptr : scale_array_name.c_str(),
            orientation_array_name.empty() ? nullptr : orientation_array_name.c_str(),
            scale_factor, &glyphs));
        return PolyData(glyphs);
    }

    void clear() noexcept
    {
        if (ptr_ != nullptr) fviz_poly_data_clear(ptr_);
    }
};

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------
class Transform : public Object<FVizTransform> {
public:
    Transform() = default;
    explicit Transform(FVizTransform* owned) : Object<FVizTransform>(owned) {}
    explicit Transform(void* owned) : Object<FVizTransform>(owned) {}

    static Transform create()
    {
        FVizTransform* transform = nullptr;
        detail::checkResult(fviz_transform_create(&transform));
        return Transform(transform);
    }

    void identity() noexcept { if (ptr_) fviz_transform_identity(ptr_); }
    void setMatrix(const Mat4& matrix) noexcept { if (ptr_) fviz_transform_set_matrix(ptr_, matrix); }
    Mat4 matrix() const noexcept { return ptr_ ? Mat4(fviz_transform_matrix(ptr_)) : Mat4(); }
    void translate(Vec3 translation) noexcept { if (ptr_) fviz_transform_translate(ptr_, translation); }
    void scale(Vec3 scale_) noexcept { if (ptr_) fviz_transform_scale(ptr_, scale_); }
    void rotate(Quat rotation) noexcept { if (ptr_) fviz_transform_rotate(ptr_, rotation); }
    Vec3 transformPoint(Vec3 point) const noexcept { return ptr_ ? Vec3(fviz_transform_point(ptr_, point)) : Vec3(); }
    Vec3 transformVector(Vec3 vector) const noexcept { return ptr_ ? Vec3(fviz_transform_vector(ptr_, vector)) : Vec3(); }
};

// ---------------------------------------------------------------------------
// ImageData - rectilinear voxel grid on a regular lattice.
// ---------------------------------------------------------------------------
class ImageData : public Object<FVizImageData> {
public:
    ImageData() = default;
    explicit ImageData(FVizImageData* owned) : Object<FVizImageData>(owned) {}
    explicit ImageData(void* owned) : Object<FVizImageData>(owned) {}

    static ImageData create()
    {
        FVizImageData* image = nullptr;
        detail::checkResult(fviz_image_data_create(&image));
        return ImageData(image);
    }

    // Extent is inclusive [xmin,xmax,ymin,ymax,zmin,zmax].
    void setExtent(const int64_t extent[6]) { detail::checkResult(fviz_image_data_set_extent(ptr_, extent)); }
    void setExtent(int64_t xmin, int64_t xmax, int64_t ymin, int64_t ymax, int64_t zmin, int64_t zmax)
    {
        const int64_t extent[6] = {xmin, xmax, ymin, ymax, zmin, zmax};
        setExtent(extent);
    }
    std::array<int64_t, 6> extent() const noexcept
    {
        std::array<int64_t, 6> out{};
        if (ptr_) fviz_image_data_extent(ptr_, out.data());
        return out;
    }
    std::array<FVizSize, 3> dimensions() const noexcept
    {
        std::array<FVizSize, 3> out{};
        if (ptr_) fviz_image_data_dimensions(ptr_, out.data());
        return out;
    }
    uint32_t dimension() const noexcept { return ptr_ ? fviz_image_data_dimension(ptr_) : 0u; }

    void setOrigin(double x, double y, double z) { const double o[3] = {x, y, z}; detail::checkResult(fviz_image_data_set_origin(ptr_, o)); }
    void setSpacing(double x, double y, double z) { const double s[3] = {x, y, z}; detail::checkResult(fviz_image_data_set_spacing(ptr_, s)); }

    FVizSize pointCount() const noexcept { return ptr_ ? fviz_image_data_point_count(ptr_) : 0u; }
    FVizSize cellCount() const noexcept { return ptr_ ? fviz_image_data_cell_count(ptr_) : 0u; }
    FVizCellType cellType() const noexcept { return ptr_ ? fviz_image_data_cell_type(ptr_) : FVIZ_CELL_HEXAHEDRON; }
    Bounds bounds() const noexcept { return ptr_ ? Bounds(fviz_image_data_bounds(ptr_)) : Bounds(); }

    AttributeSet pointData() const
    {
        FVizAttributeSet* set = ptr_ ? fviz_image_data_point_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }
    AttributeSet cellData() const
    {
        FVizAttributeSet* set = ptr_ ? fviz_image_data_cell_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }

    // Allocates a point-scalar array of the given size and returns a retained view.
    DataArray allocatePointScalars(const char* name, FVizDataType type, uint32_t components)
    {
        FVizDataArray* array = nullptr;
        detail::checkResult(fviz_image_data_allocate_point_scalars(ptr_, name, type, components, &array));
        return DataArray(array);
    }

    void validate() { detail::checkResult(fviz_image_data_validate(ptr_)); }
    void clear() noexcept { if (ptr_) fviz_image_data_clear(ptr_); }
};

// ---------------------------------------------------------------------------
// StructuredGrid - explicit points on an implicit structured extent.
// ---------------------------------------------------------------------------
class StructuredGrid : public Object<FVizStructuredGrid> {
public:
    StructuredGrid() = default;
    explicit StructuredGrid(FVizStructuredGrid* owned) : Object<FVizStructuredGrid>(owned) {}
    explicit StructuredGrid(void* owned) : Object<FVizStructuredGrid>(owned) {}

    static StructuredGrid create()
    {
        FVizStructuredGrid* grid = nullptr;
        detail::checkResult(fviz_structured_grid_create(&grid));
        return StructuredGrid(grid);
    }

    void setExtent(const int64_t extent[6]) { detail::checkResult(fviz_structured_grid_set_extent(ptr_, extent)); }
    std::array<int64_t, 6> extent() const noexcept
    {
        std::array<int64_t, 6> out{};
        if (ptr_) fviz_structured_grid_extent(ptr_, out.data());
        return out;
    }
    std::array<FVizSize, 3> dimensions() const noexcept
    {
        std::array<FVizSize, 3> out{};
        if (ptr_) fviz_structured_grid_dimensions(ptr_, out.data());
        return out;
    }
    uint32_t dimension() const noexcept { return ptr_ ? fviz_structured_grid_dimension(ptr_) : 0u; }
    FVizSize pointCount() const noexcept { return ptr_ ? fviz_structured_grid_point_count(ptr_) : 0u; }
    FVizSize cellCount() const noexcept { return ptr_ ? fviz_structured_grid_cell_count(ptr_) : 0u; }
    FVizCellType cellType() const noexcept { return ptr_ ? fviz_structured_grid_cell_type(ptr_) : FVIZ_CELL_HEXAHEDRON; }

    void setPoints(const FVizVec3* points, FVizSize point_count)
    {
        detail::checkResult(fviz_structured_grid_set_points(ptr_, points, point_count));
    }
    void setPoint(FVizSize point_id, Vec3 point) { detail::checkResult(fviz_structured_grid_set_point(ptr_, point_id, point)); }
    const FVizVec3* points() const noexcept { return ptr_ ? fviz_structured_grid_points(ptr_) : nullptr; }
    Vec3 point(FVizId point_id) const
    {
        FVizVec3 out;
        detail::checkResult(fviz_structured_grid_point(ptr_, point_id, &out));
        return out;
    }
    Bounds bounds() const noexcept { return ptr_ ? Bounds(fviz_structured_grid_bounds(ptr_)) : Bounds(); }

    AttributeSet pointData() const
    {
        FVizAttributeSet* set = ptr_ ? fviz_structured_grid_point_data(ptr_) : nullptr;
        return AttributeSet(set != nullptr ? static_cast<FVizAttributeSet*>(fviz_retain(set)) : nullptr);
    }

    void validate() { detail::checkResult(fviz_structured_grid_validate(ptr_)); }
    void clear() noexcept { if (ptr_) fviz_structured_grid_clear(ptr_); }
};

// ---------------------------------------------------------------------------
// RectilinearGrid - axis-aligned coordinates along each axis.
// ---------------------------------------------------------------------------
class RectilinearGrid : public Object<FVizRectilinearGrid> {
public:
    RectilinearGrid() = default;
    explicit RectilinearGrid(FVizRectilinearGrid* owned) : Object<FVizRectilinearGrid>(owned) {}
    explicit RectilinearGrid(void* owned) : Object<FVizRectilinearGrid>(owned) {}

    static RectilinearGrid create()
    {
        FVizRectilinearGrid* grid = nullptr;
        detail::checkResult(fviz_rectilinear_grid_create(&grid));
        return RectilinearGrid(grid);
    }

    void setExtent(const int64_t extent[6]) { detail::checkResult(fviz_rectilinear_grid_set_extent(ptr_, extent)); }
    std::array<FVizSize, 3> dimensions() const noexcept
    {
        std::array<FVizSize, 3> out{};
        if (ptr_) fviz_rectilinear_grid_dimensions(ptr_, out.data());
        return out;
    }
    uint32_t dimension() const noexcept { return ptr_ ? fviz_rectilinear_grid_dimension(ptr_) : 0u; }
    FVizSize pointCount() const noexcept { return ptr_ ? fviz_rectilinear_grid_point_count(ptr_) : 0u; }
    FVizSize cellCount() const noexcept { return ptr_ ? fviz_rectilinear_grid_cell_count(ptr_) : 0u; }
    FVizCellType cellType() const noexcept { return ptr_ ? fviz_rectilinear_grid_cell_type(ptr_) : FVIZ_CELL_HEXAHEDRON; }

    void setCoordinates(uint32_t axis, FVizDataArray* coordinates)
    {
        detail::checkResult(fviz_rectilinear_grid_set_coordinates(ptr_, axis, coordinates));
    }
    void setCoordinateValues(uint32_t axis, const double* values, FVizSize count)
    {
        detail::checkResult(fviz_rectilinear_grid_set_coordinate_values(ptr_, axis, values, count));
    }
    DataArray coordinates(uint32_t axis) const
    {
        FVizDataArray* array = ptr_ ? fviz_rectilinear_grid_coordinates(ptr_, axis) : nullptr;
        return DataArray(array != nullptr ? static_cast<FVizDataArray*>(fviz_retain(array)) : nullptr);
    }
    Bounds bounds() const noexcept { return ptr_ ? Bounds(fviz_rectilinear_grid_bounds(ptr_)) : Bounds(); }

    void validate() { detail::checkResult(fviz_rectilinear_grid_validate(ptr_)); }
    void clear() noexcept { if (ptr_) fviz_rectilinear_grid_clear(ptr_); }
};

// Defined here so PolyData is complete.
inline PolyData UnstructuredGrid::streamTracer(const std::string& vector_array_name,
    const std::vector<Vec3>& seed_points, double step_length, FVizSize max_steps) const
{
    FVizPolyData* lines = nullptr;
    std::vector<FVizVec3> raw_seeds;
    raw_seeds.reserve(seed_points.size());
    for (const Vec3& p : seed_points) raw_seeds.push_back(p);
    detail::checkResult(fviz_unstructured_grid_stream_tracer(ptr_, vector_array_name.c_str(),
        raw_seeds.data(), raw_seeds.size(), step_length, max_steps, &lines));
    return PolyData(lines);
}

inline PolyData UnstructuredGrid::cutter(const std::vector<Plane>& planes) const
{
    std::vector<FVizPlane> raw_planes;
    raw_planes.reserve(planes.size());
    for (const Plane& p : planes) raw_planes.push_back(p);
    FVizPolyData* cut = nullptr;
    detail::checkResult(fviz_unstructured_grid_cutter(ptr_, raw_planes.data(), raw_planes.size(), &cut));
    return PolyData(cut);
}

inline PolyData UnstructuredGrid::isoSurface(const std::string& scalar_array_name, double iso_value) const
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_unstructured_grid_iso_surface(ptr_, scalar_array_name.c_str(), iso_value, &surface));
    return PolyData(surface);
}

inline PolyData UnstructuredGrid::extractSurface() const
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_unstructured_grid_extract_surface(ptr_, &surface));
    return PolyData(surface);
}

} // namespace fviz

#endif // FVIZ_CPP_DATA_HPP
