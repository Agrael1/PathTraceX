// NVidia ray tracing tutorial #14
#include "shared.hlsli"

static const float PI = 3.14159265359;
// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float NextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}
float2 NextRand2(inout uint s)
{
    return float2(NextRand(s), NextRand(s));
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float origin()
{
    return 1.0f / 32.0f;
}
float float_scale()
{
    return 1.0f / 65536.0f;
}
float int_scale()
{
    return 256.0f;
}

// Normal points outward for rays exiting the surface, else is flipped.
float3 offset_ray(const float3 p, const float3 n)
{
    int3 of_i = int3(int_scale() * n.x, int_scale() * n.y, int_scale() * n.z);

    float3 p_i = float3(
     asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
     asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
     asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin() ? p.x + float_scale() * n.x : p_i.x,
                  abs(p.y) < origin() ? p.y + float_scale() * n.y : p_i.y,
                  abs(p.z) < origin() ? p.z + float_scale() * n.z : p_i.z);
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 GetPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

// Uniform sphere sampling function, y = 1 is the top of the sphere
float3 UniformHemisphereSample(float2 sigma, float3 normal)
{
    float3 bitangent = GetPerpendicularVector(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(max(0.0f, 1.0f - sigma.x * sigma.x));
    float phi = 2.0f * PI * sigma.y;
    
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal * sigma.x;
}

float3 CosineWeightedHemisphereSample(float2 sigma, float3 normal)
{
    float3 bitangent = GetPerpendicularVector(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(sigma.x);
    float phi = 2.0f * PI * sigma.y;

	// Get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal * sqrt(max(0.0, 1.0f - sigma.x));
}

// Schlick's approximation for Fresnel reflection
// R0 is the reflectance at normal incidence
// U is the cosine of the angle between the normal and the incident ray
float3 SchlickFresnel(float3 R0, float U)
{
    return R0 + (float3(1.0f, 1.0f, 1.0f) - R0) * pow(1.0f - U, 5.0f);
}

float3 LagardeFresnel(float3 F0, float U)
{
    return F0 + (float3(1.0f, 1.0f, 1.0f) - F0) * exp2((-5.55473f * U - 6.983146f) * U);
}

float SmithGGX(float NdotV, float NdotL, float roughness)
{
    float k = roughness + 1.0f;
    k = (k * k) / 8.0f;
    
    float G1 = NdotV / (NdotV * (1.0f - k) + k);
    float G2 = NdotL / (NdotL * (1.0f - k) + k);
    return G1 * G2;
}

float ThrowbridgeReitzGGX(float NdotH, float roughness)
{
    float alpha2 = roughness * roughness * roughness * roughness;
    float ndh2 = NdotH * NdotH;
    
    float tail = ndh2 * (alpha2 - 1.0f) + 1.0f;
    float denom = PI * tail * tail;
    
    return alpha2 / denom;
}

float CookTorrance(float NdotV, float NdotL, float NdotH, float VdotH, float LdotH, float roughness, float F0)
{
    float D = ThrowbridgeReitzGGX(NdotH, roughness);
    float G = SmithGGX(NdotV, NdotL, roughness);
    float F = LagardeFresnel(float3(F0, F0, F0), LdotH).x;
    
    return D * G * F / (4.0f * NdotV * NdotL);
}
float EvaluateCookTorrance(float3 N, float3 V, float3 L, float roughness)
{
    float3 H = normalize(V + L);
    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    float LdotH = dot(L, H);
    
    return CookTorrance(NdotV, NdotL, NdotH, VdotH, LdotH, roughness, 1);
}

// GGX microfacet distribution function
// returns a microfacet normal in the hemisphere around the normal
float3 GetGGXMicrofacet(float2 sigma, float3 normal, float roughness)
{
    float3 B = GetPerpendicularVector(normal);
    float3 T = cross(B, normal);

    float a2 = roughness * roughness * roughness * roughness;
    float cosThetaH = sqrt(max(0.0f, (1.0 - sigma.x) / ((a2 - 1.0) * sigma.x + 1)));
    float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
    float phiH = sigma.y * PI * 2.0f;

    return T * (sinThetaH * cos(phiH)) + B * (sinThetaH * sin(phiH)) + normal * cosThetaH;
}

float EvaluateGGXPDF(float3 N, float3 V, float3 L, float roughness)
{
    float3 H = normalize(V + L);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    
    float D = ThrowbridgeReitzGGX(NdotH, roughness);
    return D * NdotH / (4.0f * VdotH);
}

float EvaluateMixBRDF(float3 N, float3 V, float3 L, Material mat)
{
    float3 H = normalize(V + L);
    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    float LdotH = dot(L, H);
    
    float D = ThrowbridgeReitzGGX(NdotH, mat.roughness);
    float G = SmithGGX(NdotV, NdotL, mat.roughness);
    float F = LagardeFresnel(float3(1, 1, 1), LdotH);
    
    float3 specular = D * G * F / (4.0f * NdotV * NdotL);
    float3 diffuse = mat.diffuse.rgb / PI;
    
    return (float3(1.0f, 1.0f, 1.0f) - F) * diffuse + specular;
}

float EvaluateMixPDF(float3 N, float3 V, float3 L, Material mat)
{
    return EvaluateGGXPDF(N, V, L, mat.roughness) + (dot(L, N) / PI) * mat.roughness;
}