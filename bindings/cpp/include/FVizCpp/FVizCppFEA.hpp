// FEAViz C++ binding - FEA module.
//
// RAII wrappers over the optional FEAViz::FEA module: result database / steps /
// frames / fields, primary-variable evaluation, deformed-shape control, the
// Abaqus-style scalar bar actor, and contour helpers.
//
// Link against FEAViz::FEA (which links FEAViz::Core). These types are only
// available when FVIZ_BUILD_FEA is enabled.

#ifndef FVIZ_CPP_FEA_HPP
#define FVIZ_CPP_FEA_HPP

#include <FViz/FEA/FVizResultDatabase.h>
#include <FViz/FEA/FVizResultField.h>
#include <FViz/FEA/FVizPrimaryVariable.h>
#include <FViz/FEA/FVizDeformedShape.h>
#include <FViz/FEA/FVizScalarBarActor.h>
#include <FViz/FEA/FVizVisualization.h>
#include <FViz/FEA/FVizIntegrationPointData.h>

#include "FVizCppObject.hpp"
#include "FVizCppData.hpp"
#include "FVizCppRendering.hpp"

#include <string>

namespace fviz {

// Forward declaration; Frame's Field accessors are defined after Field below.
class Field;

// ---------------------------------------------------------------------------
// HistorySeries - a single XY history curve.
// ---------------------------------------------------------------------------
class HistorySeries : public Object<FVizFEAHistorySeries> {
public:
    HistorySeries() = default;
    explicit HistorySeries(FVizFEAHistorySeries* owned) : Object<FVizFEAHistorySeries>(owned) {}
    explicit HistorySeries(void* owned) : Object<FVizFEAHistorySeries>(owned) {}

    static HistorySeries create(const std::string& name, const std::string& description = "")
    {
        FVizFEAHistorySeries* series = nullptr;
        detail::checkResult(fviz_fea_history_series_create(name.c_str(), description.c_str(), &series));
        return HistorySeries(series);
    }

    const char* name() const noexcept { return ptr_ ? fviz_fea_history_series_name(ptr_) : nullptr; }
    FVizSize count() const noexcept { return ptr_ ? fviz_fea_history_series_count(ptr_) : 0u; }
    void append(double frame_value, double value) { detail::checkResult(fviz_fea_history_series_append(ptr_, frame_value, value)); }
    double interpolate(double frame_value) const
    {
        double value = 0.0;
        detail::checkResult(fviz_fea_history_series_interpolate(ptr_, frame_value, &value));
        return value;
    }
};

// ---------------------------------------------------------------------------
// HistoryRegion - a named collection of history series.
// ---------------------------------------------------------------------------
class HistoryRegion : public Object<FVizFEAHistoryRegion> {
public:
    HistoryRegion() = default;
    explicit HistoryRegion(FVizFEAHistoryRegion* owned) : Object<FVizFEAHistoryRegion>(owned) {}
    explicit HistoryRegion(void* owned) : Object<FVizFEAHistoryRegion>(owned) {}

    static HistoryRegion create(const std::string& name, const std::string& description = "")
    {
        FVizFEAHistoryRegion* region = nullptr;
        detail::checkResult(fviz_fea_history_region_create(name.c_str(), description.c_str(), &region));
        return HistoryRegion(region);
    }

