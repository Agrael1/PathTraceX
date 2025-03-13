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

// Uniform sphere sampling function, y = 1 is the top of the sphere
float3 UniformHemisphereSample(float2 uv)
{
    float phi = 2 * PI * uv.x;
    float cosTheta = uv.y;
    float sinTheta = sqrt(1 - cosTheta * cosTheta);
    return float3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

float3 TransformSample(float3 sample, float3 normal)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    
    float3 binormal = cross(normal, up);
    float3 tangent = cross(binormal, normal);
    return sample.x * tangent + sample.y * normal + sample.z * binormal;
}
