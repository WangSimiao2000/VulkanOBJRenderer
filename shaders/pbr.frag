#version 450

const float PI = 3.14159265359;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
} camera;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
} material;

layout(set = 2, binding = 0) uniform LightUBO {
    vec4 position;
    vec4 color;
    float intensity;
} light;

layout(set = 3, binding = 0) uniform sampler2D shadowMap;
layout(set = 3, binding = 1) uniform sampler2D ssaoMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;

// GGX/Trowbridge-Reitz Normal Distribution Function
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith's Schlick-GGX Geometry Function
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// Fresnel-Schlick Approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// PCF Shadow Sampling
float shadowCalculation(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(camera.camPos.xyz - fragWorldPos);
    vec3 L = normalize(light.position.xyz - fragWorldPos);
    vec3 H = normalize(V + L);

    float distance = length(light.position.xyz - fragWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = light.color.rgb * light.intensity * attenuation;

    vec3 F0 = mix(vec3(0.04), material.albedo, material.metallic);

    float NDF = distributionGGX(N, H, material.roughness);
    float G = geometrySmith(N, V, L, material.roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - material.metallic);
    float NdotL = max(dot(N, L), 0.0);

    float shadow = shadowCalculation(fragShadowCoord);
    float ssao = texture(ssaoMap, fragUV).r;

    vec3 ambient = vec3(0.03) * material.albedo * material.ao * ssao;
    vec3 Lo = (kD * material.albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);

    vec3 color = ambient + Lo;
    // Tone mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
