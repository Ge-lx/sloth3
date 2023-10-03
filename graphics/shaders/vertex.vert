#version 430 core

layout (location = 0) in vec3 position;


out vec2 tex_coord;



void main() 
{
    gl_Position = vec4(position, 1.0);
    tex_coord = position.xy;
}