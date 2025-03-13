#pragma once
#include <wisdom/wisdom_raytracing.hpp>
#include <DirectXMath.h>
#include <string>
#include <wisdom/wisdom.hpp>

namespace w {
class Graphics;

// cbuffer for sphere
struct alignas(alignof(DirectX::XMFLOAT4A)) MaterialCBuffer {
    DirectX::XMFLOAT4A diffuse;
    DirectX::XMFLOAT4A emissive;
    float roughness;
};
struct alignas(alignof(DirectX::XMFLOAT4A)) ObjectData {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 scale;
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

public:
    IndexedTriangleList list;
};

class BoxStatic
{
public:
    BoxStatic(w::Graphics& gfx);

public:
    IndexedTriangleList list;
};

struct ObjectView {
public:
    bool RenderObjectUI(MaterialCBuffer& out_data, wis::AccelerationInstance& instance_data);

    void GatherInstanceTransform(wis::AccelerationInstance& instance) const;

public:
    MaterialCBuffer material{};
    ObjectData data{};
    std::string name;
};
} // namespace w