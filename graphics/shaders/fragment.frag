#version 430 core

const float PI = 3.141592653589793238462643383279502884197169399;
const float EULER = 2.71828;

const vec4 dark_blue = vec4(0.043137254901960784, 0.2627450980392157, 0.4980392156862745, 1.0);
const vec4 light_blue = vec4(0.3058823529411765, 0.803921568627451, 0.7686274509803922, 1.0);
const vec4 transparent = vec4(0.0, 0.0, 0.0, 0.0);

in vec2 tex_coord;

uniform float aspect_ratio;
uniform float delta_time_4_s;
uniform float delta_time_1_s;
uniform float period_s;
uniform float pattern_scale;
uniform float movement_scale;
uniform float time_scale;
uniform vec4 color_bg;

struct LineParams {
    float color_inner_0;
    float color_inner_1;
    float color_inner_2;
    float radius_base;
    float radius_scale;
    float data_end_idx;
    uint buffer_length;
    uint num_aux_lines;
};

layout (std430, binding=1) buffer info
{
    LineParams params[];
};

layout (std430, binding=2) buffer data
{
    float data_samples[];
};

layout (std430, binding=3) buffer data_aux
{
    float data_samples_aux[];
};

out vec4 frag_color;

float gauss_peak (float x, float mu, float sigma) {
    return pow(EULER, -1./2. * (x - mu) / sigma);
}

vec4 color_pattern_fill (vec2 coord_polar, vec4 base_color, vec4 accet_color)
{
    float time_clog2 = pow(2, int(log2(time_scale)));
    float unit_phase = delta_time_4_s / (period_s * time_clog2);
    float radius = coord_polar[0];
    float angle = coord_polar[1] + unit_phase * 2 * PI;

    float movement_clog2 = pow(2, int(log2(movement_scale)));
    float scale = pattern_scale * (1 + cos(delta_time_4_s / (2.0 * period_s) * PI) / movement_clog2);
    float angle_step_size = 2 * PI / (scale - 1);
    float quant_upper = angle / angle_step_size;
    int quant_lower = int(quant_upper);

    float alpha = clamp(0, 1, gauss_peak(quant_upper - quant_lower, 0, 0.05));
    return mix(base_color, accet_color, alpha);
}

void main()
{
    vec2 coord;
    coord[0] = tex_coord[0] * aspect_ratio;
    coord[1] = tex_coord[1];

    float radius = length(coord)/sqrt(2.0);
    float angle = atan(coord[1], coord[0]) + PI;

    frag_color = color_bg;

    // Outer lines begin
    int offset = 0;
    for (int i = int(params[0].num_aux_lines); i >= 0; i--) {
        int num_samples = int(params[0].buffer_length);

        float angle_step_size = 2 * PI / (float(num_samples) - 1);
        float index_float = angle / angle_step_size;

        int data_index_lower = int(index_float);
        int data_index_upper = (data_index_lower + 1) % num_samples;

        float data_lower = 0.0;
        float data_upper = 0.0;
        if (i == params[0].num_aux_lines) {
            data_lower = data_samples[offset + data_index_lower];
            data_upper = data_samples[offset + data_index_upper];
        } else {
            data_lower = data_samples_aux[offset + data_index_lower];
            data_upper = data_samples_aux[offset + data_index_upper];
        }

        float alpha = index_float - data_index_lower;
        float result = mix(data_upper, data_lower, alpha);
        float target_radius = params[0].radius_base + params[0].radius_scale * result;
        if (i != params[0].num_aux_lines) { // Last index is outline of main wobble
            float beat_fraction = delta_time_1_s / period_s;
            target_radius = target_radius + (beat_fraction + i) * float(period_s) / (float(period_s) * 4);
        }

        float err = gauss_peak(abs(radius - target_radius), 0, i == params[0].num_aux_lines ? 0.001 : 0.0005);
        frag_color = frag_color + mix(transparent, vec4(1.0, 1.0, 1.0, 1.0), err);

        if (i != params[0].num_aux_lines) {
            offset = offset + int(params[0].buffer_length);
        }
    }
    // Outer lines end


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

        if (err < 0.0) {
            vec4 color = {params[i].color_inner_0, params[i].color_inner_1, params[i].color_inner_2, 1.0};
            vec4 accet_color = i == 0 ? light_blue * 0.8 : color * 0.3;
            vec2 shifted = coord + vec2(100, 100);
            vec2 polar = i == 0 ? vec2(radius, angle) : vec2(length(shifted)/sqrt(2.0), atan(shifted[1], shifted[0]) + PI);
            frag_color = color_pattern_fill(polar, color, accet_color);
        } else if (i != 0) {
            float mix_factor = clamp(log(1 + (err * 50)), 0, 1.0);
            frag_color = mix(frag_color * 0.1, frag_color, mix_factor);
        }
    }
}