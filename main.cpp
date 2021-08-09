#include <GL/glew.h>
#include <GL/glut.h>



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


class quaternion
{
public:
	inline quaternion(void) : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { /*default constructor*/ }
	inline quaternion(const float src_x, const float src_y, const float src_z, const float src_w) : x(src_x), y(src_y), z(src_z), w(src_w) { /* custom constructor */ }

	inline float self_dot(void) const
	{
		return x * x + y * y + z * z + w * w;
	}

	inline float magnitude(void) const
	{
		return sqrtf(self_dot());
	}

	quaternion operator*(const quaternion& right) const
	{
		quaternion ret;

		ret.x = x * right.x - y * right.y - z * right.z - w * right.w;
		ret.y = x * right.y + y * right.x + z * right.w - w * right.z;
		ret.z = x * right.z - y * right.w + z * right.x + w * right.y;
		ret.w = x * right.w + y * right.z - z * right.y + w * right.x;

		return ret;
	}

	quaternion operator+(const quaternion& right) const
	{
		quaternion ret;

		ret.x = x + right.x;
		ret.y = y + right.y;
		ret.z = z + right.z;
		ret.w = w + right.w;

		return ret;
	}

	quaternion operator-(const quaternion& right) const
	{
		quaternion ret;

		ret.x = x - right.x;
		ret.y = y - right.y;
		ret.z = z - right.z;
		ret.w = w - right.w;

		return ret;
	}


	float x, y, z, w;
};



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

	vertex_geometry_shader g0_mc_shader;

	if (false == g0_mc_shader.init("points.vs.glsl", "points.gs.glsl", "vert"))
	{
		cout << "Couldn't load shaders" << endl;
		return 0;
	}

	g0_mc_shader.use_program();

	// Make enough data for 1 point
	vector<float> point_vertex_data;

	size_t res = 30;

	float x_grid_max = 1.5;
	float y_grid_max = 1.5;
	float z_grid_max = 1.5;
	float x_grid_min = -x_grid_max;
	float y_grid_min = -y_grid_max;
	float z_grid_min = -z_grid_max;
	size_t x_res = res;
	size_t y_res = res;
	size_t z_res = res;

	float z_w = 0;
	quaternion C;
	C.x = 0.3f;
	C.y = 0.5f;
	C.z = 0.4f;
	C.w = 0.2f;
	int max_iterations = 8;
	float threshold = 4.0f;

	const float x_step_size = (x_grid_max - x_grid_min) / (x_res - 1);
	const float y_step_size = (y_grid_max - y_grid_min) / (y_res - 1);
	const float z_step_size = (z_grid_max - z_grid_min) / (z_res - 1);

	quaternion Z(x_grid_min, y_grid_min, z_grid_min, z_w);

	for (size_t z = 0; z < z_res; z++, Z.z += z_step_size)
	{
		cout << "Z slice " << z + 1 << " of " << z_res << endl;

		Z.x = x_grid_min;

		for (size_t x = 0; x < x_res; x++, Z.x += x_step_size)
		{
			Z.y = y_grid_min;

			for (size_t y = 0; y < y_res; y++, Z.y += y_step_size)
			{
				point_vertex_data.push_back(Z.x);
				point_vertex_data.push_back(Z.y);
				point_vertex_data.push_back(Z.z);
				point_vertex_data.push_back(z_w);
			}
		}
	}

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

	size_t max_output_vertices_per_input = 502;

	size_t max_vertices = max_output_vertices_per_input*num_vertices;
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

	vector<size_t> vec_sizes;

	size_t curr_size = 0;

	cout << primitives << endl;

	for (size_t i = 0; i < primitives; i++)
	{
		size_t feedback_index = 4 * i;

		if (feedback[feedback_index + 0] == 10000 &&
			feedback[feedback_index + 1] == 10000 &&
			feedback[feedback_index + 2] == 10000 &&
			feedback[feedback_index + 3] == 10000)
		{
			//cout << "found sentinel" << endl;
			vec_sizes.push_back(curr_size);
			curr_size = 0;
		}
		else
		{
			//cout << feedback[feedback_index + 0] << " " << feedback[feedback_index + 1] << "  " << feedback[feedback_index + 2] << " " << feedback[feedback_index + 3] << endl;

			curr_size++;
		}
	}

	cout << vec_sizes.size() << endl;




	return 1;
}



