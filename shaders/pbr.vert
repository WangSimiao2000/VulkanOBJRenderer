#version 450

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    mat4 lightSpaceMatrix;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragShadowCoord;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(pc.model))) * inNormal;
    fragUV = inUV;
    fragShadowCoord = camera.lightSpaceMatrix * worldPos;
    gl_Position = camera.proj * camera.view * worldPos;
}
