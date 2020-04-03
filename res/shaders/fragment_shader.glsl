#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D texsampler;

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_texcoords;

layout(location = 0) out vec4 out_color;

void main() {
    const vec4 diffuse = texture(texsampler, in_texcoords);

    out_color = vec4(in_color * diffuse.rgb, 1.0);
}
