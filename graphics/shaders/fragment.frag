#version 430 core

const float PI = 3.14159265359;

const vec4 colors[] = {
    vec4(0.3058823529411765, 0.803921568627451, 0.7686274509803922, 1.0), // blueish
    vec4(0.9803921568627451, 0.6509803921568628, 0.07450980392156863, 1.0), // orange
    vec4(0.40784313725490196, 0.5568627450980392, 0.14901960784313725, 1.0), // green
    vec4(0.2823529411764706, 0.2627450980392157, 0.28627450980392155, 1.0) // greyish
};

const float radius_base_CHANGEME[] = {0.6, 0.3};

in vec2 tex_coord;

uniform float aspect_ratio;
uniform float timer;
uniform vec4 color_bg;

struct LineParams {
    float color_inner_0;
    float color_inner_1;
    float color_inner_2;
    float color_inner_3;
    float radius_base;
    float radius_scale;
    float data_end_idx;
    float whatever;
};


layout (std430, binding=1) buffer info
{
    LineParams params[];
};

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

    vec2 ref = vec2(1.0, 0.0);
    float radius = length(coord)/sqrt(2.0);
    float angle = atan(coord[1], coord[0]) + PI;

    frag_color = color_bg;

    for (int i = 0; i < params.length(); i++) {

        int idx_offset = i == 0 ? 0 : int(params[i-1].data_end_idx);
        int num_samples = int(params[i].data_end_idx) - idx_offset;

        float angle_step_size = 2 * PI / (float(num_samples) - 1);
        float index_float = angle / angle_step_size;

        int data_index_lower = int(index_float);
        int data_index_upper = (data_index_lower + 1) % num_samples;

        float data_lower = data_samples[idx_offset + data_index_lower];
        float data_upper = data_samples[idx_offset + data_index_upper];
        float alpha = index_float - data_index_lower;

        float result = mix(data_upper, data_lower, alpha);
        float target_radius = params[i].radius_base + params[i].radius_scale * result;
        float err = radius - target_radius;

        if (err <= 0.0) {
            vec4 color = {params[i].color_inner_0, params[i].color_inner_1, params[i].color_inner_2, params[i].color_inner_3};
            frag_color = color;
        } else {
            float mix_factor = clamp(log(1 + (err * 50)), 0, 1.0);
            frag_color = mix(frag_color * 0.1, frag_color, mix_factor);
        }
    }
}