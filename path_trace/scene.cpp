#include "scene.h"
#include "graphics.h"
#include <imgui.h>

static constexpr const char* SAMPLING_LABELS[4] = { "Uniform", "Cosine", "GGX", "Mix" };
static constexpr const char* BRDF_LABELS[4] = { "Lambert", "LambertWithAlbedo", "GGX", "Mix" };

w::Scene::Scene(Graphics& gfx, wis::Result result)
    : object_cbuffer(gfx.allocator.CreateUploadBuffer(result, sizeof(MaterialCBuffer) * objects_count))
    , instance_buffer(gfx.allocator.CreateUploadBuffer(result, sizeof(wis::AccelerationInstance) * objects_count))
    , camera_buffer(gfx.allocator.CreateUploadBuffer(result, sizeof(w::Camera::CBuffer)))
    , sphere_static(gfx)
    , box_static(gfx)
    , mapped_cbuffer(object_cbuffer.Map<MaterialCBuffer>(), objects_count)
{
    // Box
    object_views[0] = {
        .material = {
                .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
                .emissive = {},
                .roughness = 1,
        },
        .data = { { 0, 0, 0 }, 25 },
        .name = "Box",
    };

    constexpr DirectX::XMFLOAT4A sphere_colors[3] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f },
    };
    constexpr DirectX::XMFLOAT4A sphere_pos[3] = {
        {0, 2, 0, 4  },
        {-10, 2, 2, 2},
        {5, 0, 10, 2 },
    };

    // Light
    object_views[1] = {
        .material = {
                .diffuse = { 1, 1, 1, 1 },
                .emissive = { 1, 1, 1, 1 },
                .roughness = 1,
        },
        .data = { { 0, 0, 0 }, 2 },
        .name = wis::format("Sphere {}", 1),
    };

    for (int i = 2; i < objects_count; ++i) {
        object_views[i] = {
            .material = {
                    .diffuse = sphere_colors[i - 2],
                    .emissive = {},
                    .roughness = 1,
            },
            .data = { { sphere_pos[i - 2].x, sphere_pos[i - 2].y, sphere_pos[i - 2].z }, sphere_pos[i - 2].w },
            .name = wis::format("Sphere {}", i),
        };
    }

    // create mapped instances
    mapped_instances = { instance_buffer.Map<wis::AccelerationInstance>(), objects_count };
    mapped_instances[0] = {
        .instance_id = 0,
        .mask = 0xFF,
        .instance_offset = 1,
        .flags = uint32_t(wis::ASInstanceFlags::TriangleFrontCounterClockwise),
        .acceleration_structure_handle = 0,
    };
    object_views[0].GatherInstanceTransform(mapped_instances[0]);

    for (uint32_t i = 1; i < objects_count; ++i) {
        mapped_instances[i] = {
            .instance_id = i,
            .mask = 0xFF,
            .instance_offset = 0,
            .flags = uint32_t(wis::ASInstanceFlags::TriangleCullDisable),
            .acceleration_structure_handle = 0,
        };
        object_views[i].GatherInstanceTransform(mapped_instances[i]);
    }

    mapped_camera = { camera_buffer.Map<w::Camera::CBuffer>(), w::flight_frames };

    // set material cbuffer
    for (uint32_t i = 0; i < objects_count; ++i) {
        mapped_cbuffer[i] = object_views[i].material;
    }

    CreateAccelerationStructures(gfx);
}

w::Scene::~Scene()
{
    object_cbuffer.Unmap();
    instance_buffer.Unmap();
    camera_buffer.Unmap();
}

