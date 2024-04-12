#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include <vector>
#include <cmath>
#include <iostream>
#include "shader.h"
#include "shaderprogram.h"
#include "stb_image.h"

/*=================================================================================================
	DOMAIN
=================================================================================================*/

// Window dimensions
const int InitWindowWidth  = 800;
const int InitWindowHeight = 800;
int WindowWidth  = InitWindowWidth;
int WindowHeight = InitWindowHeight;

// Last mouse cursor position
int LastMousePosX = 0;
int LastMousePosY = 0;

// Arrays that track which keys are currently pressed
bool key_states[256];
bool key_special_states[256];
bool mouse_states[8];

// Other parameters
bool draw_wireframe = false;
bool show_normals = false;
float major_r = 5.0f;
float minor_r = 2.5f;
float num_segments = 16.0f;
GLuint IndexBufferID;
GLuint textures[3];
GLuint skybox;
int texture_selected = 0;

glm::vec3 eye, up;
glm::vec3 direction(0.0, 0.0, -1.0);

float yaw = 90.0;
float pitch = 0.0;

//Joystick parameters
int axis_count;
const float* axes;
GLFWgamepadstate state;

glm::vec3 player_pos(0.0, 0.0, 0.0);

/*=================================================================================================
	SHADERS & TRANSFORMATIONS
=================================================================================================*/

ShaderProgram PassthroughShader;
ShaderProgram SkyboxShader;
ShaderProgram PerspectiveShader;

glm::mat4 PerspProjectionMatrix( 1.0f );
glm::mat4 PerspViewMatrix( 1.0f );
glm::mat4 PerspModelMatrix( 1.0f );
glm::mat4 SkyboxViewMatrix(1.0f);

float perspZoom = 1.0f, perspSensitivity = 0.35f;
float perspRotationX = 0.0f, perspRotationY = 0.0f;

/*=================================================================================================
	FUNCTIONS
=================================================================================================*/

GLuint loadSkybox(std::vector<const char*> faces);

/*=================================================================================================
	OBJECTS
=================================================================================================*/

//VAO -> the object "as a whole", the collection of buffers that make up its data
//VBOs -> the individual buffers/arrays with data, for ex: one for coordinates, one for color, etc.

GLuint axis_VAO;
GLuint axis_VBO[2];

GLuint torus_VAO;
GLuint torus_VBO[4];

GLuint normal_VAO;
GLuint normal_VBO[2];

GLuint skybox_VAO;
GLuint skybox_VBO;

std::vector<float> axis_vertices = {
	10.0f, -8.0f, 10.0f, 1.0f,
	10.0f, -8.0f, -10.0f, 1.0f,
	-10.0f, -8.0f, -10.0f, 1.0f,
	-10.0f, -8.0f, 10.0f, 1.0f
};

std::vector<float> axis_colors = {
	1.0f, 0.0f, 0.0f, 1.0f,
	1.0f, 0.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
};

std::vector<float> skyboxVertices = {
	// positions          
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f
};

std::vector<float> torus_vertices;
std::vector<float> torus_colors;
std::vector<float> torus_textures;
std::vector<int> torus_indices;

std::vector<float> torus_normals;

std::vector<float> normal_vertices;
std::vector<float> normal_colors;

/*=================================================================================================
	HELPER FUNCTIONS
=================================================================================================*/

void window_to_scene( int wx, int wy, float& sx, float& sy )
{
	sx = ( 2.0f * (float)wx / WindowWidth ) - 1.0f;
	sy = 1.0f - ( 2.0f * (float)wy / WindowHeight );
}

