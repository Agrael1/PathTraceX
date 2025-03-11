#include "sphere.h"
#include "graphics.h"
#include <numbers>
#include <algorithm>
#include <imgui.h>

struct uv_sphere_generator {
    // https://gist.github.com/Pikachuxxxx/5c4c490a7d7679824e0e18af42918efc
    static std::tuple<std::vector<DirectX::XMFLOAT3>, std::vector<DirectX::XMFLOAT3>, std::vector<uint16_t>> generate(uint32_t latitudes, uint32_t longitudes) noexcept
    {
        const float radius = 1.0f;
        std::vector<DirectX::XMFLOAT3> vertices;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT2> uv;
        std::vector<uint16_t> indices;

        float nx, ny, nz, lengthInv = 1.0f / radius; // normal
        // Temporary vertex
        struct Vertex {
            float x, y, z, s, t; // Postion and Texcoords
        };

        float deltaLatitude = std::numbers::pi_v<float> / latitudes;
        float deltaLongitude = 2 * std::numbers::pi_v<float> / longitudes;
        float latitudeAngle;
        float longitudeAngle;

        // Compute all vertices first except normals
        for (int i = 0; i <= latitudes; ++i) {
            latitudeAngle = std::numbers::pi_v<float> / 2 - i * deltaLatitude; /* Starting -pi/2 to pi/2 */
            float xy = radius * cosf(latitudeAngle); /* r * cos(phi) */
            float z = radius * sinf(latitudeAngle); /* r * sin(phi )*/

            /*
             * We add (latitudes + 1) vertices per longitude because of equator,
             * the North pole and South pole are not counted here, as they overlap.
             * The first and last vertices have same position and normal, but
             * different tex coords.
             */
            for (int j = 0; j <= longitudes; ++j) {
                longitudeAngle = j * deltaLongitude;

                Vertex vertex;
                vertex.x = xy * cosf(longitudeAngle); /* x = r * cos(phi) * cos(theta)  */
                vertex.y = xy * sinf(longitudeAngle); /* y = r * cos(phi) * sin(theta) */
                vertex.z = z; /* z = r * sin(phi) */
                vertex.s = (float)j / longitudes; /* s */
                vertex.t = (float)i / latitudes; /* t */
                vertices.push_back(DirectX::XMFLOAT3(vertex.x, vertex.y, vertex.z));
                uv.push_back(DirectX::XMFLOAT2(vertex.s, vertex.t));

                // normalized vertex normal
                nx = vertex.x * lengthInv;
                ny = vertex.y * lengthInv;
                nz = vertex.z * lengthInv;
                normals.push_back(DirectX::XMFLOAT3(nx, ny, nz));
            }
        }

        /*
         *  Indices
         *  k1--k1+1
         *  |  / |
         *  | /  |
         *  k2--k2+1
         */
        unsigned int k1, k2;
        for (int i = 0; i < latitudes; ++i) {
            k1 = i * (longitudes + 1);
            k2 = k1 + longitudes + 1;
            // 2 Triangles per latitude block excluding the first and last longitudes blocks
            for (int j = 0; j < longitudes; ++j, ++k1, ++k2) {
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (latitudes - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
        return { vertices, normals, indices };
    }
};

w::SphereStatic::SphereStatic(w::Graphics& gfx)
{
    using namespace wis;
    auto& device = gfx.GetDevice();
    auto& alloc = gfx.GetAllocator();
    auto& rt = gfx.GetRaytracing();
    wis::Result result = wis::success;

    auto [vertices, normals, indices] = uv_sphere_generator::generate(32, 32);
    list.vertex_count = (uint32_t)vertices.size();
    list.index_count = (uint32_t)indices.size();

    list.vertex_buffer = alloc.CreateBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);
    list.index_buffer = alloc.CreateBuffer(result, list.index_count * sizeof(uint16_t), BufferUsage::IndexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);

    // create staging buffer
    auto staging = alloc.CreateUploadBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3) + list.index_count * sizeof(uint16_t));

    DirectX::XMFLOAT3* vertex_data = staging.Map<DirectX::XMFLOAT3>();
    std::copy(vertices.begin(), vertices.end(), vertex_data);

