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
using namespace DirectX;
// Function to calculate the normal of a face given three vertices
XMFLOAT3 CalculateNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
    XMVECTOR edge1 = XMLoadFloat3(&v1) - XMLoadFloat3(&v0);
    XMVECTOR edge2 = XMLoadFloat3(&v2) - XMLoadFloat3(&v0);
    XMVECTOR normal = XMVector3Cross(edge1, edge2);
    normal = XMVector3Normalize(normal);

    XMFLOAT3 normalFloat3;
    XMStoreFloat3(&normalFloat3, normal);
    return normalFloat3;
}



w::BoxStatic::BoxStatic(w::Graphics& gfx)
{
    using namespace wis;
    auto& device = gfx.GetDevice();
    auto& alloc = gfx.GetAllocator();
    auto& rt = gfx.GetRaytracing();
    wis::Result result = wis::success;

    constexpr DirectX::XMFLOAT3 vertices[] = {
        { -0.5f, -0.5f, -0.5f }, // 0
        { -0.5f, 0.5f, -0.5f }, // 1
        { 0.5f, 0.5f, -0.5f }, // 2
        { 0.5f, -0.5f, -0.5f }, // 3
        { -0.5f, -0.5f, 0.5f }, // 4
        { -0.5f, 0.5f, 0.5f }, // 5
        { 0.5f, 0.5f, 0.5f }, // 6
        { 0.5f, -0.5f, 0.5f } // 7
    };

    // Define the indices for the box faces (counter-clockwise with inverted normals)
    constexpr uint16_t indices[] = {
        // Front face
        2, 0, 1,
        3, 0, 2,
        // Back face
        5, 4, 6,
        6, 4, 7,
        // Left face
        1, 0, 5,
        5, 0, 4,
        // Right face
        7, 3, 6,
        6, 3, 2,
        // Top face
        2, 1, 6,
        6, 1, 5,
        // Bottom face
        4, 0, 7,
        7, 0, 3
    };

    list.vertex_count = (uint32_t)std::size(vertices);
    list.index_count = (uint32_t)std::size(indices);

    list.vertex_buffer = alloc.CreateBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);
    list.index_buffer = alloc.CreateBuffer(result, list.index_count * sizeof(uint16_t), BufferUsage::IndexBuffer | BufferUsage::CopyDst | BufferUsage::AccelerationStructureInput);

    // create staging buffer
    auto staging = alloc.CreateUploadBuffer(result, list.vertex_count * sizeof(DirectX::XMFLOAT3) + list.index_count * sizeof(uint16_t));

    DirectX::XMFLOAT3* vertex_data = staging.Map<DirectX::XMFLOAT3>();
    std::copy(std::begin(vertices), std::end(vertices), vertex_data);

    uint16_t* index_data = (uint16_t*)(vertex_data + list.vertex_count);
    std::copy(std::begin(indices), std::end(indices), index_data);
    staging.Unmap();

    // upload data
    auto cmd_list = device.CreateCommandList(result, wis::QueueType::Graphics);
    cmd_list.CopyBuffer(staging, list.vertex_buffer, { .size_bytes = list.vertex_count * sizeof(DirectX::XMFLOAT3) });
    cmd_list.CopyBuffer(staging, list.index_buffer, { .src_offset = list.vertex_count * sizeof(DirectX::XMFLOAT3), .size_bytes = list.index_count * sizeof(uint16_t) });
    cmd_list.Close();

    gfx.ExecuteCommandLists({ cmd_list });
    gfx.WaitForGpu();
}

bool w::ObjectView::RenderObjectUI(MaterialCBuffer& out_data, wis::AccelerationInstance& instance_data)
{
    using namespace DirectX;
    ImGui::Begin(name.c_str(), nullptr);
    ImGui::PushItemWidth(150);

    bool updated = false;
    bool updated_instance = false;

    updated |= ImGui::ColorEdit3("Albedo", reinterpret_cast<float*>(&material.diffuse));
    updated |= ImGui::ColorEdit3("Emission", reinterpret_cast<float*>(&material.emissive));
    updated |= ImGui::SliderFloat("Roughness", reinterpret_cast<float*>(&material.roughness), 0.001f, 1.0f);

    updated_instance |= ImGui::DragFloat3("Position", reinterpret_cast<float*>(&data.pos), 0.01f);
    updated_instance |= ImGui::DragFloat("Scale", &data.scale, 0.01f, 0.01, 15);
    ImGui::End();

    if (updated) {
        out_data = material;
    }
    if (updated_instance) {
        GatherInstanceTransform(instance_data);
    }

    return updated_instance;
}

void w::ObjectView::GatherInstanceTransform(wis::AccelerationInstance& instance) const
{
    using namespace DirectX;
    XMMATRIX transform = XMMatrixScaling(data.scale, data.scale, data.scale) * XMMatrixTranslation(data.pos.x, data.pos.y, data.pos.z);
    XMStoreFloat3x4((DirectX::XMFLOAT3X4*)&instance.transform, transform);
}
