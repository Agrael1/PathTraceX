#pragma once
#include "sphere.h"
#include "consts.h"
#include "camera.h"

//lg 32ud99 w

namespace w {
class Graphics;
class Scene
{
    static inline constexpr uint32_t spheres_count = 4;
    static inline constexpr uint32_t objects_count = spheres_count + 1;

    struct RenderingConstants {
        uint32_t maxDepth;
        uint32_t samplingFn;
        uint32_t BRDF;
    };

public:
    Scene(Graphics& gfx, wis::Result result = wis::success);
    ~Scene();

public:
    void RenderUI();
    void RenderScene(Graphics& gfx, wis::CommandList& cmd_list, wis::DescriptorStorageView dstorage, uint32_t current_frame);
    void CreateAccelerationStructures(Graphics& gfx);
    void CreatePipeline(Graphics& gfx, std::span<wis::DescriptorBindingDesc> descs);
    void Bind(Graphics& gfx, wis::DescriptorStorage& storage);
    void UpdateDispatch(int width, int height);

    void RotateCamera(float dx, float dy);

    void ZoomCamera(float dz);
    void ResetFrames();

private:
    // UI Data
    std::array<bool, 5> show_material_window{};
    std::array<bool, w::flight_frames> update_tlas{};
    std::array<bool, w::flight_frames> update_buffers{};
    bool gamma_correction = true;
    bool limit_max_iterations = false;
    bool accumulate = true;

    int current_sampling = 0;
    int current_brdf = 0;
    int max_iterations = 100;
    int bounces = 4;
    int iterations = 0;

    uint32_t frame_count = 0;

    wis::RootSignature root;
    wis::RaytracingPipeline pipeline;
    wis::Buffer sbt_buffer;

    // Objects
    wis::Buffer object_cbuffer; // push descriptor 0
    wis::Buffer as_buffer; // blas+tlas buffer
    wis::Buffer scratch_buffer; // blas+tlas buffer
    wis::Buffer instance_buffer; // tlas instance buffer
    wis::Buffer camera_buffer; // cam buffer

    SphereStatic sphere_static; // shared geometry
    BoxStatic box_static; // shared geometry

    wis::AccelerationStructure tlas[w::flight_frames]{};
    uint32_t tlas_update_size = 0;
    std::array<wis::AccelerationStructure, 2> blas{}; // shall never be updated
    std::array<ObjectView, objects_count> object_views;

    std::span<MaterialCBuffer> mapped_cbuffer;
    std::span<wis::AccelerationInstance> mapped_instances;

    // misc
    wis::RaytracingDispatchDesc dispatch_desc{};

    w::Camera camera;
    std::span<w::Camera::CBuffer> mapped_camera;
    std::span<RenderingConstants> mapped_rendering_constants;
};
} // namespace w