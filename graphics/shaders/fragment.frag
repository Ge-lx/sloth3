#version 410 core

const float PI = 3.14159265359;

in vec2 tex_coord;

uniform float line_data[12];
uniform int line_data_length;
uniform float aspect_ratio;
uniform float timer;


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

    if (radius <= 0.1119 && radius >= 0.11) {
        frag_color = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
        frag_color = vec4(1.0, 1.0, 1.0, 1.0);
    }
}