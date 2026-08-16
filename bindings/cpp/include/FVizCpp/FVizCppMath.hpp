// FEAViz C++ binding - math types.
//
// Thin C++17 value types over the C math structs (FVizVec3, FVizMat4, ...)
// with operators, so application code reads naturally while staying ABI
// compatible with the C core.

#ifndef FVIZ_CPP_MATH_HPP
#define FVIZ_CPP_MATH_HPP

#include <FViz/Math/FVizVec2.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Math/FVizVec4.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Math/FVizRay.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace fviz {

// ---------------------------------------------------------------------------
// Vec2
// ---------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}
    constexpr Vec2(const FVizVec2& v) noexcept : x(v.x), y(v.y) {}

    operator FVizVec2() const noexcept { return fviz_vec2(x, y); }

    Vec2& operator+=(const Vec2& other) noexcept { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) noexcept { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
    Vec2& operator/=(float s) noexcept { const float inv = 1.0f / s; x *= inv; y *= inv; return *this; }

    Vec2 operator+(const Vec2& other) const noexcept { Vec2 r(*this); r += other; return r; }
    Vec2 operator-(const Vec2& other) const noexcept { Vec2 r(*this); r -= other; return r; }
    Vec2 operator*(float s) const noexcept { Vec2 r(*this); r *= s; return r; }
    Vec2 operator/(float s) const noexcept { Vec2 r(*this); r /= s; return r; }
    Vec2 operator-() const noexcept { return Vec2(-x, -y); }

    bool operator==(const Vec2& other) const noexcept { return x == other.x && y == other.y; }
    bool operator!=(const Vec2& other) const noexcept { return !(*this == other); }

    float dot(const Vec2& other) const noexcept { return fviz_vec2_dot(*this, other); }
    float length() const noexcept { return fviz_vec2_length(*this); }
    float lengthSquared() const noexcept { return x * x + y * y; }
    Vec2 normalized() const noexcept { return fviz_vec2_normalize(*this); }
};

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
    constexpr Vec3(const FVizVec3& v) noexcept : x(v.x), y(v.y), z(v.z) {}

    operator FVizVec3() const noexcept { return fviz_vec3(x, y, z); }

    Vec3& operator+=(const Vec3& other) noexcept { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) noexcept { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(float s) noexcept { const float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; return *this; }

    Vec3 operator+(const Vec3& other) const noexcept { Vec3 r(*this); r += other; return r; }
    Vec3 operator-(const Vec3& other) const noexcept { Vec3 r(*this); r -= other; return r; }
    Vec3 operator*(float s) const noexcept { Vec3 r(*this); r *= s; return r; }
    Vec3 operator/(float s) const noexcept { Vec3 r(*this); r /= s; return r; }
    Vec3 operator-() const noexcept { return Vec3(-x, -y, -z); }

    bool operator==(const Vec3& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const Vec3& other) const noexcept { return !(*this == other); }

    float dot(const Vec3& other) const noexcept { return fviz_vec3_dot(*this, other); }
    Vec3 cross(const Vec3& other) const noexcept { return fviz_vec3_cross(*this, other); }
    float length() const noexcept { return fviz_vec3_length(*this); }
    float lengthSquared() const noexcept { return x * x + y * y + z * z; }
    Vec3 normalized() const noexcept { return fviz_vec3_normalize(*this); }
};

inline Vec3 operator*(float s, const Vec3& v) noexcept { return v * s; }

// ---------------------------------------------------------------------------
// Vec4
// ---------------------------------------------------------------------------
struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const FVizVec4& v) noexcept : x(v.x), y(v.y), z(v.z), w(v.w) {}

    operator FVizVec4() const noexcept { return fviz_vec4(x, y, z, w); }

    Vec4& operator+=(const Vec4& other) noexcept { x += other.x; y += other.y; z += other.z; w += other.w; return *this; }
    Vec4& operator-=(const Vec4& other) noexcept { x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this; }
    Vec4& operator*=(float s) noexcept { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vec4& operator/=(float s) noexcept { const float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; w *= inv; return *this; }

    Vec4 operator+(const Vec4& other) const noexcept { Vec4 r(*this); r += other; return r; }
    Vec4 operator-(const Vec4& other) const noexcept { Vec4 r(*this); r -= other; return r; }
    Vec4 operator*(float s) const noexcept { Vec4 r(*this); r *= s; return r; }
    Vec4 operator/(float s) const noexcept { Vec4 r(*this); r /= s; return r; }
    Vec4 operator-() const noexcept { return Vec4(-x, -y, -z, -w); }

    bool operator==(const Vec4& other) const noexcept { return x == other.x && y == other.y && z == other.z && w == other.w; }
    bool operator!=(const Vec4& other) const noexcept { return !(*this == other); }

    float dot(const Vec4& other) const noexcept { return fviz_vec4_dot(*this, other); }
    float length() const noexcept { return std::sqrt(x * x + y * y + z * z + w * w); }
};

// ---------------------------------------------------------------------------
// Mat4 - column-major, matches FVizMat4.m[16] laid out by fviz_mat4_*.
// ---------------------------------------------------------------------------
struct Mat4 {
    std::array<float, 16> m{};

    Mat4() = default;
    Mat4(const FVizMat4& v) noexcept { std::copy(std::begin(v.m), std::end(v.m), m.begin()); }

    operator FVizMat4() const noexcept {
        FVizMat4 out;
        std::copy(m.begin(), m.end(), std::begin(out.m));
        return out;
    }

