struct Payload
{
    float3 color;
    bool allowReflection;
    bool hit; // for shadows
    uint depth;
};
struct FrameIndex
{
    uint frameIndex;
    uint frameCount;
};
struct FrameCBuffer
{
    matrix view;
    matrix projection;
    matrix viewProjection;
    matrix invView;
    matrix invProjection;
};

struct Material {
    float4 diffuse;
    float4 emissive;
    float roughness;
};

struct MaterialCBuffer
{
    Material materials[5];
};