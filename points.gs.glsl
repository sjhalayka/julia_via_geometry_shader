#version 430 core

layout (points) in;
layout (points) out;
layout (max_vertices = 6) out;

out vec4 vert;

in VS_OUT
{
    vec4 position;
} gs_in[];

void main(void)
{
    vec4 vertex0 = vec4(1, 2, 3, 7); 
    vec4 vertex1 = vec4(4, 5, 6, 8);    
    vec4 vertex2 = vec4(7, 8, 9, 9);

    vert = vertex0;
    EmitVertex();
    EndPrimitive();
    
    vert = vertex1;
    EmitVertex();
    EndPrimitive();
    
    vert = vertex2;
    EmitVertex();
    EndPrimitive();
}