    static Mat4 identity() noexcept { return Mat4(fviz_mat4_identity()); }
    static Mat4 perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) noexcept {
        return fviz_mat4_perspective(fov_y_radians, aspect, near_plane, far_plane);
    }
    static Mat4 orthographic(float l, float r, float b, float t, float n, float f) noexcept {
        return fviz_mat4_orthographic(l, r, b, t, n, f);
    }
    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept {
        return fviz_mat4_look_at(eye, target, up);
    }

    Mat4 operator*(const Mat4& other) const noexcept { return fviz_mat4_multiply(*this, other); }
    Mat4& operator*=(const Mat4& other) noexcept { *this = *this * other; return *this; }

    float& operator()(size_t row, size_t column) noexcept { return m[column * 4u + row]; }
    const float& operator()(size_t row, size_t column) const noexcept { return m[column * 4u + row]; }
};

// ---------------------------------------------------------------------------
// Quat - (x, y, z, w), normalized quaternion.
// ---------------------------------------------------------------------------
struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Quat(const FVizQuat& q) noexcept : x(q.x), y(q.y), z(q.z), w(q.w) {}

    operator FVizQuat() const noexcept { FVizQuat q; q.x = x; q.y = y; q.z = z; q.w = w; return q; }

    static Quat identity() noexcept { return Quat(0.0f, 0.0f, 0.0f, 1.0f); }
    static Quat fromAxisAngle(Vec3 axis, float angle_radians) noexcept {
        return fviz_quat_from_axis_angle(axis, angle_radians);
    }

    Quat operator*(const Quat& other) const noexcept { return fviz_quat_multiply(*this, other); }
    Quat normalized() const noexcept { return fviz_quat_normalize(*this); }
    float dot(const Quat& other) const noexcept { return fviz_quat_dot(*this, other); }
    Vec3 rotate(Vec3 v) const noexcept { return fviz_quat_rotate_vec3(*this, v); }

    bool operator==(const Quat& other) const noexcept { return x == other.x && y == other.y && z == other.z && w == other.w; }
    bool operator!=(const Quat& other) const noexcept { return !(*this == other); }
};

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------
struct Bounds {
    Vec3 minimum{};
    Vec3 maximum{};
    bool valid = false;

    Bounds() = default;
    Bounds(const FVizBounds& b) noexcept
        : minimum(b.min), maximum(b.max), valid(b.valid != FVIZ_FALSE) {}

    operator FVizBounds() const noexcept {
        FVizBounds b;
        b.min = minimum;
        b.max = maximum;
        b.valid = valid ? static_cast<FVizBool>(1) : static_cast<FVizBool>(0);
        return b;
    }

    static Bounds empty() noexcept { return Bounds(fviz_bounds_empty()); }
    void includePoint(Vec3 p) noexcept {
        if (!valid) { minimum = maximum = p; valid = true; return; }
        minimum.x = std::min(minimum.x, p.x); maximum.x = std::max(maximum.x, p.x);
        minimum.y = std::min(minimum.y, p.y); maximum.y = std::max(maximum.y, p.y);
        minimum.z = std::min(minimum.z, p.z); maximum.z = std::max(maximum.z, p.z);
    }
    void includeBounds(const Bounds& other) noexcept {
        if (!other.valid) return;
        includePoint(other.minimum);
        includePoint(other.maximum);
    }
    Vec3 center() const noexcept { const FVizBounds b = *this; return fviz_bounds_center(&b); }
    Vec3 size() const noexcept { const FVizBounds b = *this; return fviz_bounds_size(&b); }
    float radius() const noexcept { const FVizBounds b = *this; return fviz_bounds_radius(&b); }
};

// ---------------------------------------------------------------------------
// Plane / Ray
// ---------------------------------------------------------------------------
struct Plane {
    Vec3 normal{};
    float distance = 0.0f;

    Plane() = default;
    Plane(const FVizPlane& p) noexcept : normal(p.normal), distance(p.distance) {}
    operator FVizPlane() const noexcept { FVizPlane p; p.normal = normal; p.distance = distance; return p; }

    static Plane fromPointNormal(Vec3 point, Vec3 normal_) noexcept {
        return fviz_plane_from_point_normal(point, normal_);
    }
    float distanceToPoint(Vec3 point) const noexcept { return fviz_plane_distance_to_point(*this, point); }
    Vec3 projectPoint(Vec3 point) const noexcept { return fviz_plane_project_point(*this, point); }
};

struct Ray {
    Vec3 origin{};
    Vec3 direction{};

    Ray() = default;
    Ray(const FVizRay& r) noexcept : origin(r.origin), direction(r.direction) {}
    operator FVizRay() const noexcept { FVizRay r; r.origin = origin; r.direction = direction; return r; }

    static Ray fromPoints(Vec3 o, Vec3 d) noexcept { return fviz_ray(o, d); }
    Vec3 pointAt(float t) const noexcept { return fviz_ray_point_at(*this, t); }
    float distanceToPoint(Vec3 point) const noexcept { return fviz_ray_distance_to_point(*this, point); }
    bool intersectsSphere(Vec3 center, float radius, float* out_t = nullptr) const noexcept {
        return fviz_ray_intersects_sphere(*this, center, radius, out_t) != FVIZ_FALSE;
    }
};

} // namespace fviz

#endif // FVIZ_CPP_MATH_HPP
