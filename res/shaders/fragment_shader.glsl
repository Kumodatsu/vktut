#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D texsampler;

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_texcoords;
layout(location = 2) in vec3 in_position;
layout(location = 3) in vec3 in_light_position;
layout(location = 4) in vec3 in_light_color;

layout(location = 0) out vec4 out_color;

void main() {
    const vec4  diffuse         = texture(texsampler, in_texcoords);
    const vec3  light_vector    = in_light_position - in_position;
    const float light_distance  = length(light_vector);
    const vec3  light_direction = light_vector / light_distance;
    const vec3  normal          = vec3(0.0, 1.0, 0.0);
    const float n_dot_l         = dot(normal, light_direction);
    const float attenuation     = 1.0f / (light_distance * light_distance);
    const vec3  luminance       = max(n_dot_l, 0.0) * in_light_color;

    const vec3  color
        = attenuation
        * luminance
        * in_color
        * diffuse.rgb;

    out_color = vec4(color, 1.0);
}