    uint16_t* index_data = (uint16_t*)(vertex_data + list.vertex_count);
    std::copy(indices.begin(), indices.end(), index_data);
    staging.Unmap();

    // upload data
    auto cmd_list = device.CreateCommandList(result, wis::QueueType::Graphics);
    cmd_list.CopyBuffer(staging, list.vertex_buffer, { .size_bytes = list.vertex_count * sizeof(DirectX::XMFLOAT3) });
    cmd_list.CopyBuffer(staging, list.index_buffer, { .src_offset = list.vertex_count * sizeof(DirectX::XMFLOAT3), .size_bytes = list.index_count * sizeof(uint16_t) });
    cmd_list.Close();

    gfx.ExecuteCommandLists({ cmd_list });
    gfx.WaitForGpu();
}

//
// w::Sphere::Sphere(w::Graphics& gfx)
//{
//    using namespace wis;
//    auto& device = gfx.GetDevice();
//    auto& alloc = gfx.GetAllocator();
//    auto& rt = gfx.GetRaytracing();
//    wis::Result result = wis::success;
//
//    auto [vertices, normals, indices] = uv_sphere_generator::generate(32, 32);
//    vertex_count = (uint32_t)vertices.size();
//    index_count = (uint32_t)indices.size();
//
//    vertex_buffer = alloc.CreateBuffer(result, vertex_count * sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);
//    index_buffer = alloc.CreateBuffer(result, index_count * sizeof(uint16_t), BufferUsage::IndexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);
//    auto staging = alloc.CreateUploadBuffer(result, vertex_count * sizeof(DirectX::XMFLOAT3) + index_count * sizeof(uint16_t));
//
//    // create cbuffers buffer
//    constexpr size_t cbuffer_size = wis::detail::aligned_size(sizeof(InstanceData) * w::flight_frames * 3, 256ull) + 256ull * w::flight_frames * 3; // lazy
//    cbuffer = alloc.CreateBuffer(result, cbuffer_size, BufferUsage::ConstantBuffer, MemoryType::Upload, MemoryFlags::Mapped);
//    mapped_cbuffer = std::span{ cbuffer.Map<InstanceData>(), w::flight_frames * 3 };
//    for (uint32_t i = 0; i < 3; i++) {
//        mapped_cbuffer[i].light_color = { light_color[i] };
//        mapped_cbuffer[i + 3].light_color = { light_color[i] };
//        mapped_cbuffer[i].light_radius = radius;
//        mapped_cbuffer[i + 3].light_radius = radius;
//    }
//
//    DirectX::XMFLOAT3* vertex_data = staging.Map<DirectX::XMFLOAT3>();
//    std::copy(vertices.begin(), vertices.end(), vertex_data);
//
//    uint16_t* index_data = (uint16_t*)(vertex_data + vertex_count);
//    std::copy(indices.begin(), indices.end(), index_data);
//    staging.Unmap();
//
//    // upload data
//    auto cmd_list = device.CreateCommandList(result, wis::QueueType::Graphics);
//    cmd_list.CopyBuffer(staging, vertex_buffer, { .size_bytes = vertex_count * sizeof(DirectX::XMFLOAT3) });
//    cmd_list.CopyBuffer(staging, index_buffer, { .src_offset = vertex_count * sizeof(DirectX::XMFLOAT3), .size_bytes = index_count * sizeof(uint16_t) });
//
//    // slap barriers
//    // clang-format off
//    wis::BufferBarrier2 barriers[]{
//        { .barrier = {
//                  .sync_before = wis::BarrierSync::Copy,
//                  .sync_after = wis::BarrierSync::BuildRTAS,
//                  .access_before = wis::ResourceAccess::CopyDest,
//                  .access_after = wis::ResourceAccess::Common
//          },
//          .buffer = index_buffer },
//        { .barrier = {
//                  .sync_before = wis::BarrierSync::Copy,
//                  .sync_after = wis::BarrierSync::BuildRTAS,
//                  .access_before = wis::ResourceAccess::CopyDest,
//                  .access_after = wis::ResourceAccess::Common
//          },
//          .buffer = vertex_buffer }
//    };
//    // clang-format on
//    cmd_list.BufferBarriers(barriers, std::size(barriers));
//
//    // create blas
//    AcceleratedGeometryInput blas_input{
//        .geometry_type = ASGeometryType::Triangles,
//        .flags = ASGeometryFlags::Opaque,
//        .vertex_or_aabb_buffer_address = vertex_buffer.GetGPUAddress(),
//        .vertex_or_aabb_buffer_stride = sizeof(DirectX::XMFLOAT3),
//        .index_buffer_address = index_buffer.GetGPUAddress(),
//        .vertex_count = vertex_count,
//        .triangle_or_aabb_count = index_count / 3,
//        .vertex_format = wis::DataFormat::RGB32Float,
//        .index_format = wis::IndexType::UInt16,
//    };
//    auto gdesc = CreateGeometryDesc(blas_input);
//
//    wis::BottomLevelASBuildDesc blas_desc{
//        .flags = wis::AccelerationStructureFlags::PreferFastTrace,
//        .geometry_count = 1,
//        .geometry_array = { &gdesc }
//    };
//    auto info = rt.GetBottomLevelASSize(blas_desc);
//
//    as_buffer = alloc.CreateBuffer(result, info.result_size * 3, BufferUsage::AccelerationStructureBuffer);
//    auto scratch = alloc.CreateBuffer(result, info.scratch_size * 3, BufferUsage::StorageBuffer);
//
//    for (uint32_t i = 0; i < 3; i++) {
//        blas[i] = rt.CreateAccelerationStructure(result, as_buffer, i * info.result_size, info.result_size, ASLevel::Bottom);
//        rt.BuildBottomLevelAS(cmd_list, blas_desc, blas[i], scratch.GetGPUAddress() + i * info.scratch_size);
//    }
//
//    cmd_list.Close();
//
//    gfx.ExecuteCommandLists({ cmd_list });
//    gfx.WaitForGpu();
//}
//
//
// void w::Sphere::DepthPass(wis::CommandList& cl, uint32_t frame_index) const
//{
//    using namespace DirectX;
//    constexpr static uint32_t offset = wis::detail::aligned_size(sizeof(InstanceData) * w::flight_frames * 3, 256ull);
//    DirectX::XMMATRIX* transform_0 = (DirectX::XMMATRIX*)((uint8_t*)mapped_cbuffer.data() + offset);
//    DirectX::XMMATRIX* transform_1 = (DirectX::XMMATRIX*)((uint8_t*)transform_0 + 256);
//    DirectX::XMMATRIX* transform_2 = (DirectX::XMMATRIX*)((uint8_t*)transform_1 + 256);
//
//    auto moffset = XMMatrixTranslation(2.0f, -2.0f, 0.0f);
//    auto rotation = XMMatrixRotationY(rotation_y);
//    auto scale = XMMatrixScaling(radius, radius, radius);
//    transform_0[frame_index] = scale * moffset * rotation;
//
//    cl.PushDescriptor(wis::DescriptorType::ConstantBuffer, 1, cbuffer, offset);
//    cl.IASetIndexBuffer(index_buffer, wis::IndexType::UInt16, 0);
//    wis::VertexBufferBinding vb{
//        .buffer = vertex_buffer,
//        .size = vertex_count * sizeof(DirectX::XMFLOAT3),
//        .stride = sizeof(DirectX::XMFLOAT3),
//        .offset = 0
//    };
//    cl.IASetVertexBuffers(&vb, 1);
//    cl.DrawIndexedInstanced(index_count);
//
//    rotation = XMMatrixRotationY(rotation_y + 2.f * std::numbers::pi_v<float> / 3.f);
//    transform_1[frame_index] = scale * moffset * rotation;
//
//    cl.PushDescriptor(wis::DescriptorType::ConstantBuffer, 1, cbuffer, offset + 256);
//    cl.DrawIndexedInstanced(index_count);
//
//    rotation = XMMatrixRotationY(rotation_y + 4.f * std::numbers::pi_v<float> / 3.f);
//    transform_2[frame_index] = scale * moffset * rotation;
//
//    cl.PushDescriptor(wis::DescriptorType::ConstantBuffer, 1, cbuffer, offset + 512);
//    cl.DrawIndexedInstanced(index_count);
//}
//
// void w::Sphere::PutTransform(DirectX::XMFLOAT3X4* transform1,
//                             DirectX::XMFLOAT3X4* transform2,
//                             DirectX::XMFLOAT3X4* transform3) const
//{
//    using namespace DirectX;
//    static constexpr float angle_step = 2.f * std::numbers::pi_v<float> / 3.f;
//
//    auto offset = XMMatrixTranslation(2.0f, -2.0f, 0.0f);
//    auto rotation = XMMatrixRotationY(rotation_y);
//    auto scale = XMMatrixScaling(radius, radius, radius);
//    XMStoreFloat3x4(transform1, scale * offset * rotation);
//
//    rotation = XMMatrixRotationY(rotation_y + angle_step);
//    XMStoreFloat3x4(transform2, scale * offset * rotation);
//
//    rotation = XMMatrixRotationY(rotation_y + 2.f * angle_step);
//    XMStoreFloat3x4(transform3, scale * offset * rotation);
//}
//
// void w::Sphere::RenderMaterialUI(SphereCBuffer& out_data)
//{
//
//}
//
// void w::Sphere::AddLightRadius(float r)
//{
//    radius += r;
//    radius = std::clamp(radius, 0.05f, 2.0f);
//    for (uint32_t i = 0; i < 3; i++) {
//        mapped_cbuffer[i].light_radius = radius;
//        mapped_cbuffer[i + 3].light_radius = radius;
//    }
//}

