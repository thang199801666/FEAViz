// FEAViz C++ binding - rendering objects.
//
// RAII wrappers over the FEAViz rendering classes used by a typical viewer:
// Renderer, Camera, Scene, Actor, Mapper, LookupTable, RendererWidget,
// ScalarLegend. Creating a RendererWidget opens a native window; the rest are
// pure objects that can be assembled headless.

#ifndef FVIZ_CPP_RENDERING_HPP
#define FVIZ_CPP_RENDERING_HPP

#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizScene.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizLookupTable.h>
#include <FViz/Rendering/FVizRendererWidget.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizRenderWindow.h>
#include <FViz/Rendering/FVizLight.h>
#include <FViz/Rendering/FVizTextActor.h>
#include <FViz/Rendering/FVizLabelSet.h>

#include "FVizCppObject.hpp"
#include "FVizCppData.hpp"

#include <string>

namespace fviz {

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
class Camera : public Object<FVizCamera> {
public:
    Camera() = default;
    explicit Camera(FVizCamera* owned) : Object<FVizCamera>(owned) {}
    explicit Camera(void* owned) : Object<FVizCamera>(owned) {}

    static Camera create()
    {
        FVizCamera* camera = nullptr;
        detail::checkResult(fviz_camera_create(&camera));
        return Camera(camera);
    }

    void setPosition(Vec3 position) noexcept { if (ptr_) fviz_camera_set_position(ptr_, position); }
    Vec3 position() const noexcept { return ptr_ ? Vec3(fviz_camera_position(ptr_)) : Vec3(); }

    void setTarget(Vec3 target) noexcept { if (ptr_) fviz_camera_set_target(ptr_, target); }
    Vec3 target() const noexcept { return ptr_ ? Vec3(fviz_camera_target(ptr_)) : Vec3(); }

    void setUp(Vec3 up) noexcept { if (ptr_) fviz_camera_set_up(ptr_, up); }
    Vec3 up() const noexcept { return ptr_ ? Vec3(fviz_camera_up(ptr_)) : Vec3(); }

    void setPerspective(float vertical_fov_degrees, float near_plane, float far_plane) noexcept
    {
        if (ptr_) fviz_camera_set_perspective(ptr_, vertical_fov_degrees, near_plane, far_plane);
    }
    void setClippingRange(float near_plane, float far_plane) noexcept
    {
        if (ptr_) fviz_camera_set_clipping_range(ptr_, near_plane, far_plane);
    }
    float fovDegrees() const noexcept { return ptr_ ? fviz_camera_fov_degrees(ptr_) : 0.0f; }
    float nearPlane() const noexcept { return ptr_ ? fviz_camera_near_plane(ptr_) : 0.0f; }
    float farPlane() const noexcept { return ptr_ ? fviz_camera_far_plane(ptr_) : 0.0f; }

    void setProjectionMode(FVizCameraProjectionMode mode) noexcept
    {
        if (ptr_) fviz_camera_set_projection_mode(ptr_, mode);
    }
    FVizCameraProjectionMode projectionMode() const noexcept
    {
        return ptr_ ? fviz_camera_projection_mode(ptr_) : FVIZ_CAMERA_PERSPECTIVE;
    }

    void setParallelScale(float scale) noexcept { if (ptr_) fviz_camera_set_parallel_scale(ptr_, scale); }
    float parallelScale() const noexcept { return ptr_ ? fviz_camera_parallel_scale(ptr_) : 0.0f; }

    Mat4 viewMatrix() const noexcept { return ptr_ ? Mat4(fviz_camera_view_matrix(ptr_)) : Mat4(); }
    Mat4 projectionMatrix(float aspect_ratio) const noexcept
    {
        return ptr_ ? Mat4(fviz_camera_projection_matrix(ptr_, aspect_ratio)) : Mat4();
    }

