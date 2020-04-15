#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBO {
    mat4 Model;
    mat4 View;
    mat4 Projection;
    vec3 LightPosition;
    vec3 LightColor;
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_texcoords;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_texcoords;
layout(location = 2) out vec3 out_position;
layout(location = 3) out vec3 out_light_position;
layout(location = 4) out vec3 out_light_color;

void main() {
    const vec4 world_position
        = ubo.Model
        * vec4(in_position, 1.0);
    gl_Position
        = ubo.Projection
        * ubo.View
        * world_position;
    out_color          = in_color;
    out_texcoords      = in_texcoords;
    out_position       = world_position.xyz;
    out_light_position = ubo.LightPosition;
    out_light_color    = ubo.LightColor;
}