    void addSeries(HistorySeries& series) { detail::checkResult(fviz_fea_history_region_add_series(ptr_, series.get())); }
    HistorySeries series(const std::string& name) const
    {
        FVizFEAHistorySeries* s = ptr_ ? fviz_fea_history_region_series(ptr_, name.c_str()) : nullptr;
        return HistorySeries(s != nullptr ? static_cast<FVizFEAHistorySeries*>(fviz_retain(s)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// Frame - one analysis increment with result fields.
// ---------------------------------------------------------------------------
class Frame : public Object<FVizFEAFrame> {
public:
    Frame() = default;
    explicit Frame(FVizFEAFrame* owned) : Object<FVizFEAFrame>(owned) {}
    explicit Frame(void* owned) : Object<FVizFEAFrame>(owned) {}

    static Frame create(int64_t frame_id, double frame_value = 0.0, const std::string& description = "")
    {
        FVizFEAFrameInfo info;
        fviz_fea_frame_info_initialize(&info);
        info.frame_id = frame_id;
        info.frame_value = frame_value;
        info.description = description.c_str();
        FVizFEAFrame* frame = nullptr;
        detail::checkResult(fviz_fea_frame_create(&info, &frame));
        return Frame(frame);
    }

    int64_t id() const noexcept { return ptr_ ? fviz_fea_frame_id(ptr_) : 0; }
    double value() const noexcept { return ptr_ ? fviz_fea_frame_value(ptr_) : 0.0; }
    const char* description() const noexcept { return ptr_ ? fviz_fea_frame_description(ptr_) : nullptr; }
    FVizSize fieldCount() const noexcept { return ptr_ ? fviz_fea_frame_field_count(ptr_) : 0u; }
    void addField(Field& field);
    Field field(const std::string& name) const;
};

// ---------------------------------------------------------------------------
// Field - a named result field with typed blocks.
// ---------------------------------------------------------------------------
class Field : public Object<FVizFEAField> {
public:
    Field() = default;
    explicit Field(FVizFEAField* owned) : Object<FVizFEAField>(owned) {}
    explicit Field(void* owned) : Object<FVizFEAField>(owned) {}

    static Field create(const std::string& name, const std::string& description, FVizFEAFieldType field_type)
    {
        FVizFEAField* field = nullptr;
        detail::checkResult(fviz_fea_field_create(name.c_str(), description.c_str(), field_type, &field));
        return Field(field);
    }

    const char* name() const noexcept { return ptr_ ? fviz_fea_field_name(ptr_) : nullptr; }
    FVizFEAFieldType type() const noexcept { return ptr_ ? fviz_fea_field_type(ptr_) : FVIZ_FEA_FIELD_SCALAR; }
    FVizSize componentCount() const noexcept { return ptr_ ? fviz_fea_field_component_count(ptr_) : 0u; }
    const char* componentLabel(FVizSize component) const noexcept { return ptr_ ? fviz_fea_field_component_label(ptr_, component) : nullptr; }

    void setComponentLabels(const std::vector<std::string>& labels)
    {
        std::vector<const char*> raw;
        raw.reserve(labels.size());
        for (const auto& label : labels) raw.push_back(label.c_str());
        detail::checkResult(fviz_fea_field_set_component_labels(ptr_, raw.data(), raw.size()));
    }

    void addBlock(const char* instance_name, FVizFEAResultPosition position,
        FVizDataArray* entity_ids, FVizDataArray* local_ids, FVizDataArray* values)
    {
        FVizFEAFieldBlockDescriptor descriptor;
        fviz_fea_field_block_descriptor_initialize(&descriptor);
        descriptor.instance_name = instance_name;
        descriptor.position = position;
        descriptor.entity_ids = entity_ids;
        descriptor.local_ids = local_ids;
        descriptor.values = values;
        detail::checkResult(fviz_fea_field_add_block(ptr_, &descriptor, nullptr));
    }

    FVizSize blockCount() const noexcept { return ptr_ ? fviz_fea_field_block_count(ptr_) : 0u; }
    DataArray blockValues(FVizSize block_index) const
    {
        FVizDataArray* values = ptr_ ? fviz_fea_field_block_values(ptr_, block_index) : nullptr;
        return DataArray(values != nullptr ? static_cast<FVizDataArray*>(fviz_retain(values)) : nullptr);
    }
    const FVizDataArray* constBlockValues(FVizSize block_index) const noexcept
    {
        return ptr_ ? fviz_fea_field_block_const_values(ptr_, block_index) : nullptr;
    }

    DataArray evaluateInvariant(FVizSize block_index, FVizFEAInvariant invariant)
    {
        FVizDataArray* values = nullptr;
        detail::checkResult(fviz_fea_field_evaluate_invariant(ptr_, block_index, invariant, &values));
        return DataArray(values);
    }
};

// ---------------------------------------------------------------------------
// Step - a named analysis step holding frames and history regions.
// ---------------------------------------------------------------------------
class Step : public Object<FVizFEAStep> {
public:
    Step() = default;
    explicit Step(FVizFEAStep* owned) : Object<FVizFEAStep>(owned) {}
    explicit Step(void* owned) : Object<FVizFEAStep>(owned) {}

    static Step create(const std::string& name, const std::string& description,
        FVizFEAStepDomain domain = FVIZ_FEA_STEP_TIME, double time_period = 1.0)
    {
        FVizFEAStep* step = nullptr;
        detail::checkResult(fviz_fea_step_create(name.c_str(), description.c_str(), domain, time_period, &step));
        return Step(step);
    }

    const char* name() const noexcept { return ptr_ ? fviz_fea_step_name(ptr_) : nullptr; }
    FVizFEAStepDomain domain() const noexcept { return ptr_ ? fviz_fea_step_domain(ptr_) : FVIZ_FEA_STEP_TIME; }
    double timePeriod() const noexcept { return ptr_ ? fviz_fea_step_time_period(ptr_) : 0.0; }
    FVizSize frameCount() const noexcept { return ptr_ ? fviz_fea_step_frame_count(ptr_) : 0u; }

    void addFrame(Frame& frame) { detail::checkResult(fviz_fea_step_add_frame(ptr_, frame.get(), nullptr)); }
    Frame frameAt(FVizSize index) const
    {
        FVizFEAFrame* frame = ptr_ ? fviz_fea_step_frame(ptr_, index) : nullptr;
        return Frame(frame != nullptr ? static_cast<FVizFEAFrame*>(fviz_retain(frame)) : nullptr);
    }

    void addHistoryRegion(HistoryRegion& region) { detail::checkResult(fviz_fea_step_add_history_region(ptr_, region.get())); }
};

// ---------------------------------------------------------------------------
// ResultDatabase - top-level container of steps.
// ---------------------------------------------------------------------------
class ResultDatabase : public Object<FVizFEAResultDatabase> {
public:
    ResultDatabase() = default;
    explicit ResultDatabase(FVizFEAResultDatabase* owned) : Object<FVizFEAResultDatabase>(owned) {}
    explicit ResultDatabase(void* owned) : Object<FVizFEAResultDatabase>(owned) {}

    static ResultDatabase create()
    {
        FVizFEAResultDatabase* database = nullptr;
        detail::checkResult(fviz_fea_result_database_create(&database));
        return ResultDatabase(database);
    }

    FVizSize stepCount() const noexcept { return ptr_ ? fviz_fea_result_database_step_count(ptr_) : 0u; }
    void addStep(Step& step) { detail::checkResult(fviz_fea_result_database_add_step(ptr_, step.get(), nullptr)); }
    Step step(const std::string& name) const
    {
        FVizFEAStep* step = ptr_ ? fviz_fea_result_database_step(ptr_, name.c_str()) : nullptr;
        return Step(step != nullptr ? static_cast<FVizFEAStep*>(fviz_retain(step)) : nullptr);
    }
    Step stepAt(FVizSize index) const
    {
        FVizFEAStep* step = ptr_ ? fviz_fea_result_database_step_at(ptr_, index) : nullptr;
        return Step(step != nullptr ? static_cast<FVizFEAStep*>(fviz_retain(step)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// PrimaryVariable - result evaluation (component/invariant + averaging).
// ---------------------------------------------------------------------------
class PrimaryVariableEvaluator : public Object<FVizFEAPrimaryVariableEvaluator> {
public:
    PrimaryVariableEvaluator() = default;
    explicit PrimaryVariableEvaluator(FVizFEAPrimaryVariableEvaluator* owned) : Object<FVizFEAPrimaryVariableEvaluator>(owned) {}
    explicit PrimaryVariableEvaluator(void* owned) : Object<FVizFEAPrimaryVariableEvaluator>(owned) {}

    static PrimaryVariableEvaluator create()
    {
        FVizFEAPrimaryVariableEvaluator* evaluator = nullptr;
        detail::checkResult(fviz_fea_primary_variable_evaluator_create(&evaluator));
        return PrimaryVariableEvaluator(evaluator);
    }

    void clearCache() noexcept { if (ptr_) fviz_fea_primary_variable_evaluator_clear_cache(ptr_); }
};

// ---------------------------------------------------------------------------
// PrimaryVariableResult - evaluated values/ids ready for presentation.
// ---------------------------------------------------------------------------
class PrimaryVariableResult : public Object<FVizFEAPrimaryVariableResult> {
public:
    PrimaryVariableResult() = default;
    explicit PrimaryVariableResult(FVizFEAPrimaryVariableResult* owned) : Object<FVizFEAPrimaryVariableResult>(owned) {}
    explicit PrimaryVariableResult(void* owned) : Object<FVizFEAPrimaryVariableResult>(owned) {}

    FVizFEAResultPosition sourcePosition() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_source_position(ptr_) : FVIZ_FEA_POSITION_UNKNOWN; }
    FVizFEAResultPosition targetPosition() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_target_position(ptr_) : FVIZ_FEA_POSITION_UNKNOWN; }
    FVizFEADisplayAssociation association() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_association(ptr_) : FVIZ_FEA_DISPLAY_ASSOCIATION_NONE; }

    const FVizDataArray* rawValues() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_raw_values(ptr_) : nullptr; }
    const FVizDataArray* displayValues() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_display_values(ptr_) : nullptr; }
    const FVizDataArray* displayEntityIds() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_display_entity_ids(ptr_) : nullptr; }
    const FVizDataArray* displayLocalIds() const noexcept { return ptr_ ? fviz_fea_primary_variable_result_display_local_ids(ptr_) : nullptr; }

    bool displayRange(double& out_minimum, double& out_maximum) const noexcept
    {
        return ptr_ ? fviz_fea_primary_variable_result_display_range(ptr_, &out_minimum, &out_maximum) != FVIZ_FALSE : false;
    }
};

// ---------------------------------------------------------------------------
// DeformedShapeController + result - displacement-based mesh deformation.
// ---------------------------------------------------------------------------
class DeformedShapeController : public Object<FVizFEADeformedShapeController> {
public:
    DeformedShapeController() = default;
    explicit DeformedShapeController(FVizFEADeformedShapeController* owned) : Object<FVizFEADeformedShapeController>(owned) {}
    explicit DeformedShapeController(void* owned) : Object<FVizFEADeformedShapeController>(owned) {}

    static DeformedShapeController create()
    {
        FVizFEADeformedShapeController* controller = nullptr;
        detail::checkResult(fviz_fea_deformed_shape_controller_create(&controller));
        return DeformedShapeController(controller);
    }

    void clearCache() noexcept { if (ptr_) fviz_fea_deformed_shape_controller_clear_cache(ptr_); }
};

class DeformedShapeResult : public Object<FVizFEADeformedShapeResult> {
public:
    DeformedShapeResult() = default;
    explicit DeformedShapeResult(FVizFEADeformedShapeResult* owned) : Object<FVizFEADeformedShapeResult>(owned) {}
    explicit DeformedShapeResult(void* owned) : Object<FVizFEADeformedShapeResult>(owned) {}

    FVizFEADeformationState state() const noexcept { return ptr_ ? fviz_fea_deformed_shape_result_state(ptr_) : FVIZ_FEA_DEFORMATION_UNDEFORMED; }
    double scaleFactor() const noexcept { return ptr_ ? fviz_fea_deformed_shape_result_scale_factor(ptr_) : 1.0; }
    FVizSize mappedPointCount() const noexcept { return ptr_ ? fviz_fea_deformed_shape_result_mapped_point_count(ptr_) : 0u; }
    FVizSize missingPointCount() const noexcept { return ptr_ ? fviz_fea_deformed_shape_result_missing_point_count(ptr_) : 0u; }
    const FVizDataArray* displacements() const noexcept { return ptr_ ? fviz_fea_deformed_shape_result_displacements(ptr_) : nullptr; }
    UnstructuredGrid baseGrid() const
    {
        const FVizUnstructuredGrid* grid = ptr_ ? fviz_fea_deformed_shape_result_base_grid(ptr_) : nullptr;
        return UnstructuredGrid(grid != nullptr ? static_cast<FVizUnstructuredGrid*>(fviz_retain((FVizUnstructuredGrid*)grid)) : nullptr);
    }
    UnstructuredGrid grid() const
    {
        const FVizUnstructuredGrid* grid = ptr_ ? fviz_fea_deformed_shape_result_grid(ptr_) : nullptr;
        return UnstructuredGrid(grid != nullptr ? static_cast<FVizUnstructuredGrid*>(fviz_retain((FVizUnstructuredGrid*)grid)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// ScalarBarActor - Abaqus-style contour legend attached to a renderer.
// ---------------------------------------------------------------------------
class ScalarBarActor : public Object<FVizFEAScalarBarActor> {
public:
    ScalarBarActor() = default;
    explicit ScalarBarActor(FVizFEAScalarBarActor* owned) : Object<FVizFEAScalarBarActor>(owned) {}
    explicit ScalarBarActor(void* owned) : Object<FVizFEAScalarBarActor>(owned) {}

    static ScalarBarActor create(float range_minimum, float range_maximum,
        uint32_t interval_count, LookupTable& lookup_table, const std::string& title = "")
    {
        FVizFEAScalarBarOptions options;
        fviz_fea_scalar_bar_options_initialize(&options);
        options.range_minimum = range_minimum;
        options.range_maximum = range_maximum;
        options.interval_count = interval_count;
        options.tick_count = interval_count + 1u;
        options.title = title.c_str();
        options.lookup_table = lookup_table.get();
        FVizFEAScalarBarActor* actor = nullptr;
        detail::checkResult(fviz_fea_scalar_bar_actor_create(&options, &actor));
        return ScalarBarActor(actor);
    }

    void attach(Renderer& renderer) { detail::checkResult(fviz_fea_scalar_bar_actor_attach(ptr_, renderer.get())); }
    ScalarLegend legend() const
    {
        FVizScalarLegend* legend = ptr_ ? fviz_fea_scalar_bar_actor_legend(ptr_) : nullptr;
        return ScalarLegend(legend != nullptr ? static_cast<FVizScalarLegend*>(fviz_retain(legend)) : nullptr);
    }
};

// ---------------------------------------------------------------------------
// Free FEA helpers.
// ---------------------------------------------------------------------------
namespace fea {

// Configures a lookup table with Abaqus-style discrete contour colors.
inline void configureAbaqusContourLut(LookupTable& table, uint32_t interval_count)
{
    detail::checkResult(fviz_fea_configure_abaqus_contour_lut(table.get(), interval_count));
}

// Builds a banded (12-band style) contour surface from a scalar array.
inline PolyData buildBandedSurface(PolyData& input, const std::string& scalar_array_name,
    uint32_t components, float range_minimum, float range_maximum,
    uint32_t interval_count, const std::string& output_color_array_name)
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_fea_build_abaqus_banded_surface(input.get(), scalar_array_name.c_str(),
        components, range_minimum, range_maximum, interval_count,
        output_color_array_name.c_str(), &surface));
    return PolyData(surface);
}

// Extracts original element edges from a surface for edge overlay.
inline PolyData extractElementEdges(PolyData& surface)
{
    FVizPolyData* edges = nullptr;
    detail::checkResult(fviz_fea_extract_element_edges(surface.get(), &edges));
    return PolyData(edges);
}

// Builds a continuous (smooth) contour surface colored by the Abaqus rainbow.
inline PolyData buildContourSurface(PolyData& input, const std::string& scalar_array_name,
    uint32_t components, float range_minimum, float range_maximum,
    const std::string& output_color_array_name)
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_fea_build_contour_surface(input.get(), scalar_array_name.c_str(),
        components, range_minimum, range_maximum, output_color_array_name.c_str(), &surface));
    return PolyData(surface);
}

// Extracts iso-value contour lines from a surface, tagged with level scalars.
inline PolyData buildContourLines(PolyData& input, const std::string& scalar_array_name,
    uint32_t components, float range_minimum, float range_maximum,
    uint32_t interval_count, const std::string& output_scalar_array_name)
{
    FVizPolyData* lines = nullptr;
    detail::checkResult(fviz_fea_build_contour_lines(input.get(), scalar_array_name.c_str(),
        components, range_minimum, range_maximum, interval_count,
        output_scalar_array_name.c_str(), &lines));
    return PolyData(lines);
}

// Reports surface scalar extrema with original cell/face provenance.
struct Extrema {
    FVizSize min_point_id = 0;
    FVizSize max_point_id = 0;
    double min_value = 0.0;
    double max_value = 0.0;
    uint64_t min_cell_id = FVIZ_INVALID_ID;
    uint64_t max_cell_id = FVIZ_INVALID_ID;
    uint64_t min_face_id = FVIZ_INVALID_ID;
    uint64_t max_face_id = FVIZ_INVALID_ID;

    static Extrema find(PolyData& surface, const std::string& scalar_array_name, uint32_t components)
    {
        FVizFEAExtrema raw;
        fviz_fea_extrema_initialize(&raw);
        detail::checkResult(fviz_fea_find_extrema(surface.get(), scalar_array_name.c_str(), components, &raw));
        Extrema extrema;
        extrema.min_point_id = raw.min_point_id;
        extrema.max_point_id = raw.max_point_id;
        extrema.min_value = raw.min_value;
        extrema.max_value = raw.max_value;
        extrema.min_cell_id = raw.min_cell_id;
        extrema.max_cell_id = raw.max_cell_id;
        extrema.min_face_id = raw.min_face_id;
        extrema.max_face_id = raw.max_face_id;
        return extrema;
    }
};

// Builds a contour surface from an evaluated primary-variable result.
inline PolyData buildContourFromResult(PrimaryVariableResult& result, UnstructuredGrid& grid,
    FVizFEAContourMode mode, float range_minimum, float range_maximum,
    uint32_t interval_count, const std::string& output_color_array_name)
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_fea_build_contour_surface_from_result(result.get(), grid.get(), mode,
        range_minimum, range_maximum, interval_count, output_color_array_name.c_str(), &surface));
    return PolyData(surface);
}

// Extended banded surface with out-of-range colors / reversed spectrum.
inline PolyData buildBandedSurfaceEx(PolyData& input, const std::string& scalar_array_name,
    uint32_t components, float range_minimum, float range_maximum,
    uint32_t interval_count, const FVizFEABandedSurfaceOptions& options,
    const std::string& output_color_array_name)
{
    FVizPolyData* surface = nullptr;
    detail::checkResult(fviz_fea_build_abaqus_banded_surface_ex(input.get(), scalar_array_name.c_str(),
        components, range_minimum, range_maximum, interval_count, &options,
        output_color_array_name.c_str(), &surface));
    return PolyData(surface);
}

// Slices a grid with a plane and colors the cut by a scalar field.
inline PolyData sliceContour(UnstructuredGrid& grid, Plane plane, const std::string& scalar_array_name,
    uint32_t components, FVizFEAContourMode mode, float range_minimum, float range_maximum,
    uint32_t interval_count, const std::string& output_color_array_name)
{
    FVizPolyData* slice = nullptr;
    detail::checkResult(fviz_fea_slice_contour(grid.get(), plane, scalar_array_name.c_str(),
        components, mode, range_minimum, range_maximum, interval_count,
        output_color_array_name.c_str(), &slice));
    return PolyData(slice);
}

} // namespace fea

// Frame methods that need the complete Field type.
inline void Frame::addField(Field& field) { detail::checkResult(fviz_fea_frame_add_field(ptr_, field.get())); }
inline Field Frame::field(const std::string& name) const
{
    FVizFEAField* field = ptr_ ? fviz_fea_frame_field(ptr_, name.c_str()) : nullptr;
    return Field(field != nullptr ? static_cast<FVizFEAField*>(fviz_retain(field)) : nullptr);
}

} // namespace fviz

#endif // FVIZ_CPP_FEA_HPP