    void fitBounds(const Bounds& bounds, float padding) noexcept
    {
        if (ptr_) { const FVizBounds b = bounds; fviz_camera_fit_bounds(ptr_, &b, padding); }
    }
    void orbit(float yaw_radians, float pitch_radians) noexcept
    {
        if (ptr_) fviz_camera_orbit(ptr_, yaw_radians, pitch_radians);
    }
    void dolly(float factor) noexcept { if (ptr_) fviz_camera_dolly(ptr_, factor); }
    void pan(float right_amount, float up_amount) noexcept
    {
        if (ptr_) fviz_camera_pan(ptr_, right_amount, up_amount);
    }

    Ray pickRay(int width, int height, int x, int y) const noexcept
    {
        return ptr_ ? Ray(fviz_camera_pick_ray(ptr_, width, height, x, y)) : Ray();
    }
};

// ---------------------------------------------------------------------------
// LookupTable
// ---------------------------------------------------------------------------
class LookupTable : public Object<FVizLookupTable> {
public:
    LookupTable() = default;
    explicit LookupTable(FVizLookupTable* owned) : Object<FVizLookupTable>(owned) {}
    explicit LookupTable(void* owned) : Object<FVizLookupTable>(owned) {}

    static LookupTable create(FVizSize table_size = 256u)
    {
        FVizLookupTable* table = nullptr;
        detail::checkResult(fviz_lookup_table_create(table_size, &table));
        return LookupTable(table);
    }

    FVizSize size() const noexcept { return ptr_ ? fviz_lookup_table_size(ptr_) : 0u; }

    void setRange(float minimum, float maximum) noexcept
    {
        if (ptr_) fviz_lookup_table_set_range(ptr_, minimum, maximum);
    }
    void getRange(float& minimum, float& maximum) const noexcept
    {
        if (ptr_) fviz_lookup_table_get_range(ptr_, &minimum, &maximum);
    }

    void buildPreset(FVizColorMapPreset preset) noexcept
    {
        if (ptr_) (void)fviz_lookup_table_build_preset(ptr_, preset);
    }
    void build() noexcept { if (ptr_) fviz_lookup_table_build(ptr_); }

    void mapScalar(float value, float& red, float& green, float& blue) const noexcept
    {
        if (ptr_) fviz_lookup_table_map_scalar(ptr_, value, &red, &green, &blue);
    }
};

// ---------------------------------------------------------------------------
// Mapper
// ---------------------------------------------------------------------------
class Mapper : public Object<FVizMapper> {
public:
    Mapper() = default;
    explicit Mapper(FVizMapper* owned) : Object<FVizMapper>(owned) {}
    explicit Mapper(void* owned) : Object<FVizMapper>(owned) {}

    static Mapper create()
    {
        FVizMapper* mapper = nullptr;
        detail::checkResult(fviz_mapper_create(&mapper));
        return Mapper(mapper);
    }

    void setPolyData(PolyData& poly_data)
    {
        detail::checkResult(fviz_mapper_set_poly_data(ptr_, poly_data.get()));
    }

    void setLookupTable(LookupTable& table) noexcept { if (ptr_) fviz_mapper_set_lookup_table(ptr_, table.get()); }
    LookupTable lookupTable() const
    {
        FVizLookupTable* table = ptr_ ? fviz_mapper_lookup_table(ptr_) : nullptr;
        return LookupTable(table != nullptr ? static_cast<FVizLookupTable*>(fviz_retain(table)) : nullptr);
    }

    void setScalarVisibility(bool visible) noexcept
    {
        if (ptr_) fviz_mapper_set_scalar_visibility(ptr_, visible ? detail::fbool(true) : detail::fbool(false));
    }
    bool scalarVisibility() const noexcept
    {
        return ptr_ ? fviz_mapper_scalar_visibility(ptr_) != FVIZ_FALSE : false;
    }

