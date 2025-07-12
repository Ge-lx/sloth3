#version 410 core

const float PI = 3.141592653589793238462643383279502884197169399;
const float EULER = 2.71828;

const vec4 c1 = vec4(0.21568627450980393, 0.19607843137254902, 0.023529411764705882, 1);
const vec4 c2 = vec4(0.12549019607843137, 0.19607843137254902, 0.17254901960784313, 1);
const vec4 c1b = vec4(0.5058823529411764, 0.4588235294117647, 0.054901960784313725, 1);
const vec4 c2b = vec4(0.34509803921568627, 0.5372549019607843, 0.47058823529411764, 1);
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
uniform float num_hor_pix;
uniform vec4 color_bg;
uniform int num_lines;

struct LineParams {
    float color_inner_0;
    float color_inner_1;
    float color_inner_2;
    float base;
    float scale;
    float data_end_idx;
    uint buffer_length;
    uint num_aux_lines;
};

uniform samplerBuffer params;
uniform samplerBuffer data_samples;
uniform samplerBuffer data_samples_aux;

out vec4 frag_color;

LineParams read_params (int idx)
{
    LineParams parsed;
    parsed.color_inner_0 = texelFetch(params, idx * 8 + 0).r;
    parsed.color_inner_1 = texelFetch(params, idx * 8 + 1).r;
    parsed.color_inner_2 = texelFetch(params, idx * 8 + 2).r;
    parsed.base = texelFetch(params, idx * 8 + 3).r;
    parsed.scale = texelFetch(params, idx * 8 + 4).r;
    parsed.data_end_idx = texelFetch(params, idx * 8 + 5).r;
    parsed.buffer_length = floatBitsToUint(texelFetch(params, idx * 8 + 6).r);
    parsed.num_aux_lines = floatBitsToUint(texelFetch(params, idx * 8 + 7).r);
    return parsed;
}

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

// vec4 test (vec2 coord)
// {
//     return texelFetch(data_samples, int(coord[0] * 1920));
// }

