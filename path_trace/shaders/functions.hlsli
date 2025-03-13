// NVidia ray tracing tutorial #14

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