    void setScalarRange(float minimum, float maximum) noexcept
    {
        if (ptr_) fviz_mapper_set_scalar_range(ptr_, minimum, maximum);
    }
    void getScalarRange(float& minimum, float& maximum) const noexcept
    {
        if (ptr_) fviz_mapper_get_scalar_range(ptr_, &minimum, &maximum);
    }

    void useAutomaticScalarRange() noexcept { if (ptr_) fviz_mapper_use_automatic_scalar_range(ptr_); }

    // Color the mapper by an array. component_mode chooses DIRECT, MAGNITUDE or COLOR.
    void setArraySelection(FVizDataAssociation association, const char* name,
        FVizComponentMode component_mode = FVIZ_COMPONENT_DIRECT, uint32_t component = 0u)
    {
        FVizArraySelection selection;
        fviz_array_selection_initialize(&selection);
        selection.association = association;
        selection.name = name;
        selection.component_mode = component_mode;
        selection.component = component;
        detail::checkResult(fviz_mapper_set_array_selection(ptr_, &selection));
    }
};

// ---------------------------------------------------------------------------
// Actor
// ---------------------------------------------------------------------------
class Actor : public Object<FVizActor> {
public:
    Actor() = default;
    explicit Actor(FVizActor* owned) : Object<FVizActor>(owned) {}
    explicit Actor(void* owned) : Object<FVizActor>(owned) {}

    static Actor create()
    {
        FVizActor* actor = nullptr;
        detail::checkResult(fviz_actor_create(&actor));
        return Actor(actor);
    }

    void setPolyData(PolyData& poly_data)
    {
        detail::checkResult(fviz_actor_set_poly_data(ptr_, poly_data.get()));
    }
    void setMapper(Mapper& mapper) { detail::checkResult(fviz_actor_set_mapper(ptr_, mapper.get())); }
    Mapper mapper() const
    {
        FVizMapper* m = ptr_ ? fviz_actor_mapper(ptr_) : nullptr;
        return Mapper(m != nullptr ? static_cast<FVizMapper*>(fviz_retain(m)) : nullptr);
    }

    void setColor(float red, float green, float blue) noexcept
    {
        if (ptr_) fviz_actor_set_color(ptr_, red, green, blue);
    }
    void getColor(float& red, float& green, float& blue) const noexcept
    {
        if (ptr_) fviz_actor_get_color(ptr_, &red, &green, &blue);
    }

    void setVisible(bool visible) noexcept { if (ptr_) fviz_actor_set_visible(ptr_, visible ? detail::fbool(true) : detail::fbool(false)); }
    bool isVisible() const noexcept { return ptr_ ? fviz_actor_is_visible(ptr_) != FVIZ_FALSE : false; }

    void setPickable(bool pickable) noexcept { if (ptr_) fviz_actor_set_pickable(ptr_, pickable ? detail::fbool(true) : detail::fbool(false)); }
    bool pickable() const noexcept { return ptr_ ? fviz_actor_pickable(ptr_) != FVIZ_FALSE : false; }

    void setWireframe(bool enabled) noexcept { if (ptr_) fviz_actor_set_wireframe(ptr_, enabled ? detail::fbool(true) : detail::fbool(false)); }
    bool wireframe() const noexcept { return ptr_ ? fviz_actor_wireframe(ptr_) != FVIZ_FALSE : false; }

    void setOpacity(float opacity) noexcept { if (ptr_) fviz_actor_set_opacity(ptr_, opacity); }
    float opacity() const noexcept { return ptr_ ? fviz_actor_opacity(ptr_) : 1.0f; }

    void setEdgeVisibility(bool visible) noexcept { if (ptr_) fviz_actor_set_edge_visibility(ptr_, visible ? detail::fbool(true) : detail::fbool(false)); }
    bool edgeVisibility() const noexcept { return ptr_ ? fviz_actor_edge_visibility(ptr_) != FVIZ_FALSE : false; }

    void setEdgeColor(float red, float green, float blue) noexcept { if (ptr_) fviz_actor_set_edge_color(ptr_, red, green, blue); }

