#include <GL/glew.h>
#include <GL/glut.h>

#include "marching_cubes.h"
using namespace marching_cubes;

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

// Automatically link in the GLUT and GLEW libraries if compiling on MSVC++
#ifdef _MSC_VER
#pragma comment(lib, "glew32")
#pragma comment(lib, "freeglut")
#endif

#include <iostream>
#include <vector>
using namespace std;




#include "vertex_geometry_shader.h"






bool write_triangles_to_binary_stereo_lithography_file(const vector<triangle>& triangles, const char* const file_name)
{
	cout << "Triangle count: " << triangles.size() << endl;

	if (0 == triangles.size())
		return false;

	// Write to file.
	ofstream out(file_name, ios_base::binary);

	if (out.fail())
		return false;

	const size_t header_size = 80;
	vector<char> buffer(header_size, 0);
	const unsigned int num_triangles = static_cast<unsigned int>(triangles.size()); // Must be 4-byte unsigned int.
	vertex_3 normal;

	// Write blank header.
	out.write(reinterpret_cast<const char*>(&(buffer[0])), header_size);

	// Write number of triangles.
	out.write(reinterpret_cast<const char*>(&num_triangles), sizeof(unsigned int));

	// Copy everything to a single buffer.
	// We do this here because calling ofstream::write() only once PER MESH is going to 
	// send the data to disk faster than if we were to instead call ofstream::write()
	// thirteen times PER TRIANGLE.
	// Of course, the trade-off is that we are using 2x the RAM than what's absolutely required,
	// but the trade-off is often very much worth it (especially so for meshes with millions of triangles).
	cout << "Generating normal/vertex/attribute buffer" << endl;

	// Enough bytes for twelve 4-byte floats plus one 2-byte integer, per triangle.
	const size_t data_size = (12 * sizeof(float) + sizeof(short unsigned int)) * num_triangles;
	buffer.resize(data_size, 0);

	// Use a pointer to assist with the copying.
	// Should probably use std::copy() instead, but memcpy() does the trick, so whatever...
	char* cp = &buffer[0];

	for (vector<triangle>::const_iterator i = triangles.begin(); i != triangles.end(); i++)
	{
		// Get face normal.
		vertex_3 v0 = i->vertex[1] - i->vertex[0];
		vertex_3 v1 = i->vertex[2] - i->vertex[0];
		normal = v0.cross(v1);
		normal.normalize();

		memcpy(cp, &normal.x, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &normal.y, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &normal.z, sizeof(float)); cp += sizeof(float);

		memcpy(cp, &i->vertex[0].x, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[0].y, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[0].z, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[1].x, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[1].y, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[1].z, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[2].x, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[2].y, sizeof(float)); cp += sizeof(float);
		memcpy(cp, &i->vertex[2].z, sizeof(float)); cp += sizeof(float);

		cp += sizeof(short unsigned int);
	}

	cout << "Writing " << data_size / 1048576.0f << " MB of data to binary Stereo Lithography file: " << file_name << endl;

	out.write(reinterpret_cast<const char*>(&buffer[0]), data_size);
	out.close();

	return true;
}



