#version 450

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceModel; // lightSpaceMatrix * model
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pc.lightSpaceModel * vec4(inPosition, 1.0);
}
