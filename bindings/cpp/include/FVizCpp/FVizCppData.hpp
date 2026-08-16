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
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizCellArray.h>

#include "FVizCppObject.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fviz {

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

    void clear() noexcept
    {
        if (ptr_ != nullptr) fviz_poly_data_clear(ptr_);
    }
};

} // namespace fviz

#endif // FVIZ_CPP_DATA_HPP
