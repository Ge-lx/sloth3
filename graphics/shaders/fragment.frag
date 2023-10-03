#version 410 core

const float PI = 3.14159265359;

in vec2 tex_coord;

uniform int line_lengths[10];
uniform float aspect_ratio;
uniform float timer;

uniform float line_data_1[2000];
// uniform float line_data_1[200];
// uniform float line_data_2[200];
// uniform float line_data_3[200];
// uniform float line_data_4[200];
// uniform float line_data_5[200];
// uniform float line_data_6[200];
// uniform float line_data_7[200];
// uniform float line_data_8[200];
// uniform float line_data_9[200];


out vec4 frag_color;

vec3 RainbowColorSmooth(float t) {
    t = mod(t, 1.0); // Wrap t within [0, 1] to create a repeating rainbow

    // Convert the hue from [0, 1] to [0, 360] degrees
    float hue = t * 360.0;

    // Calculate RGB values from HSV (Hue, Saturation, Value)
    float c = 1.0; // Saturation and Value are constant
    float x = c * (1.0 - abs(mod(hue / 60.0, 2.0) - 1.0));
    float m = 0.0;
    
    vec3 color;
    
    if (hue >= 0.0 && hue < 60.0) {
        color = vec3(c, x, m);
    } else if (hue >= 60.0 && hue < 120.0) {
        color = vec3(x, c, m);
    } else if (hue >= 120.0 && hue < 180.0) {
        color = vec3(m, c, x);
    } else if (hue >= 180.0 && hue < 240.0) {
        color = vec3(m, x, c);
    } else if (hue >= 240.0 && hue < 300.0) {
        color = vec3(x, m, c);
    } else {
        color = vec3(c, m, x);
    }

    return color;
}

void main()
{
    vec2 coord;
    coord[0] = tex_coord[0] * aspect_ratio;
    coord[1] = tex_coord[1];

    vec2 ref = vec2(1.0, 0.0);


    float radius = length(coord)/sqrt(2.0);
    float angle = acos(dot(ref, coord/length(coord)));


    float eps = 0.0001;
    float angle_step_size = 2.0 * PI / line_lengths[0];
    int data_index = int(angle * angle_step_size);
    float target_radius = 0.12 + (line_data_1[data_index] / 2.0);

    if (abs(radius - target_radius) <= eps)  {
        frag_color = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        frag_color = vec4(0.0, 0.0, 0.0, 0.0);
    }
}