w::BoxStatic::BoxStatic(w::Graphics& gfx)
{
    using namespace wis;
    auto& device = gfx.GetDevice();
    auto& alloc = gfx.GetAllocator();
    auto& rt = gfx.GetRaytracing();
    wis::Result result = wis::success;

    constexpr auto vertices = std::array{
        DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f),
        DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f),
        DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f),
        DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f),
        DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f),
        DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f),
        DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f),
    };

    // clang-format off
    constexpr auto indices = std::array{
        0, 1, 2, 2, 1, 3, 4, 6, 5, 5, 6, 7, 0, 2, 4, 4, 2, 6, 1, 5, 3, 3, 5, 7, 0, 4, 1, 1, 4, 5, 2, 3, 6, 6, 3, 7,
    };
    // clang-format on

    list.vertex_count = (uint32_t)vertices.size();
    list.index_count = (uint32_t)indices.size();

    list.vertex_buffer = alloc.CreateBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);
    list.index_buffer = alloc.CreateBuffer(result, list.index_count * sizeof(uint16_t), BufferUsage::IndexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);

    // create staging buffer
    auto staging = alloc.CreateUploadBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3) + list.index_count * sizeof(uint16_t));

    DirectX::XMFLOAT3* vertex_data = staging.Map<DirectX::XMFLOAT3>();
    std::copy(vertices.begin(), vertices.end(), vertex_data);

    uint16_t* index_data = (uint16_t*)(vertex_data + list.vertex_count);
    std::copy(indices.begin(), indices.end(), index_data);
    staging.Unmap();

    // upload data
    auto cmd_list = device.CreateCommandList(result, wis::QueueType::Graphics);
    cmd_list.CopyBuffer(staging, list.vertex_buffer, { .size_bytes = list.vertex_count * sizeof(DirectX::XMFLOAT3) });
    cmd_list.CopyBuffer(staging, list.index_buffer, { .src_offset = list.vertex_count * sizeof(DirectX::XMFLOAT3), .size_bytes = list.index_count * sizeof(uint16_t) });
    cmd_list.Close();

    gfx.ExecuteCommandLists({ cmd_list });
    gfx.WaitForGpu();
}

w::ObjectView::ObjectView(std::string name, ObjectCBuffer mat)
    : name(std::move(name))
    , material(mat)
{
}

void w::ObjectView::RenderMatrialUI(ObjectCBuffer& out_data)
{
    ImGui::Begin(name.c_str(), nullptr);
    ImGui::PushItemWidth(150);

    bool updated = false;

    updated |= ImGui::ColorEdit3("Albedo", reinterpret_cast<float*>(&material.diffuse));
    updated |= ImGui::ColorEdit3("Emission", reinterpret_cast<float*>(&material.emissive));
    updated |= ImGui::SliderFloat("Roughness", reinterpret_cast<float*>(&material.roughness), 0.001f, 1.0f);
    ImGui::End();

    if (updated) {
        out_data = material;
    }
}