    void setLineWidth(float width) noexcept { if (ptr_) fviz_actor_set_line_width(ptr_, width); }
    void setLineDepthBias(float bias) noexcept { if (ptr_) fviz_actor_set_line_depth_bias(ptr_, bias); }

    void setMaterial(float ambient, float diffuse, float specular, float specular_power) noexcept
    {
        if (ptr_) fviz_actor_set_material(ptr_, ambient, diffuse, specular, specular_power);
    }
    void setShadingMode(FVizShadingMode mode) noexcept { if (ptr_) fviz_actor_set_shading_mode(ptr_, mode); }

    Bounds bounds() const noexcept { return ptr_ ? Bounds(fviz_actor_bounds(ptr_)) : Bounds(); }

    void setPosition(Vec3 position) noexcept { if (ptr_) fviz_actor_set_position(ptr_, position); }
    Vec3 position() const noexcept { return ptr_ ? Vec3(fviz_actor_position(ptr_)) : Vec3(); }
    void setOrientation(Quat orientation) noexcept { if (ptr_) fviz_actor_set_orientation(ptr_, orientation); }
    Quat orientation() const noexcept { return ptr_ ? Quat(fviz_actor_orientation(ptr_)) : Quat(); }
    void setScale(Vec3 scale) noexcept { if (ptr_) fviz_actor_set_scale(ptr_, scale); }
    Vec3 scale() const noexcept { return ptr_ ? Vec3(fviz_actor_scale(ptr_)) : Vec3(); }
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
class Scene : public Object<FVizScene> {
public:
    Scene() = default;
    explicit Scene(FVizScene* owned) : Object<FVizScene>(owned) {}
    explicit Scene(void* owned) : Object<FVizScene>(owned) {}

    static Scene create()
    {
        FVizScene* scene = nullptr;
        detail::checkResult(fviz_scene_create(&scene));
        return Scene(scene);
    }

    void addActor(Actor& actor) { detail::checkResult(fviz_scene_add_actor(ptr_, actor.get())); }
    void removeActor(Actor& actor) { detail::checkResult(fviz_scene_remove_actor(ptr_, actor.get())); }
    void clear() noexcept { if (ptr_) fviz_scene_clear(ptr_); }

    FVizSize actorCount() const noexcept { return ptr_ ? fviz_scene_actor_count(ptr_) : 0u; }

    Actor actorAt(FVizSize index) const
    {
        FVizActor* actor = ptr_ ? fviz_scene_actor(ptr_, index) : nullptr;
        return Actor(actor != nullptr ? static_cast<FVizActor*>(fviz_retain(actor)) : nullptr);
    }

    Bounds bounds() const noexcept { return ptr_ ? Bounds(fviz_scene_bounds(ptr_)) : Bounds(); }
};

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------
class Renderer : public Object<FVizRenderer> {
public:
    Renderer() = default;
    explicit Renderer(FVizRenderer* owned) : Object<FVizRenderer>(owned) {}
    explicit Renderer(void* owned) : Object<FVizRenderer>(owned) {}

    static Renderer create()
    {
        FVizRenderer* renderer = nullptr;
        detail::checkResult(fviz_renderer_create(&renderer));
        return Renderer(renderer);
    }

    Scene scene() const
    {
        FVizScene* scene = ptr_ ? fviz_renderer_scene(ptr_) : nullptr;
        return Scene(scene != nullptr ? static_cast<FVizScene*>(fviz_retain(scene)) : nullptr);
    }

    Camera camera() const
    {
        FVizCamera* camera = ptr_ ? fviz_renderer_camera(ptr_) : nullptr;
        return Camera(camera != nullptr ? static_cast<FVizCamera*>(fviz_retain(camera)) : nullptr);
    }

    void setBackground(float red, float green, float blue) noexcept
    {
        if (ptr_) fviz_renderer_set_background(ptr_, red, green, blue);
    }
    void setBackground(Vec3 color) noexcept { setBackground(color.x, color.y, color.z); }

