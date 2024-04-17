#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
GLuint tex[3];
GLuint skybox;
int texture_selected = 0;

glm::vec3 eye, up, direction;

float yaw = 0.0;
float pitch = 30.0;

//Joystick parameters
int axis_count;
const float* axes;
float left_joystick_x, left_joystick_z;
GLFWgamepadstate state;

glm::vec3 player_pos(0.0, 0.0, 0.0);
glm::vec3 camera_direction_vector;
glm::vec3 player_direction_vector;
glm::vec3 difference;
float player_yaw = 90.0;
float goal_yaw = 90.0;
float rotation_speed = 5.0f;

/*=================================================================================================
	SHADERS & TRANSFORMATIONS
=================================================================================================*/

ShaderProgram PassthroughShader;
ShaderProgram SkyboxShader;
ShaderProgram PerspectiveShader;

glm::mat4 PerspProjectionMatrix( 1.0f );
glm::mat4 PerspViewMatrix( 1.0f );
glm::mat4 PerspModelMatrix( 1.0f );
glm::mat4 PlayerModelMatrix(1.0f);
glm::mat4 SkyboxViewMatrix(1.0f);

float perspZoom = 1.0f, perspSensitivity = 0.35f;
float perspRotationX = 0.0f, perspRotationY = 0.0f;

/*=================================================================================================
	FUNCTIONS
=================================================================================================*/

GLuint loadSkybox(std::vector<const char*> faces);
GLuint TextureFromFile(aiTexture *texture);

/*=================================================================================================
	CLASSES
=================================================================================================*/
struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
};

struct Texture {
	GLuint id;
	std::string type;
};

class Mesh {
	public:
		std::vector<Vertex>  vertices;
		std::vector<GLuint>  indices;
		std::vector<Texture> textures;
		GLuint VAO;

		Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures)
		{
			this->vertices = vertices;
			this->indices = indices;
			this->textures = textures;

			setupMesh();
		}
		void Draw()
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[0].id);

			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, static_cast<GLuint>(indices.size()), GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
			glActiveTexture(GL_TEXTURE0);
		}
	private:
		GLuint VBO, EBO;
		void setupMesh()
		{
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
			glGenBuffers(1, &EBO);

			glBindVertexArray(VAO);
			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

			glBindVertexArray(0);
		}
};

class Model {
	public:
		Model(std::string path)
		{
			loadModel(path);
		}
		void Draw()
		{
			for (GLuint i = 0; i < meshes.size(); i++)
				meshes[i].Draw();
		}
	private:
		std::vector<Mesh> meshes;

		void loadModel(std::string path)
		{
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
			if (!scene)
			{
				std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
				return;
			}
			processNode(scene->mRootNode, scene);
		}
		void processNode(aiNode *node, const aiScene *scene)
		{
			for (GLuint i = 0; i < node->mNumMeshes; i++)
			{
				aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
				meshes.push_back(processMesh(mesh, scene));
			}
			for (GLuint i = 0; i < node->mNumChildren; i++)
			{
				processNode(node->mChildren[i], scene);
			}
		}
		Mesh processMesh(aiMesh* mesh, const aiScene* scene)
		{
			std::vector<Vertex> vertices;
			std::vector<GLuint> indices;
			std::vector<Texture> textures;

			for (GLuint i = 0; i < mesh->mNumVertices; i++)
			{
				Vertex vertex;
				glm::vec3 vector;

				vector.x = mesh->mVertices[i].x;
				vector.y = mesh->mVertices[i].y;
				vector.z = mesh->mVertices[i].z;
				vertex.Position = vector;

				if (mesh->HasNormals())
				{
					vector.x = mesh->mNormals[i].x;
					vector.y = mesh->mNormals[i].y;
					vector.z = mesh->mNormals[i].z;
					vertex.Normal = vector;
				}

				if (mesh->mTextureCoords[0])
				{
					glm::vec2 vec;
					vec.x = mesh->mTextureCoords[0][i].x;
					vec.y = mesh->mTextureCoords[0][i].y;
					vertex.TexCoords = vec;
				}
				else
					vertex.TexCoords = glm::vec2(0.0f, 0.0f);

				vertices.push_back(vertex);
			}

			for (GLuint i = 0; i < mesh->mNumFaces; i++)
			{
				aiFace face = mesh->mFaces[i];

				for (GLuint j = 0; j < face.mNumIndices; j++)
					indices.push_back(face.mIndices[j]);
			}
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiString str;
			material->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), str);
			aiTexture *texture = scene->mTextures[atoi(str.C_Str())];
			Texture tex;
			tex.id = TextureFromFile(texture);
			tex.type = "texture_diffuse";
			textures.push_back(tex);

			Mesh temp(vertices, indices, textures);
			return temp;
		}
};

