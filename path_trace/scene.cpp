#include "scene.h"
#include "graphics.h"
#include <imgui.h>

w::Scene::Scene(Graphics& gfx, wis::Result result)
    : object_cbuffer(gfx.allocator.CreateUploadBuffer(result, sizeof(ObjectCBuffer) * objects_count))
    , instance_buffer(gfx.allocator.CreateUploadBuffer(result, sizeof(wis::AccelerationInstance) * objects_count))
    , sphere_static(gfx)
    , box_static(gfx)
    , mapped_cbuffer(object_cbuffer.Map<ObjectCBuffer>(), objects_count)
{
    object_views[0] = ObjectView("Box", {
                                                .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
                                                .emissive = {},
                                                .roughness = 1,
                                        });

    constexpr DirectX::XMFLOAT4A sphere_colors[spheres_count] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f },
    };

    for (int i = 1; i < objects_count; ++i) {
        object_views[i] = ObjectView(wis::format("Sphere {}", i), {
                                                                          .diffuse = sphere_colors[i],
                                                                          .emissive = {},
                                                                          .roughness = 1,
                                                                  });
    }
    CreateAccelerationStructures(gfx);
}

w::Scene::~Scene()
{
    object_cbuffer.Unmap();
}

void w::Scene::RenderUI()
{
    ImGui::Begin("Settings", nullptr);
    ImGui::PushItemWidth(150);
    // std::string fps_cpu_string = "FPS (CPU): " + std::to_string(fps_cpu);
    // ImGui::Text(fps_cpu_string.c_str());
    // std::string fps_string = "FPS (GPU): " + std::to_string(fps_cpu);
    // ImGui::Text(fps_cpu_string.c_str());
    // std::string iterations_string = "Iterations: " + std::to_string(iterations);
    // ImGui::Text(iterations_string.c_str());
    ImGui::Checkbox("Show Material Box", &show_material_window[0]);
    ImGui::Checkbox("Show Material Sph#1", &show_material_window[1]);
    ImGui::Checkbox("Show Material Sph#2", &show_material_window[2]);
    ImGui::Checkbox("Show Material Sph#3", &show_material_window[3]);
    ImGui::Checkbox("Show Material Sph#4", &show_material_window[4]);

    // ImGui::Checkbox("Accumulate", &accumulate);
    ImGui::Checkbox("Gama Correction", &gamma_correction);
    // if (ImGui::Checkbox("Limit Iterations", &limit_max_iterations)) {
    //     clear();
    // }
    // if (ImGui::SliderInt("Max Iterations", &max_iterations, 1, 1000)) {
    //     clear();
    // }

    // if (ImGui::Combo("Sampling", &current_sampling, SAMPLING_LABELS, IM_ARRAYSIZE(SAMPLING_LABELS))) {
    //     clear();
    // }
    // if (ImGui::Combo("BRDF", &current_brdf, BRDF_LABELS, IM_ARRAYSIZE(BRDF_LABELS))) {
    //     clear();
    // }
    // if (ImGui::SliderInt("Bounces", &bounces, 1, 100)) {
    //     clear();
    // }
    ImGui::End();

    for (int m = 0; m < objects_count; ++m) {
        if (show_material_window[m]) {
            object_views[m].RenderMatrialUI(mapped_cbuffer[m]);
        }
    }
}

