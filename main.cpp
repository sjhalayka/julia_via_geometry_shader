
#include "main.h"



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
	size_t x_res = 1000;
	size_t y_res = 1000;
	size_t z_res = 1000;

	float z_w = 0;
	quaternion C;
	C.x = 0.3f;
	C.y = 0.5f;
	C.z = 0.4f;
	C.w = 0.2f;
	int max_iterations = 8;
	float threshold = 4.0f;

	vertex_geometry_shader g0_mc_shader;

	emit_shaders_to_files("points.vs.glsl", "points.gs.glsl", max_iterations);

	if (false == g0_mc_shader.init("points.vs.glsl", "points.gs.glsl", "vert"))
	{
		cout << "Couldn't load shaders" << endl;
		return 0;
	}

	g0_mc_shader.use_program();

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
		}
		
		size_t box_count = 0;
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

	return 0;
}