void get_trajectories(
	const vector<float>& point_vertex_data,
	vector<vector<quaternion>>& trajectories,
	vertex_geometry_shader& g0_mc_shader,
	quaternion C,
	int max_iterations,
	float threshold)
{
	const GLuint components_per_position = 4;
	const GLuint components_per_vertex = components_per_position;

	GLuint point_buffer;

	glGenBuffers(1, &point_buffer);

	const GLuint num_vertices = static_cast<GLuint>(point_vertex_data.size()) / components_per_vertex;

	glBindBuffer(GL_ARRAY_BUFFER, point_buffer);
	glBufferData(GL_ARRAY_BUFFER, point_vertex_data.size() * sizeof(GLfloat), &point_vertex_data[0], GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(glGetAttribLocation(g0_mc_shader.get_program(), "position"));
	glVertexAttribPointer(glGetAttribLocation(g0_mc_shader.get_program(), "position"),
		components_per_position,
		GL_FLOAT,
		GL_FALSE,
		components_per_vertex * sizeof(GLfloat),
		0);

	glUseProgram(g0_mc_shader.get_program());

	glUniform4f(glGetUniformLocation(g0_mc_shader.get_program(), "C"), C.x, C.y, C.z, C.w);
	glUniform1i(glGetUniformLocation(g0_mc_shader.get_program(), "max_iterations"), max_iterations);
	glUniform1f(glGetUniformLocation(g0_mc_shader.get_program(), "threshold"), threshold);

	size_t max_output_vertices_per_input = max_iterations + 2;

	size_t max_vertices = max_output_vertices_per_input * num_vertices;
	size_t num_floats_per_vertex = 4;

	// Allocate enough for the maximum number of vertices
	GLuint tbo;
	glGenBuffers(1, &tbo);
	glBindBuffer(GL_ARRAY_BUFFER, tbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * max_vertices * num_floats_per_vertex, nullptr, GL_STATIC_READ);

	GLuint query;
	glGenQueries(1, &query);

	// Perform feedback transform
	glEnable(GL_RASTERIZER_DISCARD);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbo);

	glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, num_vertices);
	glEndTransformFeedback();
	glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

	glDisable(GL_RASTERIZER_DISCARD);

	glFlush();

	GLuint primitives;
	glGetQueryObjectuiv(query, GL_QUERY_RESULT, &primitives);

	// Read back actual number of triangles (in case it's less than two triangles)
	vector<GLfloat> feedback(primitives * num_floats_per_vertex);
	glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(GLfloat) * feedback.size(), &feedback[0]);

	glDeleteQueries(1, &query);
	glDeleteBuffers(1, &tbo);

	vector<quaternion> trajectory;

	for (size_t i = 0; i < primitives; i++)
	{
		size_t feedback_index = 4 * i;

		if (feedback[feedback_index + 0] == 10000 &&
			feedback[feedback_index + 1] == 10000 &&
			feedback[feedback_index + 2] == 10000 &&
			feedback[feedback_index + 3] == 10000)
		{
			trajectories.push_back(trajectory);
			trajectory.clear();
		}
		else
		{
			quaternion Q(
				feedback[feedback_index + 0],
				feedback[feedback_index + 1],
				feedback[feedback_index + 2],
				feedback[feedback_index + 3]);

			trajectory.push_back(Q);
		}
	}
}