    void setBackground2(float red, float green, float blue) noexcept
    {
        if (ptr_) fviz_renderer_set_background2(ptr_, red, green, blue);
    }
    void setGradientBackground(bool enabled) noexcept
    {
        if (ptr_) fviz_renderer_set_gradient_background(ptr_, enabled ? detail::fbool(true) : detail::fbool(false));
    }

    void setScene(Scene& scene) { detail::checkResult(fviz_renderer_set_scene(ptr_, scene.get())); }
    void fitCamera(float padding = 1.0f) noexcept { if (ptr_) fviz_renderer_fit_camera(ptr_, padding); }
    void resetClippingRange() noexcept { if (ptr_) fviz_renderer_reset_clipping_range(ptr_); }
    void update() { detail::checkResult(fviz_renderer_update(ptr_)); }
};

// ---------------------------------------------------------------------------
// ScalarLegend
// ---------------------------------------------------------------------------
class ScalarLegend : public Object<FVizScalarLegend> {
public:
    ScalarLegend() = default;
    explicit ScalarLegend(FVizScalarLegend* owned) : Object<FVizScalarLegend>(owned) {}
    explicit ScalarLegend(void* owned) : Object<FVizScalarLegend>(owned) {}

    static ScalarLegend create()
    {
        FVizScalarLegend* legend = nullptr;
        detail::checkResult(fviz_scalar_legend_create(&legend));
        return ScalarLegend(legend);
    }

    void setLookupTable(LookupTable& table) noexcept { if (ptr_) fviz_scalar_legend_set_lookup_table(ptr_, table.get()); }
    void setRange(float minimum, float maximum) noexcept { if (ptr_) fviz_scalar_legend_set_range(ptr_, minimum, maximum); }
    void getRange(float& minimum, float& maximum) const noexcept { if (ptr_) fviz_scalar_legend_get_range(ptr_, &minimum, &maximum); }
    void setPosition(FVizLegendPosition position) noexcept { if (ptr_) fviz_scalar_legend_set_position(ptr_, position); }
    void setVisible(bool visible) noexcept { if (ptr_) fviz_scalar_legend_set_visible(ptr_, visible ? detail::fbool(true) : detail::fbool(false)); }
    bool isVisible() const noexcept { return ptr_ ? fviz_scalar_legend_is_visible(ptr_) != FVIZ_FALSE : false; }
    void setTitle(const char* title) noexcept { if (ptr_) fviz_scalar_legend_set_title(ptr_, title); }
    const char* title() const noexcept { return ptr_ ? fviz_scalar_legend_title(ptr_) : nullptr; }
    void setUnits(const char* units) noexcept { if (ptr_) fviz_scalar_legend_set_units(ptr_, units); }
    void setTickCount(uint32_t tick_count) noexcept { if (ptr_) fviz_scalar_legend_set_tick_count(ptr_, tick_count); }
    uint32_t tickCount() const noexcept { return ptr_ ? fviz_scalar_legend_tick_count(ptr_) : 0u; }
    void setBarSize(float width_pixels, float height_pixels) noexcept
    {
        if (ptr_) fviz_scalar_legend_set_bar_size(ptr_, width_pixels, height_pixels);
    }
    void setDiscrete(bool discrete) noexcept { if (ptr_) fviz_scalar_legend_set_discrete(ptr_, discrete ? detail::fbool(true) : detail::fbool(false)); }
};

// ---------------------------------------------------------------------------
// RendererWidget - native window hosting a renderer.
// ---------------------------------------------------------------------------
class RendererWidget : public Object<FVizRendererWidget> {
public:
    RendererWidget() = default;
    explicit RendererWidget(FVizRendererWidget* owned) : Object<FVizRendererWidget>(owned) {}
    explicit RendererWidget(void* owned) : Object<FVizRendererWidget>(owned) {}