void CreateTorusSmooth(float major, float minor, int num)
{
	float x, y, z, nx, ny, nz;
	float u = 0.0f;
	float v = 0.0f;
	glm::vec3 normal_vector;
	//Main segment
	for (float phi = 0; phi <= (2.0f * M_PI) + 0.01f; phi += (2.0f * M_PI)/num)
	{
		//Tube segment
		for (float theta = 0; theta <= (2.0f * M_PI) + 0.01f; theta += (2.0f * M_PI) / num)
		{
			x = (major + minor * cos(theta)) * cos(phi);
			y = (major + minor * cos(theta)) * sin(phi);
			z = minor * sin(theta);

			torus_vertices.push_back(x);
			torus_vertices.push_back(y);
			torus_vertices.push_back(z);
			torus_vertices.push_back(1.0f);

			torus_colors.push_back(1.0f);
			torus_colors.push_back(1.0f);
			torus_colors.push_back(1.0f);
			torus_colors.push_back(1.0f);

			nx = cos(phi) * cos(theta);
			ny = sin(phi) * cos(theta);
			nz = sin(theta);

			normal_vector = { nx, ny, nz };
			normalize(normal_vector);

			torus_normals.push_back(normal_vector[0]);
			torus_normals.push_back(normal_vector[1]);
			torus_normals.push_back(normal_vector[2]);
			torus_normals.push_back(1.0f);

			normal_vertices.push_back(x);
			normal_vertices.push_back(y);
			normal_vertices.push_back(z);
			normal_vertices.push_back(1.0f);
			normal_vertices.push_back(x + normal_vector[0]);
			normal_vertices.push_back(y + normal_vector[1]);
			normal_vertices.push_back(z + normal_vector[2]);
			normal_vertices.push_back(1.0f);

			normal_colors.push_back(1.0f);
			normal_colors.push_back(0.0f);
			normal_colors.push_back(0.0f);
			normal_colors.push_back(1.0f);
			normal_colors.push_back(1.0f);
			normal_colors.push_back(0.0f);
			normal_colors.push_back(0.0f);
			normal_colors.push_back(1.0f);

			torus_textures.push_back(u);
			torus_textures.push_back(v);

			v += 1.0 / num;
		}
		v = 0;
		u += 1.0 / num;
	}

	int vertex_index = 0;

	for (int i = 0; i < num; i++)
	{
		for (int j = 0; j < num + 1; j++)
		{
			torus_indices.push_back(vertex_index);
			torus_indices.push_back(vertex_index + num + 1);

			vertex_index++;
		}
		//Does not push the restart index if the end of the torus has been reached
		if (i != num - 1)
		{
			torus_indices.push_back(99999);
		}
	}
}

void CreateTextures(void)
{
	std::vector<const char*> faces
	{
		"./textures/city_skybox_right.png",
		"./textures/city_skybox_left.png",
		"./textures/city_skybox_top.png",
		"./textures/city_skybox_bottom.png",
		"./textures/city_skybox_front.png",
		"./textures/city_skybox_back.png"
	};

	glGenTextures(3, textures);

	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("./textures/stripes.jpg", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);

	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	data = stbi_load("./textures/cobblestone.jpg", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}

	stbi_image_free(data);

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	data = stbi_load("./textures/rainbow.jpg", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}

	stbi_image_free(data);

	skybox = loadSkybox(faces);
}

GLuint loadSkybox(std::vector<const char*> faces)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i], &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

/*=================================================================================================
	SHADERS
=================================================================================================*/

