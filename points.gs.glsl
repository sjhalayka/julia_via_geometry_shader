#version 430 core

layout (points) in;
layout (points) out;
layout (max_vertices = 502) out;

uniform vec4 C;
uniform int max_iterations;
uniform float threshold;

out vec4 vert;

in VS_OUT
{
    vec4 position;
} gs_in[];

vec4 inverse_vec4(vec4 in_vec)
{
	// inv(a) = conjugate(a) / norm(a)

	float temp_a_norm = in_vec.x*in_vec.x + in_vec.y*in_vec.y + in_vec.z*in_vec.z + in_vec.w*in_vec.w;

    vec4 out_vec;

	out_vec.x =  in_vec.x;
	out_vec.y = -in_vec.y;
	out_vec.z = -in_vec.z;
	out_vec.w = -in_vec.w;


	out_vec.x = out_vec.x / temp_a_norm;
    out_vec.y = out_vec.y / temp_a_norm;
	out_vec.z = out_vec.z / temp_a_norm;
	out_vec.w = out_vec.w / temp_a_norm;

    return out_vec;
}

vec4 pow_vec4(vec4 in_vec, float beta)
{
	float fabs_beta = abs(beta);

	float self_dot = in_vec.x * in_vec.x + in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w;

	if (self_dot == 0)
	{
        return vec4(0, 0, 0, 0);
	}

	float len = sqrt(self_dot);
	float self_dot_beta = pow(self_dot, fabs_beta / 2.0f);

	vec4 out_vec;

	out_vec.x = self_dot_beta * cos(fabs_beta * acos(in_vec.x / len));
	out_vec.y = in_vec.y * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);
	out_vec.z = in_vec.z * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);
	out_vec.w = in_vec.w * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);

	if (beta < 0)
		out_vec = inverse_vec4(out_vec);

	return out_vec;
}


void main(void)
{
    vec4 Z = gs_in[0].position;
		
    vert = Z;
    EmitVertex();
    EndPrimitive();

    for (int i = 0; i < max_iterations; i++)
    {
        Z = pow_vec4(Z, 2.0) + C;
        
        vert = Z;
        EmitVertex();
        EndPrimitive();
        
        if (length(Z) >= threshold)
            break;
    }

    vec4 sentinel = vec4(10000,10000,10000,10000);

    vert = sentinel;
    EmitVertex();
    EndPrimitive();
}