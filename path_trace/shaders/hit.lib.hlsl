#include "shared.hlsli"
#include "functions.hlsli"

[[vk::push_constant]] ConstantBuffer<FrameIndex> frameIndex : register(b4);
[[vk::binding(0, 0)]] ConstantBuffer<FrameCBuffer> camera : register(b0);
[[vk::binding(1, 0)]] ConstantBuffer<MaterialCBuffer> materials : register(b1);
[[vk::binding(0, 3)]] RWTexture2D<float4> image[] : register(u0, space3);
[[vk::binding(0, 4)]] RaytracingAccelerationStructure scene[] : register(t0, space4);

static const float3 faceNormalsBox[] = {
    float3(0, 0, 1),
    float3(0, 0, -1),
    float3(1, 0, 0),
    float3(-1, 0, 0),
    float3(0, -1, 0),
    float3(0, 1, 0),
};

[shader("closesthit")] void ClosestHit_Box(inout Payload payload,
                                           BuiltInTriangleIntersectionAttributes attrib) {
    uint instanceID = InstanceID();
    Material mat = materials.materials[instanceID];
    if (payload.depth > 3) {
        payload.color = mat.diffuse;
        return;
    }

    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint faceID = PrimitiveIndex();
    float3 normal = faceNormalsBox[faceID / 2];

    uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, frameIndex.frameCount, 16);

    float3 sample = TransformSample(UniformHemisphereSample(NextRand2(randSeed)), normal);
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 newDir = normalize(sample);

    RayDesc rayDesc;
    rayDesc.Origin = hitPoint + newDir * 0.001;
    rayDesc.Direction = newDir;
    rayDesc.TMin = 0.001;
    rayDesc.TMax = 1000.0;

    payload.depth++;
    TraceRay(scene[0], RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    payload.color = payload.color;
}

        [shader("closesthit")] void ClosestHit(inout Payload payload,
                                               BuiltInTriangleIntersectionAttributes attrib)
{

    uint instance = InstanceID();
    Material mat = materials.materials[instance];
    payload.color = mat.diffuse.rgb;
}