void w::Scene::CreateAccelerationStructures(Graphics& gfx)
{
    using namespace wis;
    wis::Result result = wis::success;
    auto& device = gfx.GetDevice();
    auto& rt = gfx.GetRaytracing();

    wis::AcceleratedGeometryInput inputs[2]{
        {
                .geometry_type = wis::ASGeometryType::Triangles,
                .flags = wis::ASGeometryFlags::Opaque,
                .vertex_or_aabb_buffer_address = box_static.list.vertex_buffer.GetGPUAddress(),
                .vertex_or_aabb_buffer_stride = sizeof(DirectX::XMFLOAT3),
                .index_buffer_address = box_static.list.index_buffer.GetGPUAddress(),
                .transform_matrix_address = 0,
                .vertex_count = box_static.list.vertex_count,
                .triangle_or_aabb_count = box_static.list.index_count / 3,
                .vertex_format = wis::DataFormat::RGB32Float,
                .index_format = wis::IndexType::UInt16,
        },
        {
                .geometry_type = wis::ASGeometryType::Triangles,
                .flags = wis::ASGeometryFlags::Opaque,
                .vertex_or_aabb_buffer_address = sphere_static.list.vertex_buffer.GetGPUAddress(),
                .vertex_or_aabb_buffer_stride = sizeof(DirectX::XMFLOAT3),
                .index_buffer_address = sphere_static.list.index_buffer.GetGPUAddress(),
                .transform_matrix_address = 0,
                .vertex_count = sphere_static.list.vertex_count,
                .triangle_or_aabb_count = sphere_static.list.index_count / 3,
                .vertex_format = wis::DataFormat::RGB32Float,
                .index_format = wis::IndexType::UInt16,
        }
    };
    wis::AcceleratedGeometryDesc accelerated_geometry_descs[2]{};
    for (int i = 0; i < 2; ++i) {
        accelerated_geometry_descs[i] = wis::CreateGeometryDesc(inputs[i]);
    }

    wis::BottomLevelASBuildDesc blas_descs[2]{
        { .flags = wis::AccelerationStructureFlags::PreferFastTrace,
          .geometry_count = 1,
          .geometry_array = &accelerated_geometry_descs[0] },
        { .flags = wis::AccelerationStructureFlags::PreferFastTrace,
          .geometry_count = 1,
          .geometry_array = &accelerated_geometry_descs[1] }
    };

    wis::TopLevelASBuildDesc tlas_desc{
        .flags = wis::AccelerationStructureFlags::PreferFastTrace | wis::AccelerationStructureFlags::AllowUpdate,
        .instance_count = objects_count,
        .gpu_address = instance_buffer.GetGPUAddress(),
    };

    wis::ASAllocationInfo infos[3]{};
    for (int i = 0; i < 2; ++i) {
        infos[i] = rt.GetBottomLevelASSize(blas_descs[i]);
    }
    infos[2] = rt.GetTopLevelASSize(tlas_desc);

    // allocate buffers
    uint64_t full_size = infos[0].result_size + infos[1].result_size + infos[2].result_size * w::flight_frames;
    as_buffer = gfx.allocator.CreateBuffer(result, full_size, wis::BufferUsage::AccelerationStructureBuffer);
    wis::Buffer scratch_buffer = gfx.allocator.CreateBuffer(result, infos[0].scratch_size + infos[1].scratch_size + infos[2].scratch_size * w::flight_frames, wis::BufferUsage::StorageBuffer);

    // create acceleration structures
    wis::CommandList cmd_list = gfx.device.CreateCommandList(result, wis::QueueType::Graphics);
    uint64_t offset_scratch = 0;
    uint64_t offset_result = 0;
    for (int i = 0; i < 2; ++i) {
        blas[i] = rt.CreateAccelerationStructure(result, as_buffer, offset_result, infos[i].result_size, wis::ASLevel::Bottom);
        rt.BuildBottomLevelAS(cmd_list, blas_descs[i], blas[i], scratch_buffer.GetGPUAddress() + offset_scratch);
        offset_scratch += infos[i].scratch_size;
        offset_result += infos[i].result_size;
    }

    // Fill instance buffer
    std::span<wis::AccelerationInstance> instances{ instance_buffer.Map<wis::AccelerationInstance>(), objects_count };
    instances[0] = {
        .transform = {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
        },
        .instance_id = 0,
        .mask = 0xFF,
        .instance_offset = 0,
        .flags = 0,
        .acceleration_structure_handle = rt.GetAccelerationStructureDeviceAddress(blas[0]),
    };
    for (uint32_t i = 1; i < objects_count; ++i) {
        instances[i] = {
            .transform = {
                    { 1.0f, 0.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f, 0.0f },
            },
            .instance_id = i,
            .mask = 0xFF,
            .instance_offset = 0,
            .flags = uint32_t(wis::ASInstanceFlags::TriangleCullDisable),
            .acceleration_structure_handle = rt.GetAccelerationStructureDeviceAddress(blas[1]),
        };
    }
    instance_buffer.Unmap();

    // insert barrier
    cmd_list.BufferBarrier({ .sync_before = wis::BarrierSync::BuildRTAS,
                             .sync_after = wis::BarrierSync::BuildRTAS,
                             .access_before = wis::ResourceAccess::AccelerationStructureWrite,
                             .access_after = wis::ResourceAccess::AccelerationStructureRead | wis::ResourceAccess::AccelerationStructureWrite },
                           as_buffer);

    // build top level acceleration structure
    for (int i = 0; i < w::flight_frames; ++i) {
        tlas[i] = rt.CreateAccelerationStructure(result, as_buffer, offset_result, infos[2].result_size, wis::ASLevel::Top);
        rt.BuildTopLevelAS(cmd_list, tlas_desc, tlas[i], scratch_buffer.GetGPUAddress() + offset_scratch);
        offset_result += infos[2].result_size;
        offset_scratch += infos[2].scratch_size;
    }

    cmd_list.Close();
    gfx.ExecuteCommandLists({ cmd_list });
    gfx.WaitForGpu();
}