void w::Scene::RenderUI()
{
    ImGui::Begin("Settings", nullptr);
    ImGui::PushItemWidth(150);
    ImGui::Text(wis::format("FPS (CPU): {}", ImGui::GetIO().Framerate).c_str());
    ImGui::Text(wis::format("Iterations: {}", iterations).c_str());

    ImGui::Checkbox("Show Material Box", &show_material_window[0]);
    ImGui::Checkbox("Show Material Sph#1", &show_material_window[1]);
    ImGui::Checkbox("Show Material Sph#2", &show_material_window[2]);
    ImGui::Checkbox("Show Material Sph#3", &show_material_window[3]);
    ImGui::Checkbox("Show Material Sph#4", &show_material_window[4]);

    ImGui::Checkbox("Accumulate", &accumulate);
    ImGui::Checkbox("Gama Correction", &gamma_correction);
    if (ImGui::Checkbox("Limit Iterations", &limit_max_iterations)) {
        // clear();
    }
    if (ImGui::SliderInt("Max Iterations", &max_iterations, 1, 1000)) {
        // clear();
    }

    if (ImGui::Combo("Sampling", &current_sampling, SAMPLING_LABELS, IM_ARRAYSIZE(SAMPLING_LABELS))) {
        // clear();
    }
    if (ImGui::Combo("BRDF", &current_brdf, BRDF_LABELS, IM_ARRAYSIZE(BRDF_LABELS))) {
        // clear();
    }
    if (ImGui::SliderInt("Bounces", &bounces, 1, 24)) {
        // clear();
    }
    ImGui::End();

    bool updated_tlas = false;
    for (int m = 0; m < objects_count; ++m) {
        if (show_material_window[m]) {
            updated_tlas |= object_views[m].RenderObjectUI(mapped_cbuffer[m], mapped_instances[m]);
        }
    }
    if (updated_tlas) {
        for (int i = 0; i < w::flight_frames; ++i) {
            update_tlas[i] = true;
        }
    }
}