    static RendererWidget create(int width, int height, const char* title = "FEAViz")
    {
        FVizRendererWidget* widget = nullptr;
        detail::checkResult(fviz_renderer_widget_create(width, height, title, &widget));
        return RendererWidget(widget);
    }

    static RendererWidget createAttached(void* host_native_handle, int width, int height)
    {
        FVizRendererWidget* widget = nullptr;
        detail::checkResult(fviz_renderer_widget_create_attached(host_native_handle, width, height, &widget));
        return RendererWidget(widget);
    }

    Renderer renderer() const
    {
        FVizRenderer* renderer = ptr_ ? fviz_renderer_widget_renderer(ptr_) : nullptr;
        return Renderer(renderer != nullptr ? static_cast<FVizRenderer*>(fviz_retain(renderer)) : nullptr);
    }

    void show() { detail::checkResult(fviz_renderer_widget_show(ptr_)); }
    void render() { detail::checkResult(fviz_renderer_widget_render(ptr_)); }
    void resize(int width, int height) { detail::checkResult(fviz_renderer_widget_resize(ptr_, width, height)); }
    void syncHostSize() { detail::checkResult(fviz_renderer_widget_sync_host_size(ptr_)); }
    void addActor(Actor& actor) { detail::checkResult(fviz_renderer_widget_add_actor(ptr_, actor.get())); }

    void* nativeHandle() const noexcept { return ptr_ ? fviz_renderer_widget_native_handle(ptr_) : nullptr; }
    void* hostNativeHandle() const noexcept { return ptr_ ? fviz_renderer_widget_host_native_handle(ptr_) : nullptr; }
    bool isAttached() const noexcept { return ptr_ ? fviz_renderer_widget_is_attached(ptr_) != FVIZ_FALSE : false; }
};

// ---------------------------------------------------------------------------
// Light
// ---------------------------------------------------------------------------
class Light : public Object<FVizLight> {
public:
    Light() = default;
    explicit Light(FVizLight* owned) : Object<FVizLight>(owned) {}
    explicit Light(void* owned) : Object<FVizLight>(owned) {}

    static Light create()
    {
        FVizLight* light = nullptr;
        detail::checkResult(fviz_light_create(&light));
        return Light(light);
    }

    void setType(FVizLightType type) noexcept { if (ptr_) fviz_light_set_type(ptr_, type); }
    FVizLightType type() const noexcept { return ptr_ ? fviz_light_type(ptr_) : FVIZ_LIGHT_SCENE; }
    void setEnabled(bool enabled) noexcept { if (ptr_) fviz_light_set_enabled(ptr_, detail::fbool(enabled)); }
    bool enabled() const noexcept { return ptr_ ? fviz_light_enabled(ptr_) != FVIZ_FALSE : false; }
    void setPosition(Vec3 position) noexcept { if (ptr_) fviz_light_set_position(ptr_, position); }
    Vec3 position() const noexcept { return ptr_ ? Vec3(fviz_light_position(ptr_)) : Vec3(); }
    void setColor(float red, float green, float blue) noexcept { if (ptr_) fviz_light_set_color(ptr_, red, green, blue); }
    void setIntensity(float intensity) noexcept { if (ptr_) fviz_light_set_intensity(ptr_, intensity); }
    float intensity() const noexcept { return ptr_ ? fviz_light_intensity(ptr_) : 1.0f; }
};

// ---------------------------------------------------------------------------
// TextActor2D - screen-space text.
// ---------------------------------------------------------------------------
class TextActor2D : public Object<FVizTextActor2D> {
public:
    TextActor2D() = default;
    explicit TextActor2D(FVizTextActor2D* owned) : Object<FVizTextActor2D>(owned) {}
    explicit TextActor2D(void* owned) : Object<FVizTextActor2D>(owned) {}

    static TextActor2D create()
    {
        FVizTextActor2D* actor = nullptr;
        detail::checkResult(fviz_text_actor_2d_create(&actor));
        return TextActor2D(actor);
    }

