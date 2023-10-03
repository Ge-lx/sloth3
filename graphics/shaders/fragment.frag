#version 430 core

const float PI = 3.14159265359;

in vec2 tex_coord;

uniform float aspect_ratio;
uniform float timer;

uniform int num_samples;
uniform vec2 center;
uniform float radius_base;
uniform float radius_scale;

layout (std430, binding=2) buffer data
{
    float data_samples[];
};

out vec4 frag_color;

void main()
{
    vec2 coord;
    coord[0] = tex_coord[0] * aspect_ratio;
    coord[1] = tex_coord[1];
    coord = coord - center;

    vec2 ref = vec2(1.0, 0.0);
    float radius = length(coord)/sqrt(2.0);
    float angle = atan(coord[1], coord[0]) + PI;


    float eps = 0.0005;
    float angle_step_size = 2 * PI / (float(num_samples) - 1);
    int data_index = int(angle / angle_step_size);

    float target_radius = radius_base + radius_scale * data_samples[data_index];

    if (abs(radius - target_radius) <= eps)  {
        frag_color = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        frag_color = vec4(0.0, 0.0, 0.0, 0.0);
    }
}