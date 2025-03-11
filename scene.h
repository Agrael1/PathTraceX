#pragma once
#include "sphere.h"

namespace w {
class Graphics;
class Scene
{
    static inline constexpr uint32_t spheres_count = 4;
    static inline constexpr uint32_t objects_count = spheres_count + 1;

public:
    Scene(Graphics& gfx, wis::Result result = wis::success);
    ~Scene();

public:
    void RenderUI();

private:
    // UI Data
    std::array<bool, 5> show_material_window{};
    bool gamma_correction = true;

    wis::Buffer object_cbuffer; // push descriptor 0
    wis::Buffer as_buffer; // blas+tlas buffer
    wis::Buffer instance_buffer; // tlas instance buffer

    SphereStatic sphere_static; // shared geometry
    BoxStatic box_static; // shared geometry

    wis::AccelerationStructure tlas{};
    std::array<wis::AccelerationStructure, objects_count> blas{}; // shall never be updated
    std::array<ObjectView, objects_count> object_views;

    std::span<ObjectCBuffer> mapped_cbuffer;
};
} // namespace w