void w::Scene::RenderScene(Graphics& gfx, wis::CommandList& cmd_list, wis::DescriptorStorageView dstorage, uint32_t current_frame)
{
    using namespace wis;
    camera.PutCBuffer(mapped_camera.data() + current_frame);
    auto& rt = gfx.GetRaytracing();

    if (update_tlas[current_frame]) {
        wis::TopLevelASBuildDesc tlas_desc{
            .flags = wis::AccelerationStructureFlags::PreferFastTrace | wis::AccelerationStructureFlags::AllowUpdate,
            .instance_count = objects_count,
            .gpu_address = instance_buffer.GetGPUAddress(),
            .update = true,
        };

        uint32_t offset_scratch = tlas_update_size * current_frame;
        rt.BuildTopLevelAS(cmd_list, tlas_desc, tlas[current_frame], scratch_buffer.GetGPUAddress() + offset_scratch, tlas[current_frame]);
        update_tlas[current_frame] = false;

        // insert barrier
        wis::BufferBarrier barrier{
            .sync_before = wis::BarrierSync::BuildRTAS,
            .sync_after = wis::BarrierSync::Raytracing,
            .access_before = wis::ResourceAccess::Common,
            .access_after = wis::ResourceAccess::UnorderedAccess,
        };
        cmd_list.BufferBarrier(barrier, as_buffer);
    }

    rt.SetPipelineState(cmd_list, pipeline);
    cmd_list.SetComputeRootSignature(root);

    std::pair<uint32_t, uint32_t> frame_ref = { current_frame, frame_count++ };
    cmd_list.SetComputePushConstants(&frame_ref, 2, 0);
    rt.PushDescriptor(cmd_list, wis::DescriptorType::ConstantBuffer, 0, camera_buffer, 0);
    rt.PushDescriptor(cmd_list, wis::DescriptorType::ConstantBuffer, 1, object_cbuffer, 0);
    rt.SetDescriptorStorage(cmd_list, dstorage);

    rt.DispatchRays(cmd_list, dispatch_desc);
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
    scratch_buffer = gfx.allocator.CreateBuffer(result, infos[0].scratch_size + infos[1].scratch_size + infos[2].scratch_size * w::flight_frames, wis::BufferUsage::StorageBuffer);
    tlas_update_size = infos[2].update_size;

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

    for (int i = 0; i < objects_count; ++i) {
        mapped_instances[i].acceleration_structure_handle = blas[i != 0];
    }

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
    auto& allocator = gfx.GetAllocator();

    wis::PushConstant constants[] = {
        { .stage = wis::ShaderStages::All, .size_bytes = sizeof(uint32_t) * 2, .bind_register = 4 },
    };
    wis::PushDescriptor push_desc[] = {
        { .stage = wis::ShaderStages::All, .type = wis::DescriptorType::ConstantBuffer },
        { .stage = wis::ShaderStages::All, .type = wis::DescriptorType::ConstantBuffer },
    };

    root = device.CreateRootSignature(result, constants, std::size(constants), push_desc, std::size(push_desc), bindings.data(), std::size(bindings));

    // Load shaders
    auto raygen_code = w::LoadShader("shaders/pathtrace.lib");
    wis::Shader raygen_shader = device.CreateShader(result, raygen_code.data(), raygen_code.size());

    auto hit_code = w::LoadShader("shaders/hit.lib");
    wis::Shader hit_shader = device.CreateShader(result, hit_code.data(), hit_code.size());

    // Create pipeline
    wis::ShaderView shaders[]{
        raygen_shader, hit_shader
    };
    wis::ShaderExport exports[]{
        { .entry_point = "RayGeneration", .shader_type = wis::RaytracingShaderType::Raygen },
        { .entry_point = "Miss", .shader_type = wis::RaytracingShaderType::Miss },
        { .entry_point = "ClosestHit", .shader_type = wis::RaytracingShaderType::ClosestHit, .shader_array_index = 1 },
        { .entry_point = "ClosestHit_Box", .shader_type = wis::RaytracingShaderType::ClosestHit, .shader_array_index = 1 },
    };
    wis::HitGroupDesc hit_groups[]{
        { .type = wis::HitGroupType::Triangles, .closest_hit_export_index = 2 },
        { .type = wis::HitGroupType::Triangles, .closest_hit_export_index = 3 },
    };
    wis::RaytracingPipelineDesc rt_pipeline_desc{
        .root_signature = root,
        .shaders = shaders,
        .shader_count = std::size(shaders),
        .exports = exports,
        .export_count = std::size(exports),
        .hit_groups = hit_groups,
        .hit_group_count = std::size(hit_groups),
        .max_recursion_depth = 5,
        .max_payload_size = 24,
        .max_attribute_size = 16,
    };
    pipeline = rt.CreateRaytracingPipeline(result, rt_pipeline_desc);

    // Create shader binding table
    wis::ShaderBindingTableInfo sbt_info = rt.GetShaderBindingTableInfo();

    const uint8_t* shader_ident = pipeline.GetShaderIdentifiers();

    // 1 raygen, 1 miss, 1 hit group
    sbt_buffer = allocator.CreateBuffer(result, sbt_info.table_start_alignment * 4, wis::BufferUsage::ShaderBindingTable, wis::MemoryType::GPUUpload, wis::MemoryFlags::Mapped);
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
    std::memcpy(memory, shader_ident + sbt_info.entry_size * 2, sbt_info.entry_size * std::size(hit_groups));
    memory += table_increment;
    sbt_buffer.Unmap();

    auto gpu_address = sbt_buffer.GetGPUAddress();

    dispatch_desc.ray_gen_shader_table_address = gpu_address;
    dispatch_desc.miss_shader_table_address = gpu_address + table_increment;
    dispatch_desc.hit_group_table_address = gpu_address + table_increment * 2;
    dispatch_desc.callable_shader_table_address = 0;
    dispatch_desc.ray_gen_shader_table_size = sbt_info.entry_size;
    dispatch_desc.miss_shader_table_size = sbt_info.entry_size;
    dispatch_desc.hit_group_table_size = sbt_info.entry_size * std::size(hit_groups);
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

    camera.SetPerspective(std::numbers::pi_v<float> / 3.0f, float(width) / float(height), 0.1f, 1000.0f);
}

void w::Scene::RotateCamera(float dx, float dy)
{
    camera.Rotate(dx * 0.05f, dy * 0.05f);
}

void w::Scene::ZoomCamera(float dz)
{
    camera.Zoom(dz);
}