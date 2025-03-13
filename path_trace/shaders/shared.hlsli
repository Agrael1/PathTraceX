struct Payload
{
    float3 color;
    uint depth;
    
    uint randSeed;
    bool allowReflection;
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
struct RenderingConstants
{
    uint maxDepth;
    uint samplingFn;
    uint BRDF;
};