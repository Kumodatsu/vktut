#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBO {
    mat4 Model;
    mat4 View;
    mat4 Projection;
} ubo;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 out_color;

void main() {
    gl_Position
        = ubo.Projection
        * ubo.View
        * ubo.Model
        * vec4(in_position, 0.0, 1.0);
    out_color = in_color;
}