    void setText(const char* utf8) { detail::checkResult(fviz_text_actor_2d_set_text(ptr_, utf8)); }
    const char* text() const noexcept { return ptr_ ? fviz_text_actor_2d_text(ptr_) : nullptr; }
    void setPosition(float x, float y) noexcept { if (ptr_) fviz_text_actor_2d_set_position(ptr_, x, y); }
    void getPosition(float& x, float& y) const noexcept { if (ptr_) fviz_text_actor_2d_get_position(ptr_, &x, &y); }
    void setCoordinateSystem(FVizTextCoordinateSystem system) noexcept { if (ptr_) fviz_text_actor_2d_set_coordinate_system(ptr_, system); }
    void setVisible(bool visible) noexcept { if (ptr_) fviz_text_actor_2d_set_visible(ptr_, detail::fbool(visible)); }
    bool isVisible() const noexcept { return ptr_ ? fviz_text_actor_2d_is_visible(ptr_) != FVIZ_FALSE : false; }
};

// ---------------------------------------------------------------------------
// BillboardTextActor3D - world-space text facing the camera.
// ---------------------------------------------------------------------------
class BillboardTextActor3D : public Object<FVizBillboardTextActor3D> {
public:
    BillboardTextActor3D() = default;
    explicit BillboardTextActor3D(FVizBillboardTextActor3D* owned) : Object<FVizBillboardTextActor3D>(owned) {}
    explicit BillboardTextActor3D(void* owned) : Object<FVizBillboardTextActor3D>(owned) {}

    static BillboardTextActor3D create()
    {
        FVizBillboardTextActor3D* actor = nullptr;
        detail::checkResult(fviz_billboard_text_actor_3d_create(&actor));
        return BillboardTextActor3D(actor);
    }

    void setText(const char* utf8) { detail::checkResult(fviz_billboard_text_actor_3d_set_text(ptr_, utf8)); }
    const char* text() const noexcept { return ptr_ ? fviz_billboard_text_actor_3d_text(ptr_) : nullptr; }
    void setWorldPosition(Vec3 position) noexcept { if (ptr_) fviz_billboard_text_actor_3d_set_world_position(ptr_, position); }
    Vec3 worldPosition() const noexcept { return ptr_ ? Vec3(fviz_billboard_text_actor_3d_world_position(ptr_)) : Vec3(); }
    void setPixelOffset(float x, float y) noexcept { if (ptr_) fviz_billboard_text_actor_3d_set_pixel_offset(ptr_, x, y); }
    void setVisible(bool visible) noexcept { if (ptr_) fviz_billboard_text_actor_3d_set_visible(ptr_, detail::fbool(visible)); }
    bool isVisible() const noexcept { return ptr_ ? fviz_billboard_text_actor_3d_is_visible(ptr_) != FVIZ_FALSE : false; }
};

// ---------------------------------------------------------------------------
// LabelSet3D - batched world-space labels.
// ---------------------------------------------------------------------------
class LabelSet3D : public Object<FVizLabelSet3D> {
public:
    LabelSet3D() = default;
    explicit LabelSet3D(FVizLabelSet3D* owned) : Object<FVizLabelSet3D>(owned) {}
    explicit LabelSet3D(void* owned) : Object<FVizLabelSet3D>(owned) {}

    static LabelSet3D create()
    {
        FVizLabelSet3D* set = nullptr;
        detail::checkResult(fviz_label_set_3d_create(&set));
        return LabelSet3D(set);
    }