void CreateTransformationMatrices( void )
{
	// PROJECTION MATRIX
	PerspProjectionMatrix = glm::perspective<float>( glm::radians( 60.0f ), (float)WindowWidth / (float)WindowHeight, 0.01f, 1000.0f );

	// VIEW MATRIX
	glm::vec3 player_center(player_pos.x, player_pos.y, player_pos.z);
	eye.x = player_pos.x + (40.0 * cos(glm::radians(yaw)) * cos(glm::radians(pitch)));
	eye.y = player_pos.y + (40.0 * sin(glm::radians(pitch)));
	eye.z = player_pos.z + (40.0 * sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
	up = glm::vec3(0.0, 1.0, 0.0);

	PerspViewMatrix = glm::lookAt(eye, player_center, up);
	SkyboxViewMatrix = glm::mat4(glm::mat3(PerspViewMatrix));

	// MODEL MATRIX
	PerspModelMatrix = glm::mat4( 1.0 );
	PerspModelMatrix = glm::rotate(PerspModelMatrix, glm::radians(perspRotationX), glm::vec3(1.0, 0.0, 0.0));
	PerspModelMatrix = glm::rotate(PerspModelMatrix, glm::radians(perspRotationY), glm::vec3(0.0, 1.0, 0.0));
	PerspModelMatrix = glm::scale( PerspModelMatrix, glm::vec3( perspZoom ) );
}

void CreateShaders( void )
{
	// Renders using perspective projection
	SkyboxShader.Create( "./shaders/skybox.vert", "./shaders/skybox.frag" );

	PerspectiveShader.Create("./shaders/texpersplight.vert", "./shaders/texpersplight.frag");
}

/*=================================================================================================
	BUFFERS
=================================================================================================*/
void CreateNormalBuffers(void)
{
	glGenVertexArrays(1, &normal_VAO);
	glBindVertexArray(normal_VAO);

	glGenBuffers(2, &normal_VBO[0]);

	glBindBuffer(GL_ARRAY_BUFFER, normal_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normal_vertices[0]) * normal_vertices.size(), &normal_vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, normal_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normal_colors[0]) * normal_colors.size(), &normal_colors[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
}

void CreateFloorBuffers()
{
	glGenVertexArrays(1, &axis_VAO);
	glBindVertexArray(axis_VAO);

	glGenBuffers(2, &axis_VBO[0]);

	glBindBuffer(GL_ARRAY_BUFFER, axis_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axis_vertices[0]) * axis_vertices.size(), &axis_vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, axis_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axis_colors[0]) * axis_colors.size(), &axis_colors[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
}

void CreateTorusBuffers(void)
{
	CreateTorusSmooth(major_r, minor_r, num_segments);

	glGenVertexArrays(1, &torus_VAO);
	glBindVertexArray(torus_VAO);

	glGenBuffers(4, &torus_VBO[0]);

	glBindBuffer(GL_ARRAY_BUFFER, torus_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(torus_vertices[0]) * torus_vertices.size(), &torus_vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, torus_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(torus_colors[0]) * torus_colors.size(), &torus_colors[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, torus_VBO[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(torus_normals[0]) * torus_normals.size(), &torus_normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, torus_VBO[3]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(torus_textures[0]) * torus_textures.size(), &torus_textures[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(3);

	//Generates buffers for the indices of the torus
	glGenBuffers(1, &IndexBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(torus_indices[0]) * torus_indices.size(), &torus_indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void CreateSkyboxBuffers()
{
	glGenVertexArrays(1, &skybox_VAO);
	glBindVertexArray(skybox_VAO);

	glGenBuffers(1, &skybox_VBO);

	glBindBuffer(GL_ARRAY_BUFFER, skybox_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices[0]) * skyboxVertices.size(), &skyboxVertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

//Clears torus values and calls to create a new torus with updated parameters
void UpdateTorus(void)
{
	torus_vertices.clear();
	torus_colors.clear();
	torus_textures.clear();
	torus_indices.clear();
	torus_normals.clear();

	normal_vertices.clear();
	normal_colors.clear();

	CreateTorusBuffers();
	CreateNormalBuffers();
}

void Draw(GLuint VAO, int size, GLenum primitive)
{
	glBindVertexArray(VAO);
	glDrawArrays(primitive, 0, size);
	glBindVertexArray(0);
}

void GamepadInput()
{
	axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);

	if (axes[2] > 0.1 || axes[2] < -0.1)
		yaw += axes[2];
	if (axes[3] > 0.1 || axes[3] < -0.1)
		pitch += axes[3];

	if (pitch > 70.0)
		pitch = 70.0;
	if (pitch < 0.0)
		pitch = 0.0;

	if(axes[0] > 0.1 || axes[0] < -0.1)
		player_pos.x += axes[0] / 10.0f;
	if(axes[1] > 0.1 || axes[1] < -0.1)
		player_pos.z += axes[1] / 10.0f;

	PerspModelMatrix = glm::translate(PerspModelMatrix, player_pos);
	PerspectiveShader.SetUniform("modelMatrix", glm::value_ptr(PerspModelMatrix), 4, GL_FALSE, 1);
	Draw(axis_VAO, axis_vertices.size()/4, GL_QUADS);
}

/*=================================================================================================
	CALLBACKS
=================================================================================================*/

//-----------------------------------------------------------------------------
// CALLBACK DOCUMENTATION
// https://www.opengl.org/resources/libraries/glut/spec3/node45.html
// http://freeglut.sourceforge.net/docs/api.php#WindowCallback
//-----------------------------------------------------------------------------

void idle_func()
{
	//uncomment below to repeatedly draw new frames
	glutPostRedisplay();
}

void reshape_func( int width, int height )
{
	WindowWidth  = width;
	WindowHeight = height;

	glViewport( 0, 0, width, height );
	glutPostRedisplay();
}

void keyboard_func( unsigned char key, int x, int y )
{
	key_states[ key ] = true;

	switch( key )
	{
		case 'r':
		{
			draw_wireframe = !draw_wireframe;
			if( draw_wireframe == true )
				std::cout << "Wireframes on.\n";
			else
				std::cout << "Wireframes off.\n";
			break;
		}

		case 'q':
		{
			num_segments++;
			UpdateTorus();
			break;
		}

		case 'a':
		{
			if (num_segments > 1)
			{
				num_segments--;
				UpdateTorus();
			}
			break;
		}

		case 'w':
		{
			minor_r += 0.1f;
			UpdateTorus();
			break;
		}

		case 's':
		{
			minor_r -= 0.1f;
			UpdateTorus();
			break;
		}

		case 'e':
		{
			major_r += 0.1f;
			UpdateTorus();
			break;
		}

		case 'd':
		{
			major_r -= 0.1f;
			UpdateTorus();
			break;
		}

		case 'c':
		{
			show_normals = !show_normals;
			UpdateTorus();
			break;
		}

		case 't':
		{
			std::cout << yaw << ", " << pitch << std::endl;
			break;
		}

		case 'y':
		{
			std::cout << glfwGetTime() << std::endl;
			break;
		}

		case ' ':
		{
			if (texture_selected < 2)
				texture_selected++;
			else
				texture_selected = 0;
			break;
		}

		// Exit on escape key press
		case '\x1B':
		{
			exit( EXIT_SUCCESS );
			break;
		}
	}
}

void key_released( unsigned char key, int x, int y )
{
	key_states[ key ] = false;
}

void key_special_pressed( int key, int x, int y )
{
	key_special_states[ key ] = true;
}

void key_special_released( int key, int x, int y )
{
	key_special_states[ key ] = false;
}

void mouse_func( int button, int state, int x, int y )
{
	// Key 0: left button
	// Key 1: middle button
	// Key 2: right button
	// Key 3: scroll up
	// Key 4: scroll down

	if( x < 0 || x > WindowWidth || y < 0 || y > WindowHeight )
		return;

	float px, py;
	window_to_scene( x, y, px, py );

	if( button == 3 )
	{
		perspZoom += 0.03f;
	}
	else if( button == 4 )
	{
		if( perspZoom - 0.03f > 0.0f )
			perspZoom -= 0.03f;
	}

	mouse_states[ button ] = ( state == GLUT_DOWN );

	LastMousePosX = x;
	LastMousePosY = y;
}

void passive_motion_func( int x, int y )
{
	if( x < 0 || x > WindowWidth || y < 0 || y > WindowHeight )
		return;

	float px, py;
	window_to_scene( x, y, px, py );

	LastMousePosX = x;
	LastMousePosY = y;
}

void active_motion_func( int x, int y )
{
	if( x < 0 || x > WindowWidth || y < 0 || y > WindowHeight )
		return;

	float px, py;
	window_to_scene( x, y, px, py );

	if( mouse_states[0] == true )
	{
		perspRotationY += ( x - LastMousePosX ) * perspSensitivity;
		perspRotationX += ( y - LastMousePosY ) * perspSensitivity;
	}

	LastMousePosX = x;
	LastMousePosY = y;
}

/*=================================================================================================
	RENDERING
=================================================================================================*/

void display_func( void )
{
	// Clear the contents of the back buffer
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// Update transformation matrices
	CreateTransformationMatrices();

	// Drawing in wireframe?
	if( draw_wireframe == true )
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	else
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	
	PerspectiveShader.Use();
	PerspectiveShader.SetUniform("projectionMatrix", glm::value_ptr(PerspProjectionMatrix), 4, GL_FALSE, 1);
	PerspectiveShader.SetUniform("viewMatrix", glm::value_ptr(PerspViewMatrix), 4, GL_FALSE, 1);
	PerspectiveShader.SetUniform("modelMatrix", glm::value_ptr(PerspModelMatrix), 4, GL_FALSE, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[texture_selected]);
	glBindVertexArray(torus_VAO);
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(99999);
	glDrawElements(GL_TRIANGLE_STRIP, torus_indices.size(), GL_UNSIGNED_INT, (GLvoid*)0);
	glDisable(GL_PRIMITIVE_RESTART);

	if (show_normals)
	{
		glBindVertexArray(normal_VAO);
		glDrawArrays(GL_LINES, 0, normal_vertices.size() / 4);
	}
	glBindVertexArray(0);

	if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
		GamepadInput();
	
	PerspectiveShader.SetUniform("modelMatrix", glm::value_ptr(PerspModelMatrix), 4, GL_FALSE, 1);
	Draw(axis_VAO, axis_vertices.size()/4, GL_QUADS);

	SkyboxShader.Use();
	SkyboxShader.SetUniform("projectionMatrix", glm::value_ptr(PerspProjectionMatrix), 4, GL_FALSE, 1);
	SkyboxShader.SetUniform("viewMatrix", glm::value_ptr(SkyboxViewMatrix), 4, GL_FALSE, 1);

	glDepthFunc(GL_LEQUAL);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
	Draw(skybox_VAO, 36, GL_TRIANGLES);
	glDepthFunc(GL_LESS);

	// Swap the front and back buffers
	glutSwapBuffers();
}

/*=================================================================================================
	INIT
=================================================================================================*/

void init( void )
{
	// Print some info
	std::cout << "Vendor:         " << glGetString( GL_VENDOR   ) << "\n";
	std::cout << "Renderer:       " << glGetString( GL_RENDERER ) << "\n";
	std::cout << "OpenGL Version: " << glGetString( GL_VERSION  ) << "\n";
	std::cout << "GLSL Version:   " << glGetString( GL_SHADING_LANGUAGE_VERSION ) << "\n\n";

	// Set OpenGL settings
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f ); // background color
	glEnable( GL_DEPTH_TEST ); // enable depth test
	glEnable( GL_CULL_FACE ); // enable back-face culling

	// Create shaders
	CreateShaders();

	// Create buffers
	CreateFloorBuffers();
	CreateTorusBuffers();
	CreateSkyboxBuffers();

	CreateTextures();

	std::cout << "Finished initializing...\n\n";
}

/*=================================================================================================
	MAIN
=================================================================================================*/

int main( int argc, char** argv )
{
	// Create and initialize the OpenGL context
	glutInit( &argc, argv );

	glutInitWindowPosition( 100, 100 );
	glutInitWindowSize( InitWindowWidth, InitWindowHeight );
	glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH );

	glutCreateWindow( "CSE-170 Computer Graphics" );

	// Initialize GLEW
	GLenum ret = glewInit();
	if( ret != GLEW_OK ) {
		std::cerr << "GLEW initialization error." << std::endl;
		glewGetErrorString( ret );
		return -1;
	}

	//Initialize GLFW
	glfwInit();
	if (!glfwInit())
	{
		std::cerr << "GLFW initialization error." << std::endl;
		return -1;
	}

	// Register callback functions
	glutDisplayFunc( display_func );
	glutIdleFunc( idle_func );
	glutReshapeFunc( reshape_func );
	glutKeyboardFunc( keyboard_func );
	glutKeyboardUpFunc( key_released );
	glutSpecialFunc( key_special_pressed );
	glutSpecialUpFunc( key_special_released );
	glutMouseFunc( mouse_func );
	glutMotionFunc( active_motion_func );
	glutPassiveMotionFunc( passive_motion_func );

	// Do program initialization
	init();

	// Enter the main loop
	glutMainLoop();

	glfwTerminate();
	return EXIT_SUCCESS;
}
