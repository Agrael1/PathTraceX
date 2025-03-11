#pragma once
#include "sphere.h"
#include "consts.h"

//lg 32ud99 w

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
    void CreateAccelerationStructures(Graphics& gfx);
    void CreatePipeline(Graphics& gfx, std::span<wis::DescriptorBindingDesc> descs);
    void Bind(Graphics& gfx, wis::DescriptorStorage& storage);
    void UpdateDispatch(int width, int height);

private:
    // UI Data
    std::array<bool, 5> show_material_window{};
    bool gamma_correction = true;

    wis::RootSignature root;
    wis::RaytracingPipeline pipeline;
    wis::Buffer sbt_buffer;

    // Objects
    wis::Buffer object_cbuffer; // push descriptor 0
    wis::Buffer as_buffer; // blas+tlas buffer
    wis::Buffer instance_buffer; // tlas instance buffer

    SphereStatic sphere_static; // shared geometry
    BoxStatic box_static; // shared geometry

    wis::AccelerationStructure tlas[w::flight_frames]{};
    std::array<wis::AccelerationStructure, 2> blas{}; // shall never be updated
    std::array<ObjectView, objects_count> object_views;

    std::span<ObjectCBuffer> mapped_cbuffer;

    // misc
    wis::RaytracingDispatchDesc dispatch_desc;
};
} // namespace w