GLuint TextureFromFile(aiTexture *texture)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/player.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(texture->pcData);
	}

	return textureID;
}

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

Model *player;

/*=================================================================================================
	HELPER FUNCTIONS
=================================================================================================*/

void window_to_scene( int wx, int wy, float& sx, float& sy )
{
	sx = ( 2.0f * (float)wx / WindowWidth ) - 1.0f;
	sy = 1.0f - ( 2.0f * (float)wy / WindowHeight );
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
	glm::vec3 player_center(player_pos.x, player_pos.y + 10.0f, player_pos.z);
	direction = glm::vec3(cos(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(pitch)), sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
	eye.x = player_pos.x + ((10.0 + 1.5*pitch) * direction.x);
	eye.y = player_pos.y + ((10.0 + 1.5*pitch) * direction.y);
	eye.z = player_pos.z + ((10.0 + 1.5*pitch) * direction.z);
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

void Draw(GLuint VAO, int size, GLenum primitive)
{
	glBindVertexArray(VAO);
	glDrawArrays(primitive, 0, size);
	glBindVertexArray(0);
}

void GamepadInput()
{
	axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);
	camera_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw))), up));
	player_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(player_yaw - 90.0)), 0, sin(glm::radians(player_yaw - 90.0))), up));
	difference = player_direction_vector - camera_direction_vector;

	if (axes[2] > 0.1 || axes[2] < -0.1)
		yaw += axes[2];
	if (axes[3] > 0.1 || axes[3] < -0.1)
		pitch += axes[3];

	if (pitch > 70.0)
		pitch = 70.0;
	if (pitch < 7.0)
		pitch = 7.0;

	if (axes[0] > 0.1 || axes[0] < -0.1)
	{
		player_pos -= axes[0] / 5.0f * camera_direction_vector;
		player_yaw = yaw + 90.0;
		/*if (sin(glm::radians(player_yaw - yaw)) > 0.0)
			player_yaw += rotation_speed * axes[0];
		else if (sin(glm::radians(player_yaw - yaw)) < 0.0)
			player_yaw -= rotation_speed * axes[0];*/
	}
	if (axes[1] > 0.1 || axes[1] < -0.1)
	{
		player_pos += axes[1] / 5.0f * glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw)));
		player_yaw = yaw + 90.0;
		/*if (cos(glm::radians(player_yaw - yaw)) > 0.0)
			player_yaw -= rotation_speed * axes[1];
		else if (cos(glm::radians(player_yaw - yaw)) < 0.0)
			player_yaw += rotation_speed * axes[1];*/
	}

	PerspModelMatrix = glm::translate(PerspModelMatrix, player_pos) * glm::rotate(PerspModelMatrix, glm::radians(-player_yaw), glm::vec3(0.0, 1.0, 0.0)) * glm::translate(PerspModelMatrix, -player_pos);
	PerspModelMatrix = glm::translate(PerspModelMatrix, player_pos);
	PerspModelMatrix = glm::scale(PerspModelMatrix, glm::vec3(4.0f, 4.0f, 4.0f));
	PerspectiveShader.SetUniform("modelMatrix", glm::value_ptr(PerspModelMatrix), 4, GL_FALSE, 1);
	player->Draw();
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

		case 'a':
		{
			yaw += 2.0;
			break;
		}

		case 'w':
		{
			if (pitch > 7.0)
				pitch -= 2.0;
			break;
		}

		case 's':
		{
			if (pitch < 70.0)
				pitch += 2.0;
			break;
		}

		case 'd':
		{
			yaw -= 2.0;
			break;
		}

		case 't':
		{
			std::cout << "Camera: " << camera_direction_vector.x << ", " << camera_direction_vector.z << std::endl;
			std::cout << "Player: " << player_direction_vector.x << ", " << player_direction_vector.z << std::endl;
			std::cout << "Difference: " << player_direction_vector.x  - camera_direction_vector.x << ", " << player_direction_vector.z - camera_direction_vector.z << std::endl << std::endl;
			break;
		}

		case 'y':
		{
			std::cout << glfwGetTime() << std::endl;
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
	//Up arrow
	if (key == 101)
	{
		player_pos -= 0.5f * glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw)));
	}
	//Down arrow
	if (key == 103)
	{
		player_pos += 0.5f * glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw)));
	}
	//Left arrow
	if (key == 100)
	{
		camera_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw))), up));
		player_pos += 0.5f * camera_direction_vector;
	}
	//Right arrow
	if (key == 102)
	{
		camera_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw))), up));
		player_pos -= 0.5f * camera_direction_vector;
	}
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

	Draw(axis_VAO, axis_vertices.size() / 4, GL_QUADS);

	if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
		GamepadInput();

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
	CreateSkyboxBuffers();

	CreateTextures();

	player = new Model("models/player.glb");
	player->Draw();

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
