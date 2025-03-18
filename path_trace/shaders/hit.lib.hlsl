#include "shared.hlsli"
#include "functions.hlsli"

[[vk::push_constant]] ConstantBuffer<FrameIndex> frameIndex : register(b4);
[[vk::binding(0, 0)]] ConstantBuffer<FrameCBuffer> camera : register(b0);
[[vk::binding(1, 0)]] ConstantBuffer<MaterialCBuffer> materials : register(b1);
[[vk::binding(0, 3)]] RWTexture2D<float4> image[] : register(u0, space3);
[[vk::binding(0, 4)]] RaytracingAccelerationStructure scene[] : register(t0, space4);

[[vk::binding(0, 5)]] StructuredBuffer<float3> sphere_nrm[] : register(t0, space5); // bindings 0 is vn
[[vk::binding(0, 5)]] StructuredBuffer<uint> indices[] : register(t0, space6); // overloading binding 5 for indices, 1 is indices

static const float3 faceNormalsBox[] = {
    float3(0, 0, 1),
    float3(0, 0, -1),
    float3(1, 0, 0),
    float3(-1, 0, 0),
    float3(0, -1, 0),
    float3(0, 1, 0),
};

float3 SampleSelect(float2 sigma, float3 normal, float roughness, float bias)
{
    switch (frameIndex.samplingFn) {
    default:
    case 0:
        return UniformHemisphereSample(sigma, normal);
    case 1:
        return CosineWeightedHemisphereSample(sigma, normal);
    case 2:
    case 3:
        return reflect(WorldRayDirection(), GetGGXMicrofacet(sigma, normal, roughness));
        return bias < roughness
                ? reflect(WorldRayDirection(), GetGGXMicrofacet(sigma, normal, roughness))
                : CosineWeightedHemisphereSample(sigma, normal);
    }
}
float PDFSelect(Material mat, float3 V, float3 L, float3 N, float bias)
{
    switch (frameIndex.samplingFn) {
    default:
    case 0:
        return 1.0 / (2.0 * PI);
    case 1:
        return dot(L, N) / PI;
    case 2:
        return EvaluateGGXPDF(N, V, L, mat.roughness);
    case 3:
        return mat.roughness < bias ? EvaluateGGXPDF(N, V, L, mat.roughness) : dot(L, N) / PI * mat.roughness;
    }
}

float3 ComputeBRDF(Material mat, float3 V, float3 L, float3 N, float bias)
{
    switch (frameIndex.BRDF) {
    default:
    case 0:
        return float3(1.0 / PI, 1.0 / PI, 1.0 / PI);
    case 1:
        return mat.diffuse.rgb / PI;
    case 2:
        return mat.diffuse.rgb * EvaluateCookTorrance(N, V, L, mat.roughness);
    case 3:
        return mat.roughness < bias ? EvaluateCookTorrance(N, V, L, mat.roughness) : (mat.diffuse.rgb / PI * mat.roughness);
    }
}

[shader("closesthit")] void ClosestHit_Box(inout Payload payload,
                                           BuiltInTriangleIntersectionAttributes attrib) {
    uint instanceID = InstanceID();
    Material mat = materials.materials[instanceID];
    if (payload.depth >= frameIndex.maxDepth) {
        payload.color = mat.emissive.rgb;
        return;
    }

    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint faceID = PrimitiveIndex();

    float bias = saturate(NextRand(payload.randSeed) + 0.01);
    float3 normal = faceNormalsBox[faceID / 2];
    float3 sample = SampleSelect(NextRand2(payload.randSeed), normal, mat.roughness, bias);

    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 newDir = normalize(sample);
    float3 V = -normalize(WorldRayDirection());

    RayDesc rayDesc;
    rayDesc.Origin = offset_ray(hitPoint, normal);
    rayDesc.Direction = newDir;
    rayDesc.TMin = 0;
    rayDesc.TMax = 1000.0;

    payload.depth++;
    TraceRay(scene[frameIndex.frameIndex], RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    float3 brdf = ComputeBRDF(mat, V, newDir, normal, bias);
    float cosTheta = max(dot(normal, newDir), 0.0);
    payload.color = payload.color * brdf * cosTheta / PDFSelect(mat, V, newDir, normal, bias);
}

        [shader("closesthit")] void ClosestHit(inout Payload payload,
                                               BuiltInTriangleIntersectionAttributes attrib)
{
    uint instance = InstanceID();
    Material mat = materials.materials[instance];
    if (any(mat.emissive != float4(0, 0, 0, 0)) || payload.depth >= frameIndex.maxDepth) {
        payload.color = mat.emissive.rgb;
        return;
    }

    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint faceID = PrimitiveIndex();

    const uint3 aindices = uint3(
            indices[1][faceID * 3 + 0],
            indices[1][faceID * 3 + 1],
            indices[1][faceID * 3 + 2]);

    const float3 vn[3] = {
        sphere_nrm[0][aindices.x],
        sphere_nrm[0][aindices.y],
        sphere_nrm[0][aindices.z],
    };

    float bias = saturate(NextRand(payload.randSeed) + 0.01);
    float3 normal = normalize(HitAttribute(vn, attrib));
    float3 sample = SampleSelect(NextRand2(payload.randSeed), normal, mat.roughness, bias);
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 newDir = normalize(sample);
    float3 V = -normalize(WorldRayDirection());

    RayDesc rayDesc;
    rayDesc.Origin = offset_ray(hitPoint, normal);
    rayDesc.Direction = newDir;
    rayDesc.TMin = 0;
    rayDesc.TMax = 1000.0;

    payload.depth++;
    TraceRay(scene[frameIndex.frameIndex], RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    float3 brdf = ComputeBRDF(mat, V, newDir, normal, bias);
    float cosTheta = max(dot(normal, newDir), 0.0);
    payload.color = payload.color * brdf * cosTheta / PDFSelect(mat, V, newDir, normal, bias);
}