    void add(Vec3 position, const char* utf8)
    {
        detail::checkResult(fviz_label_set_3d_add(ptr_, position, utf8, nullptr));
    }
    FVizSize count() const noexcept { return ptr_ ? fviz_label_set_3d_count(ptr_) : 0u; }
    Vec3 positionAt(FVizSize index) const noexcept
    {
        return ptr_ ? Vec3(fviz_label_set_3d_position_at(ptr_, index)) : Vec3();
    }
    const char* textAt(FVizSize index) const noexcept { return ptr_ ? fviz_label_set_3d_text_at(ptr_, index) : nullptr; }
    void setVisible(bool visible) noexcept { if (ptr_) fviz_label_set_3d_set_visible(ptr_, detail::fbool(visible)); }
    bool visible() const noexcept { return ptr_ ? fviz_label_set_3d_visible(ptr_) != FVIZ_FALSE : false; }
    void clear() noexcept { if (ptr_) fviz_label_set_3d_clear(ptr_); }
};

// ---------------------------------------------------------------------------
// RenderWindow - native window (optionally offscreen) hosting renderers.
// ---------------------------------------------------------------------------
class RenderWindow : public Object<FVizRenderWindow> {
public:
    RenderWindow() = default;
    explicit RenderWindow(FVizRenderWindow* owned) : Object<FVizRenderWindow>(owned) {}
    explicit RenderWindow(void* owned) : Object<FVizRenderWindow>(owned) {}

    static RenderWindow create(int width, int height, const char* title = "FEAViz")
    {
        FVizRenderWindow* window = nullptr;
        detail::checkResult(fviz_render_window_create(width, height, title, &window));
        return RenderWindow(window);
    }

    static RenderWindow createOffscreen(int width, int height)
    {
        FVizRenderWindow* window = nullptr;
        detail::checkResult(fviz_render_window_create_offscreen(width, height, &window));
        return RenderWindow(window);
    }

    static RenderWindow createAttached(void* host_native_handle, int width, int height)
    {
        FVizRenderWindow* window = nullptr;
        detail::checkResult(fviz_render_window_create_attached(host_native_handle, width, height, &window));
        return RenderWindow(window);
    }

    static bool supported() noexcept { return fviz_render_window_supported() != FVIZ_FALSE; }

    void setRenderer(Renderer& renderer) { detail::checkResult(fviz_render_window_set_renderer(ptr_, renderer.get())); }
    void addRenderer(Renderer& renderer) { detail::checkResult(fviz_render_window_add_renderer(ptr_, renderer.get())); }
    Renderer rendererAt(FVizSize index) const
    {
        FVizRenderer* renderer = ptr_ ? fviz_render_window_renderer_at(ptr_, index) : nullptr;
        return Renderer(renderer != nullptr ? static_cast<FVizRenderer*>(fviz_retain(renderer)) : nullptr);
    }
    FVizSize rendererCount() const noexcept { return ptr_ ? fviz_render_window_renderer_count(ptr_) : 0u; }

    void show() { detail::checkResult(fviz_render_window_show(ptr_)); }
    void render() { detail::checkResult(fviz_render_window_render(ptr_)); }
    void requestRender() noexcept { if (ptr_) fviz_render_window_request_render(ptr_); }
    void resize(int width, int height) { detail::checkResult(fviz_render_window_resize(ptr_, width, height)); }
    void getSize(int& width, int& height) const noexcept { if (ptr_) fviz_render_window_get_size(ptr_, &width, &height); }
    void initialize() { detail::checkResult(fviz_render_window_initialize(ptr_)); }
    void finalize() noexcept { if (ptr_) fviz_render_window_finalize(ptr_); }
    void requestClose() noexcept { if (ptr_) fviz_render_window_request_close(ptr_); }
    FVizRenderWindowState state() const noexcept { return ptr_ ? fviz_render_window_state(ptr_) : FVIZ_RENDER_WINDOW_CREATED; }
    uint32_t dpi() const noexcept { return ptr_ ? fviz_render_window_dpi(ptr_) : 96u; }
    float contentScale() const noexcept { return ptr_ ? fviz_render_window_content_scale(ptr_) : 1.0f; }
    void* nativeHandle() const noexcept { return ptr_ ? fviz_render_window_native_handle(ptr_) : nullptr; }
};

} // namespace fviz

#endif // FVIZ_CPP_RENDERING_HPP