void emit_shaders_to_files(const char* const vs_filename, const char* const gs_filename, int max_iterations)
{
	ofstream vs_out(vs_filename);

	vs_out << "#version 410 core" << endl;

	vs_out << "// Per-vertex inputs" << endl;
	vs_out << "layout(location = 0) in vec4 position;" << endl;

	vs_out << "out VS_OUT" << endl;
	vs_out << "{" << endl;
	vs_out << "	vec4 position; " << endl;
	vs_out << "} vs_out;" << endl;

	vs_out << "void main(void)" << endl;
	vs_out << "{" << endl;
	vs_out << "	vs_out.position = position; " << endl;
	vs_out << "}" << endl;

	vs_out.close();

	ofstream gs_out(gs_filename);

	gs_out << "#version 430 core" << endl;
	gs_out << "" << endl;
	gs_out << "layout (points) in;" << endl;
	gs_out << "layout (points) out;" << endl;
	gs_out << "layout (max_vertices = " << max_iterations + 2 << ") out;" << endl;
	gs_out << "" << endl;
	gs_out << "uniform vec4 C;" << endl;
	gs_out << "uniform int max_iterations;" << endl;
	gs_out << "uniform float threshold;" << endl;
	gs_out << "" << endl;
	gs_out << "out vec4 vert;" << endl;
	gs_out << "" << endl;
	gs_out << "in VS_OUT" << endl;
	gs_out << "{" << endl;
	gs_out << "    vec4 position;" << endl;
	gs_out << "} gs_in[];" << endl;
	gs_out << "" << endl;
	gs_out << "vec4 inverse_vec4(vec4 in_vec)" << endl;
	gs_out << "{" << endl;
	gs_out << "	// inv(a) = conjugate(a) / norm(a)" << endl;
	gs_out << "" << endl;
	gs_out << "	float temp_a_norm = in_vec.x*in_vec.x + in_vec.y*in_vec.y + in_vec.z*in_vec.z + in_vec.w*in_vec.w;" << endl;
	gs_out << "" << endl;
	gs_out << "    vec4 out_vec;" << endl;
	gs_out << "" << endl;
	gs_out << "	out_vec.x =  in_vec.x;" << endl;
	gs_out << "	out_vec.y = -in_vec.y;" << endl;
	gs_out << "	out_vec.z = -in_vec.z;" << endl;
	gs_out << "	out_vec.w = -in_vec.w;" << endl;
	gs_out << "" << endl;
	gs_out << "" << endl;
	gs_out << "	out_vec.x = out_vec.x / temp_a_norm;" << endl;
	gs_out << "    out_vec.y = out_vec.y / temp_a_norm;" << endl;
	gs_out << "	out_vec.z = out_vec.z / temp_a_norm;" << endl;
	gs_out << "	out_vec.w = out_vec.w / temp_a_norm;" << endl;
	gs_out << "" << endl;
	gs_out << "    return out_vec;" << endl;
	gs_out << "}" << endl;
	gs_out << "" << endl;
	gs_out << "vec4 pow_vec4(vec4 in_vec, float beta)" << endl;
	gs_out << "{" << endl;
	gs_out << "	float fabs_beta = abs(beta);" << endl;
	gs_out << "" << endl;
	gs_out << "	float self_dot = in_vec.x * in_vec.x + in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w;" << endl;
	gs_out << "" << endl;
	gs_out << "	if (self_dot == 0)" << endl;
	gs_out << "	{" << endl;
	gs_out << "        return vec4(0, 0, 0, 0);" << endl;
	gs_out << "	}" << endl;
	gs_out << "" << endl;
	gs_out << "	float len = sqrt(self_dot);" << endl;
	gs_out << "	float self_dot_beta = pow(self_dot, fabs_beta / 2.0f);" << endl;
	gs_out << "" << endl;
	gs_out << "	vec4 out_vec;" << endl;
	gs_out << "" << endl;
	gs_out << "	out_vec.x = self_dot_beta * cos(fabs_beta * acos(in_vec.x / len));" << endl;
	gs_out << "	out_vec.y = in_vec.y * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);" << endl;
	gs_out << "	out_vec.z = in_vec.z * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);" << endl;
	gs_out << "	out_vec.w = in_vec.w * self_dot_beta * sin(fabs_beta * acos(in_vec.x / len)) / sqrt(in_vec.y * in_vec.y + in_vec.z * in_vec.z + in_vec.w * in_vec.w);" << endl;
	gs_out << "" << endl;
	gs_out << "	if (beta < 0)" << endl;
	gs_out << "		out_vec = inverse_vec4(out_vec);" << endl;
	gs_out << "" << endl;
	gs_out << "	return out_vec;" << endl;
	gs_out << "}" << endl;
	gs_out << "" << endl;
	gs_out << "" << endl;
	gs_out << "void main(void)" << endl;
	gs_out << "{" << endl;
	gs_out << "    vec4 Z = gs_in[0].position;" << endl;
	gs_out << "		" << endl;
	gs_out << "    vert = Z;" << endl;
	gs_out << "    EmitVertex();" << endl;
	gs_out << "    EndPrimitive();" << endl;
	gs_out << "" << endl;
	gs_out << "    for (int i = 0; i < max_iterations; i++)" << endl;
	gs_out << "    {" << endl;
	gs_out << "        Z = pow_vec4(Z, 2.0) + C;" << endl;
	gs_out << "        " << endl;
	gs_out << "        vert = Z;" << endl;
	gs_out << "        EmitVertex();" << endl;
	gs_out << "        EndPrimitive();" << endl;
	gs_out << "        " << endl;
	gs_out << "        if (length(Z) >= threshold)" << endl;
	gs_out << "            break;" << endl;
	gs_out << "    }" << endl;
	gs_out << "" << endl;
	gs_out << "    vec4 sentinel = vec4(10000,10000,10000,10000);" << endl;
	gs_out << "" << endl;
	gs_out << "    vert = sentinel;" << endl;
	gs_out << "    EmitVertex();" << endl;
	gs_out << "    EndPrimitive();" << endl;
	gs_out << "}" << endl;

}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(10, 10);
	glutInitWindowPosition(0, 0);

	GLint win_id = glutCreateWindow("GS Test");

	if (GLEW_OK != glewInit())
	{
		cout << "GLEW initialization error" << endl;
		return 0;
	}

	int GL_major_version = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &GL_major_version);

	int GL_minor_version = 0;
	glGetIntegerv(GL_MINOR_VERSION, &GL_minor_version);

	if (GL_major_version < 4)
	{
		cout << "GPU does not support OpenGL 4.3 or higher" << endl;
		return 0;
	}
	else if (GL_major_version == 4)
	{
		if (GL_minor_version < 3)
		{
			cout << "GPU does not support OpenGL 4.3 or higher" << endl;
			return 0;
		}
	}


	float x_grid_max = 1.5;
	float y_grid_max = 1.5;
	float z_grid_max = 1.5;
	float x_grid_min = -x_grid_max;
	float y_grid_min = -y_grid_max;
	float z_grid_min = -z_grid_max;
	size_t x_res = 100;
	size_t y_res = 100;
	size_t z_res = 100;

	float z_w = 0;
	quaternion C;
	C.x = 0.3f;
	C.y = 0.5f;
	C.z = 0.4f;
	C.w = 0.2f;
	int max_iterations = 8;
	float threshold = 4.0f;


	emit_shaders_to_files("points.vs.glsl", "points.gs.glsl", max_iterations);

	vertex_geometry_shader g0_mc_shader;

	if (false == g0_mc_shader.init("points.vs.glsl", "points.gs.glsl", "vert"))
	{
		cout << "Couldn't load shaders" << endl;
		return 0;
	}

	g0_mc_shader.use_program();



	// Make enough data for 1 point
	vector<float> point_vertex_data;

	const float x_step_size = (x_grid_max - x_grid_min) / (x_res - 1);
	const float y_step_size = (y_grid_max - y_grid_min) / (y_res - 1);
	const float z_step_size = (z_grid_max - z_grid_min) / (z_res - 1);

	vector<float> xyplane0(x_res * y_res, 0);
	vector<float> xyplane1(x_res * y_res, 0);
	vector<triangle> triangles;
	size_t box_count = 0;

	size_t z = 0;

	quaternion Z(x_grid_min, y_grid_min, z_grid_min, z_w);

	// Calculate 0th xy plane.
	for (size_t x = 0; x < x_res; x++, Z.x += x_step_size)
	{
		Z.y = y_grid_min;

		for (size_t y = 0; y < y_res; y++, Z.y += y_step_size)
		{
			point_vertex_data.push_back(Z.x);
			point_vertex_data.push_back(Z.y);
			point_vertex_data.push_back(Z.z);
			point_vertex_data.push_back(Z.w);
		}
	}

	// Prepare for 1st xy plane.
	z++;
	Z.z += z_step_size;

	vector<vector<quaternion>> local_trajectories;

	vector<vector<quaternion>> all_trajectories;

	get_trajectories(
		point_vertex_data,
		local_trajectories,
		g0_mc_shader,
		C,
		max_iterations,
		threshold);

	for (size_t i = 0; i < local_trajectories.size(); i++)
	{
		if (local_trajectories[i].size() > 0)
			xyplane0[i] = (local_trajectories[i][local_trajectories[i].size() - 1]).magnitude();
		else
			xyplane0[i] = 0;

		all_trajectories.push_back(local_trajectories[i]);
	}



	// Calculate 1st and subsequent xy planes.
	for (; z < z_res; z++, Z.z += z_step_size)
	{
		point_vertex_data.clear();
		Z.x = x_grid_min;

		cout << "Calculating triangles from xy-plane pair " << z << " of " << z_res - 1 << endl;

		for (size_t x = 0; x < x_res; x++, Z.x += x_step_size)
		{
			Z.y = y_grid_min;

			for (size_t y = 0; y < y_res; y++, Z.y += y_step_size)
			{
				point_vertex_data.push_back(Z.x);
				point_vertex_data.push_back(Z.y);
				point_vertex_data.push_back(Z.z);
				point_vertex_data.push_back(Z.w);
			}
		}

		local_trajectories.clear();

		get_trajectories(
			point_vertex_data,
			local_trajectories,
			g0_mc_shader,
			C,
			max_iterations,
			threshold);

		for (size_t i = 0; i < local_trajectories.size(); i++)
		{
			if (local_trajectories[i].size() > 0)
				xyplane1[i] = (local_trajectories[i][local_trajectories[i].size() - 1]).magnitude();
			else
				xyplane1[i] = 0;

			all_trajectories.push_back(local_trajectories[i]);
		}

		// Calculate triangles for the xy-planes corresponding to z - 1 and z by marching cubes.
		tesselate_adjacent_xy_plane_pair(
			box_count,
			xyplane0, xyplane1,
			z - 1,
			triangles,
			threshold, // Use threshold as isovalue.
			x_grid_min, x_grid_max, x_res,
			y_grid_min, y_grid_max, y_res,
			z_grid_min, z_grid_max, z_res);

		// Swap memory pointers (fast) instead of performing a memory copy (slow).
		xyplane1.swap(xyplane0);
	}

	if (0 < triangles.size())
		write_triangles_to_binary_stereo_lithography_file(triangles, "out.stl");




	size_t in_set = 0;

	for (size_t i = 0; i < all_trajectories.size(); i++)
	{
		if (all_trajectories[i].size() > 0)
		{
			quaternion Q = all_trajectories[i][all_trajectories[i].size() - 1];

			if (Q.magnitude() < threshold)
				in_set++;
		}
	}

	cout << in_set << " of " << all_trajectories.size() << endl;

	return 0;
}