void w::Scene::CreatePipeline(Graphics& gfx, std::span<wis::DescriptorBindingDesc> bindings)
{
    wis::Result result = wis::success;
    auto& device = gfx.GetDevice();
    auto& rt = gfx.GetRaytracing();

    wis::PushConstant constants[] = {
        { .stage = wis::ShaderStages::All, .size_bytes = sizeof(uint32_t) * 2, .bind_register = 0 },
    };
    wis::PushDescriptor push_desc[] = {
        { .stage = wis::ShaderStages::All, .type = wis::DescriptorType::ConstantBuffer },
    };

    root = device.CreateRootSignature(result, constants, std::size(constants), push_desc, std::size(push_desc), bindings.data(), std::size(bindings));

    // Load shaders
    auto raygen_code = w::LoadShader("shaders/pathtrace.raygen");
    wis::Shader raygen_shader = device.CreateShader(result, raygen_code.data(), raygen_code.size());

    // Create pipeline
    wis::ShaderView shaders[]{
        raygen_shader
    };
    wis::ShaderExport exports[]{
        { .entry_point = "RayGeneration", .shader_type = wis::RaytracingShaderType::Raygen },
        { .entry_point = "Miss", .shader_type = wis::RaytracingShaderType::Miss },
        { .entry_point = "ClosestHit", .shader_type = wis::RaytracingShaderType::ClosestHit },
    };
    wis::HitGroupDesc hit_groups[]{
        { .type = wis::HitGroupType::Triangles, .closest_hit_export_index = 2 },
    };
    wis::RaytracingPipelineDesc rt_pipeline_desc{
        .root_signature = root,
        .shaders = shaders,
        .shader_count = std::size(shaders),
        .exports = exports,
        .export_count = std::size(exports),
        .hit_groups = hit_groups,
        .hit_group_count = std::size(hit_groups),
        .max_recursion_depth = 1,
        .max_payload_size = 24,
        .max_attribute_size = 16,
    };
    pipeline = rt.CreateRaytracingPipeline(result, rt_pipeline_desc);

    // Create shader binding table
    wis::ShaderBindingTableInfo sbt_info = raytracing_extension.GetShaderBindingTableInfo();

    const uint8_t* shader_ident = rt_pipeline.GetShaderIdentifiers();

    1 raygen, 1 miss, 1 hit group sbt_buffer = setup.allocator.CreateBuffer(result, 1024, wis::BufferUsage::ShaderBindingTable, wis::MemoryType::Upload, wis::MemoryFlags::Mapped);
    auto memory = sbt_buffer.Map<uint8_t>();

    // raygen
    uint32_t table_increment = wis::detail::aligned_size(sbt_info.entry_size, sbt_info.table_start_alignment); // not real, just for demonstration

    // copies should have size of entry_size, only the last one should have the size aligned to table_start_alignment
    std::memcpy(memory, shader_ident, sbt_info.entry_size);
    memory += table_increment;

    // miss
    std::memcpy(memory, shader_ident + sbt_info.entry_size, sbt_info.entry_size);
    memory += table_increment;

    // hit group
    std::memcpy(memory, shader_ident + sbt_info.entry_size * 2, sbt_info.entry_size);
    memory += table_increment;
    sbt_buffer.Unmap();

    auto gpu_address = sbt_buffer.GetGPUAddress();

    dispatch_desc.ray_gen_shader_table_address = gpu_address;
    dispatch_desc.miss_shader_table_address = gpu_address + table_increment;
    dispatch_desc.hit_group_table_address = gpu_address + table_increment * 2;
    dispatch_desc.callable_shader_table_address = 0;
    dispatch_desc.ray_gen_shader_table_size = sbt_info.entry_size;
    dispatch_desc.miss_shader_table_size = sbt_info.entry_size;
    dispatch_desc.hit_group_table_size = sbt_info.entry_size;
    dispatch_desc.callable_shader_table_size = 0;
    dispatch_desc.miss_shader_table_stride = sbt_info.entry_size;
    dispatch_desc.hit_group_table_stride = sbt_info.entry_size;
    dispatch_desc.callable_shader_table_stride = 0;
}

void w::Scene::Bind(Graphics& gfx, wis::DescriptorStorage& storage)
{
    auto& rt = gfx.GetRaytracing();
    for (int i = 0; i < w::flight_frames; ++i) {
        rt.WriteAccelerationStructure(storage, 3, i, tlas[i]);
    }
}

void w::Scene::UpdateDispatch(int width, int height)
{
    dispatch_desc.width = width;
    dispatch_desc.height = height;
    dispatch_desc.depth = 1;
}
