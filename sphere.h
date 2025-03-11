#pragma once
#include <wisdom/wisdom_raytracing.hpp>
#include <DirectXMath.h>
#include <string>

namespace w {
class Graphics;

// cbuffer for sphere
struct alignas(alignof(DirectX::XMFLOAT4A)) ObjectCBuffer {
    DirectX::XMFLOAT4A diffuse;
    DirectX::XMFLOAT4A emissive;
    DirectX::XMFLOAT3 pos;
    float scale;

    float roughness;
};

struct IndexedTriangleList {
    wis::Buffer vertex_buffer;
    wis::Buffer index_buffer;

    uint32_t vertex_count;
    uint32_t index_count;
};

class SphereStatic
{
public:
    SphereStatic(w::Graphics& gfx);

private:
    IndexedTriangleList list;
};

class BoxStatic
{
public:
    BoxStatic(w::Graphics& gfx);

private:
    IndexedTriangleList list;
};

class ObjectView
{
public:
    ObjectView() = default;
    ObjectView(std::string name, ObjectCBuffer mat);

public:
    void RenderMatrialUI(ObjectCBuffer& out_data);

private:
    wis::AccelerationStructure blas{};
    ObjectCBuffer material{};
    std::string name;
};
} // namespace w