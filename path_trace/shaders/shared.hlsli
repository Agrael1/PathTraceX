#pragma once
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
    
    int maxDepth;
    int samplingFn;
    int BRDF;
    bool accumulate;
    int maxIterations;
    bool limitIterations;
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