#version 450

layout(set = 0, binding = 0) uniform sampler2D gPosition; // View-space positions
layout(set = 0, binding = 1) uniform sampler2D gNormal;   // View-space normals
layout(set = 0, binding = 2) uniform sampler2D noiseTex;   // 4x4 random rotation vectors

layout(set = 0, binding = 3) uniform SSAOParams {
    vec4 samples[64]; // Hemisphere kernel samples
    mat4 projection;
    float radius;
    float bias;
    int kernelSize;
} params;

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outOcclusion;

void main() {
    vec2 noiseScale = textureSize(gPosition, 0) / vec2(4.0);

    vec3 fragPos = texture(gPosition, inUV).xyz;
    vec3 normal = normalize(texture(gNormal, inUV).xyz);
    vec3 randomVec = normalize(texture(noiseTex, inUV * noiseScale).xyz);

    // Construct TBN to orient kernel hemisphere along surface normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < params.kernelSize; i++) {
        vec3 samplePos = fragPos + TBN * params.samples[i].xyz * params.radius;

        // Project sample to screen space
        vec4 offset = params.projection * vec4(samplePos, 1.0);
        offset.xy = offset.xy / offset.w * 0.5 + 0.5;

        float sampleDepth = texture(gPosition, offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, params.radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + params.bias ? 1.0 : 0.0) * rangeCheck;
    }

    outOcclusion = 1.0 - (occlusion / float(params.kernelSize));
}