void main()
{
    vec2 coord;
    coord[0] = tex_coord[0] * aspect_ratio;
    coord[1] = tex_coord[1];

    // if () {
    // frag_color = test(coord);
    // return;
    // }

    // float radius = length(coord)/sqrt(2.0);
    // float angle = atan(coord[1], coord[0]) + PI;

    // frag_color = color_bg;
    frag_color = transparent; // mix(transparent, vec4(0.0, 0.0, 0.0, 0.0), 0);

    // Outer lines begin
    // LineParams params_0 = read_params(0);
    // int offset = 0;
    // for (int i = int(params_0.num_aux_lines); i >= 0; i--) {
    //     int num_samples = int(params_0.buffer_length);

    //     float angle_step_size = 2 * PI / (float(num_samples) - 1);
    //     float index_float = angle / angle_step_size;

    //     int data_index_lower = int(index_float);
    //     int data_index_upper = (data_index_lower + 1) % num_samples;

    //     float data_lower = 0.0;
    //     float data_upper = 0.0;
    //     if (i == params_0.num_aux_lines) {
    //         data_lower = texelFetch(data_samples, offset + data_index_lower).r;
    //         data_upper = texelFetch(data_samples, offset + data_index_upper).r;
    //     } else {
    //         data_lower = texelFetch(data_samples_aux, offset + data_index_lower).r;
    //         data_upper = texelFetch(data_samples_aux, offset + data_index_upper).r;
    //     }

    //     float alpha = index_float - data_index_lower;
    //     float result = mix(data_lower, data_upper, alpha);
    //     float target_radius = params_0.base + params_0.scale * result;
    //     if (i != params_0.num_aux_lines) { // Last index is outline of main wobble
    //         float beat_fraction = delta_time_1_s / period_s;
    //         target_radius = target_radius + (beat_fraction + i) * float(period_s) / (float(period_s) * 4);
    //     }

    //     float err = gauss_peak(abs(radius - target_radius), 0, i == params_0.num_aux_lines ? 0.001 : 0.0005);
    //     frag_color = frag_color + mix(transparent, vec4(1.0, 1.0, 1.0, 1.0), err);

    //     if (i != params_0.num_aux_lines) {
    //         offset = offset + int(params_0.buffer_length);
    //     }
    // }
    // Outer lines end


    float x = coord[0] + aspect_ratio; // move x to left border

    for (int i = 0; i < num_lines; i++) {
        LineParams params_i = read_params(i);
        int idx_offset = i == 0 ? 0 : int(read_params(i-1).data_end_idx);
        int num_samples = int(params_i.data_end_idx) - idx_offset;

        float x_step_size = (float(num_samples) - 1) / (2 * aspect_ratio);
        // float x_step_size = 2 * aspect_ratio / (float(num_samples) - 1);
        float index_float = x * x_step_size;

        float oversamp = float(num_samples) / float(num_hor_pix);

        int data_index_floor = int(index_float);
        int data_index_ceil = (data_index_floor + int(oversamp + 0.99)) % num_samples;

        float data_floor = texelFetch(data_samples, idx_offset + data_index_floor).r;
        float data_ceil = texelFetch(data_samples, idx_offset + data_index_ceil).r;

        float alpha = index_float - data_index_floor;
        float diff = abs(data_ceil - data_floor);
        float data_interp = mix(data_floor, data_ceil, alpha);

        float target_y = params_i.base + params_i.scale * data_interp;

        float target_y_floor = params_i.base + params_i.scale * data_floor;
        float target_y_ceil = params_i.base + params_i.scale * data_ceil;

        // float err = coord[1] - params_i.base;
        // if (err < 0){
        //     frag_color = vec4(1, 1, 1, 1);
        // } else {
        //     frag_color = vec4(1, 0, 0, 1);
        // }

        float deviation = coord[1] - target_y;

        bool not_over = !(coord[1] > (max(target_y_floor, target_y_ceil) + 0.001));
        bool not_under = !(coord[1] < (min(target_y_floor, target_y_ceil) - 0.001));
        bool close_enough = abs(deviation) < (0.001 + oversamp * 0.5 * diff);

        float aa = abs(deviation);
        float aa_clamp = clamp(aa * 1, 0, 1);
        vec4 pure_color = frag_color;

        if (i == 0) {
            float m = clamp(log(80 * aa_clamp) * 0.5, 0, 1);
            float m2 = clamp(abs(deviation) / 2, 0, 1);
            if (target_y > coord[1]) {
                pure_color = mix(c1b, c1, m2);
            } else {
                pure_color = mix(c2b, c2, m2);
            }

            frag_color = pure_color;//mix(transparent, pure_color, m);
        }

        if (close_enough) {
            if (not_over && not_under){
                frag_color = vec4(1, 1, 1, 1);
            } else {
                frag_color = mix(frag_color, vec4(1.0, 1.0, 1.0, 1.0), aa_clamp);
            }
        }

        // float b = abs((target_y - coord[1]));
        // float m = 1 - exp(b* 10/3);
        // frag_color = mix(frag_color, vec4(1, 1, 1, 1), m);

        // if (not_over || not_under) {
        //     frag_color = light_blue;
        // }

        // if (err < 0.0) {
        //     vec4 color = vec4(params_i.color_inner_0, params_i.color_inner_1, params_i.color_inner_2, 1.0);
        //     vec4 accet_color = i == 0 ? light_blue * 0.8 : color * 0.3;
        //     vec2 shifted = coord + vec2(100, 100);
        //     vec2 polar = i == 0 ? vec2(radius, angle) : vec2(length(shifted)/sqrt(2.0), atan(shifted[1], shifted[0]) + PI);
        //     frag_color = color_pattern_fill(polar, color, accet_color);
        // } else if (i != 0) {
        //     float mix_factor = clamp(log(1 + (err * 50)), 0, 1.0);
        //     frag_color = mix(frag_color * 0.1, frag_color, mix_factor);
        // }
    }
}