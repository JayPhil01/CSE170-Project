#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#define MAX_BONE_INFLUENCE 4

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
#include <unordered_map>
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

//Camera
glm::vec3 eye, direction;
glm::vec3 up(0.0, 1.0, 0.0);
float yaw = 0.0;
float pitch = 30.0;

//Joystick parameters
int axis_count;
const float *axes;
int button_count;
const unsigned char *buttons;
GLFWgamepadstate state;

//Jumping and falling
static bool jumping = false;
static bool standing = true;
static float jump_start = 0.0f;
static float fall_start = 0.0f;
static float jump_height = 0.5f;
static float gravity = 9.81f;
static float jump_velocity = 0.0f;
static float initial_jump_pos = 0.0f;
static float jump_displacement = 0.0f;

//Player movement and direction-facing
glm::vec3 player_pos(0.0, 0.0, 0.0);
glm::vec3 camera_direction_vector;
glm::vec3 player_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(yaw + 90.0)), 0, sin(glm::radians(yaw + 90.0))), up));
glm::vec3 direction_vector1(0.0, 0.0, 0.0);
glm::vec3 direction_vector2(0.0, 0.0, 0.0);
glm::vec3 respawn_point(0.0, 0.0, 0.0);

//Animation
float lastFrame = 0.0f;
float lastFrameAnim = 0.0f;
float deltaTime = 0.0f;
int animationNum = 6;

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
GLuint TextureFromFile();
GLuint FloorTexture1();
GLuint FloorTexture2();
GLuint FloorTexture3();

/*=================================================================================================
	CLASSES
=================================================================================================*/
struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
	GLuint id;
	std::string type;
};

struct BoneInfo
{
	int id;
	glm::mat4 offset;
};

class AssimpGLMHelpers
{
public:

	static inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	static inline glm::vec3 GetGLMVec(const aiVector3D& vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	static inline glm::quat GetGLMQuat(const aiQuaternion& pOrientation)
	{
		return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
	}
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
			glEnableVertexAttribArray(3);
			glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

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
		auto& GetBoneInfoMap() { return m_BoneInfoMap; }
		int& GetBoneCount() { return m_BoneCounter; }
	private:
		std::vector<Mesh> meshes;
		std::unordered_map<std::string, BoneInfo> m_BoneInfoMap;
		int m_BoneCounter = 0;

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

				SetVertexBoneDataToDefault(vertex);

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
			tex.id = TextureFromFile();
			tex.type = "texture_diffuse";
			textures.push_back(tex);

			ExtractBoneWeightForVertices(vertices, mesh, scene);

			Mesh temp(vertices, indices, textures);
			return temp;
		}
		void SetVertexBoneDataToDefault(Vertex& vertex)
		{
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
			{
				vertex.m_BoneIDs[i] = -1;
				vertex.m_Weights[i] = 0.0f;
			}
		}

		void SetVertexBoneData(Vertex& vertex, int boneID, float weight)
		{
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
			{
				if (vertex.m_BoneIDs[i] < 0)
				{
					if (weight == 0)
						weight = 0.000001;
					vertex.m_Weights[i] = weight;
					vertex.m_BoneIDs[i] = boneID;
					break;
				}
			}
		}


		void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
		{
			auto& boneInfoMap = m_BoneInfoMap;
			int& boneCount = m_BoneCounter;

			for (int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
			{
				int boneID = -1;
				std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
				if (boneInfoMap.find(boneName) == boneInfoMap.end())
				{
					BoneInfo newBoneInfo;
					newBoneInfo.id = boneCount;
					newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
					boneInfoMap[boneName] = newBoneInfo;
					boneID = boneCount;
					boneCount++;
				}
				else
				{
					boneID = boneInfoMap[boneName].id;
				}
				assert(boneID != -1);
				auto weights = mesh->mBones[boneIndex]->mWeights;
				int numWeights = mesh->mBones[boneIndex]->mNumWeights;

				for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
				{
					int vertexId = weights[weightIndex].mVertexId;
					float weight = weights[weightIndex].mWeight;
					if (boneName.compare("Arm.002.R") == 0 && vertexId == 0 && strcmp(mesh->mName.C_Str(), "Cube.013") == 0)
					{
						std::cout << "This is the incorrect vertice on the arm: " << vertexId << std::endl;

					}
					assert(vertexId <= vertices.size());
					SetVertexBoneData(vertices[vertexId], boneID, weight);

				}
			}
		}
};

GLuint TextureFromFile()
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	return textureID;
}

class rectangularPrism {
public:
	float x;
	float y;
	float z;
	float length;
	float width;
	float height;
	bool isCheckpoint;
	bool isFinish;

	rectangularPrism(float x, float y, float z, float length, float width, float height, bool isCheckpoint, bool isFinish) {
		this->x = x;
		this->y = y;
		this->z = z;
		this->length = length;
		this->width = width;
		this->height = height;
		this->isCheckpoint = isCheckpoint;
		this->isFinish = isFinish;

		std::vector<Vertex> vertices = calcVertices();
		std::vector<GLuint>  indices;
		for (int i = 0; i < 36; i++) {
			indices.push_back(i);
		}
		std::vector<Texture> textures;
		Texture tex;
		if (!isCheckpoint && !isFinish)
			tex.id = FloorTexture1();
		else if (isCheckpoint && !isFinish)
			tex.id = FloorTexture2();
		tex.type = "texture_diffuse";
		textures.push_back(tex);
		mesh = new Mesh(vertices, indices, textures);
	}

	void Draw() {
		mesh->Draw();
	}

	float minX() {
		if (length >= 0) {
			return x;
		}
		else {
			return x + length;
		}
	}

	float maxX() {
		if (length >= 0) {
			return x + length;
		}
		else {
			return x;
		}
	}

	float minY() {
		if (width >= 0) {
			return y;
		}
		else {
			return y + width;
		}
	}

	float maxY() {
		if (width >= 0) {
			return y + width;
		}
		else {
			return y;
		}
	}

	float minZ() {
		if (height >= 0) {
			return z;
		}
		else {
			return z + height;
		}
	}

	float maxZ() {
		if (height >= 0) {
			return z + height;
		}
		else {
			return z;
		}
	}

	void deleteMesh() {
		delete mesh;
	}

private:
	Mesh* mesh;

	enum Direction {
		xpos,
		xneg,
		ypos,
		yneg,
		zpos,
		zneg
	};

	enum texPos {
		tl,
		tr,
		bl,
		br
	};

	Vertex calcVertex(float xPos, float yPos, float zPos, Direction dir, texPos tex) {
		Vertex vertex;
		glm::vec3 vector;

		//Position
		vector.x = xPos;
		vector.y = yPos;
		vector.z = zPos;
		vertex.Position = vector;

		//Normals
		float normx = 0;
		float normy = 0;
		float normz = 0;
		switch (dir) {
		case xpos:
			normx = 1;
			break;
		case xneg:
			normx = -1;
			break;
		case ypos:
			normy = 1;
			break;
		case yneg:
			normy = -1;
			break;
		case zpos:
			normz = 1;
			break;
		case zneg:
			normz = -1;
			break;
		}

		vector.x = normx;
		vector.y = normy;
		vector.z = normz;
		vertex.Normal = vector;

		//Texture Coordinates
		glm::vec2 vec;

		float texX;
		float texY;

		switch (tex) {
		case tl:
			texX = 1;
			texY = 0;
			break;
		case tr:
			texX = 1;
			texY = 1;
			break;
		case bl:
			texX = 0;
			texY = 0;
			break;
		case br:
			texX = 0;
			texY = 1;
			break;
		}
		vec.x = texX;
		vec.y = texY;
		vertex.TexCoords = vec;

		return vertex;
	}

	std::vector<Vertex> calcVertices() {
		std::vector<Vertex>  vertices;
		for (GLuint i = 0; i < 6; i++) //Different Faces
		{
			Direction dir = Direction(i);
			float x2 = x + length;
			float y2 = y + width;
			float z2 = z + height;
			switch (dir) {
			case xpos:
				vertices.push_back(calcVertex(x2, y, z, Direction(i), bl));
				vertices.push_back(calcVertex(x2, y2, z, Direction(i), br));
				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));

				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x2, y, z2, Direction(i), tl));
				vertices.push_back(calcVertex(x2, y, z, Direction(i), bl));
				break;
			case xneg:
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				vertices.push_back(calcVertex(x, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x, y2, z, Direction(i), br));

				vertices.push_back(calcVertex(x, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				vertices.push_back(calcVertex(x, y, z2, Direction(i), tl));
				break;
			case ypos:
				vertices.push_back(calcVertex(x, y2, z, Direction(i), bl));
				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x2, y2, z, Direction(i), br));

				vertices.push_back(calcVertex(x, y2, z2, Direction(i), tl));
				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x, y2, z, Direction(i), bl));
				break;
			case yneg:
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				vertices.push_back(calcVertex(x2, y, z, Direction(i), br));
				vertices.push_back(calcVertex(x2, y, z2, Direction(i), tr));

				vertices.push_back(calcVertex(x2, y, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x, y, z2, Direction(i), tl));
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				break;
			case zpos:
				vertices.push_back(calcVertex(x, y, z2, Direction(i), bl));
				vertices.push_back(calcVertex(x2, y, z2, Direction(i), br));
				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));

				vertices.push_back(calcVertex(x2, y2, z2, Direction(i), tr));
				vertices.push_back(calcVertex(x, y2, z2, Direction(i), tl));
				vertices.push_back(calcVertex(x, y, z2, Direction(i), bl));
				break;
			case zneg:
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				vertices.push_back(calcVertex(x2, y2, z, Direction(i), tr));
				vertices.push_back(calcVertex(x2, y, z, Direction(i), br));

				vertices.push_back(calcVertex(x, y2, z, Direction(i), tl));
				vertices.push_back(calcVertex(x2, y2, z, Direction(i), tr));
				vertices.push_back(calcVertex(x, y, z, Direction(i), bl));
				break;
			}
		}
		return vertices;
	}
};

GLuint FloorTexture1()
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/casset_block_1.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	return textureID;
}
GLuint FloorTexture2()
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/wood_3.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	return textureID;
}
GLuint FloorTexture3()
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/special_floor_1.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	return textureID;
}

struct KeyPosition
{
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation
{
	glm::quat orientation;
	float timeStamp;
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp;
};

class Bone
{
public:
	Bone(const std::string& name, int ID, const aiNodeAnim* channel)
		:
		m_Name(name),
		m_ID(ID),
		m_LocalTransform(1.0f)
	{
		m_NumPositions = channel->mNumPositionKeys;

		for (int positionIndex = 0; positionIndex < m_NumPositions; ++positionIndex)
		{
			aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
			float timeStamp = channel->mPositionKeys[positionIndex].mTime;
			KeyPosition data;
			data.position = AssimpGLMHelpers::GetGLMVec(aiPosition);
			data.timeStamp = timeStamp;
			m_Positions.push_back(data);
		}

		m_NumRotations = channel->mNumRotationKeys;
		for (int rotationIndex = 0; rotationIndex < m_NumRotations; ++rotationIndex)
		{
			aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
			float timeStamp = channel->mRotationKeys[rotationIndex].mTime;
			KeyRotation data;
			data.orientation = AssimpGLMHelpers::GetGLMQuat(aiOrientation);
			data.timeStamp = timeStamp;
			m_Rotations.push_back(data);
		}

		m_NumScalings = channel->mNumScalingKeys;
		for (int keyIndex = 0; keyIndex < m_NumScalings; ++keyIndex)
		{
			aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
			float timeStamp = channel->mScalingKeys[keyIndex].mTime;
			KeyScale data;
			data.scale = AssimpGLMHelpers::GetGLMVec(scale);
			data.timeStamp = timeStamp;
			m_Scales.push_back(data);
		}
	}

	void Update(float animationTime)
	{
		glm::mat4 translation = InterpolatePosition(animationTime);
		glm::mat4 rotation = InterpolateRotation(animationTime);
		glm::mat4 scale = InterpolateScaling(animationTime);
		m_LocalTransform = translation * rotation * scale;
	}
	glm::mat4 GetLocalTransform() { return m_LocalTransform; }
	std::string GetBoneName() const { return m_Name; }
	int GetBoneID() { return m_ID; }



	int GetPositionIndex(float animationTime)
	{
		for (int index = 0; index < m_NumPositions - 1; ++index)
		{
			if (animationTime < m_Positions[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	int GetRotationIndex(float animationTime)
	{
		for (int index = 0; index < m_NumRotations - 1; ++index)
		{
			if (animationTime < m_Rotations[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	int GetScaleIndex(float animationTime)
	{
		for (int index = 0; index < m_NumScalings - 1; ++index)
		{
			if (animationTime < m_Scales[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}


private:

	float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
	{
		float scaleFactor = 0.0f;
		float midWayLength = animationTime - lastTimeStamp;
		float framesDiff = nextTimeStamp - lastTimeStamp;
		scaleFactor = midWayLength / framesDiff;
		return scaleFactor;
	}

	glm::mat4 InterpolatePosition(float animationTime)
	{
		if (1 == m_NumPositions)
			return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

		int p0Index = GetPositionIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Positions[p0Index].timeStamp,
			m_Positions[p1Index].timeStamp, animationTime);
		glm::vec3 finalPosition = glm::mix(m_Positions[p0Index].position, m_Positions[p1Index].position
			, scaleFactor);
		return glm::translate(glm::mat4(1.0f), finalPosition);
	}

	glm::mat4 InterpolateRotation(float animationTime)
	{
		if (1 == m_NumRotations)
		{
			auto rotation = glm::normalize(m_Rotations[0].orientation);
			return glm::mat4(rotation);
		}

		int p0Index = GetRotationIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Rotations[p0Index].timeStamp,
			m_Rotations[p1Index].timeStamp, animationTime);
		glm::quat finalRotation = glm::slerp(m_Rotations[p0Index].orientation, m_Rotations[p1Index].orientation
			, scaleFactor);
		finalRotation = glm::normalize(finalRotation);
		return glm::mat4(finalRotation);

	}

	glm::mat4 InterpolateScaling(float animationTime)
	{
		if (1 == m_NumScalings)
			return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

		int p0Index = GetScaleIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Scales[p0Index].timeStamp,
			m_Scales[p1Index].timeStamp, animationTime);
		glm::vec3 finalScale = glm::mix(m_Scales[p0Index].scale, m_Scales[p1Index].scale
			, scaleFactor);
		return glm::scale(glm::mat4(1.0f), finalScale);
	}

	std::vector<KeyPosition> m_Positions;
	std::vector<KeyRotation> m_Rotations;
	std::vector<KeyScale> m_Scales;
	int m_NumPositions;
	int m_NumRotations;
	int m_NumScalings;

	glm::mat4 m_LocalTransform;
	std::string m_Name;
	int m_ID;
};

struct AssimpNodeData
{
	glm::mat4 transformation;
	std::string name;
	int childrenCount;
	std::vector<AssimpNodeData> children;
};

class Animation
{
public:
	Animation() = default;

	Animation(const std::string& animationPath, Model* model, int animationNum)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
		assert(scene && scene->mRootNode);
		auto animation = scene->mAnimations[animationNum];
		m_Duration = animation->mDuration;
		m_TicksPerSecond = animation->mTicksPerSecond;
		aiMatrix4x4 globalTransformation = scene->mRootNode->mTransformation;
		globalTransformation = globalTransformation.Inverse();
		ReadHierarchyData(m_RootNode, scene->mRootNode);
		ReadMissingBones(animation, *model);
	}

	~Animation()
	{
	}

	Bone* FindBone(const std::string& name)
	{
		auto iter = std::find_if(m_Bones.begin(), m_Bones.end(),
			[&](const Bone& Bone)
			{
				return Bone.GetBoneName() == name;
			}
		);
		if (iter == m_Bones.end()) return nullptr;
		else return &(*iter);
	}


	inline float GetTicksPerSecond() { return m_TicksPerSecond; }
	inline float GetDuration() { return m_Duration; }
	inline const AssimpNodeData& GetRootNode() { return m_RootNode; }
	inline const std::unordered_map<std::string, BoneInfo>& GetBoneIDMap()
	{
		return m_BoneInfoMap;
	}

private:
	void ReadMissingBones(const aiAnimation* animation, Model& model)
	{
		int size = animation->mNumChannels;

		auto& boneInfoMap = model.GetBoneInfoMap();//getting m_BoneInfoMap from Model class
		int& boneCount = model.GetBoneCount(); //getting the m_BoneCounter from Model class

		//reading channels(bones engaged in an animation and their keyframes)
		for (int i = 0; i < size; i++)
		{
			auto channel = animation->mChannels[i];
			std::string boneName = channel->mNodeName.data;

			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				boneInfoMap[boneName].id = boneCount;
				boneCount++;
			}
			m_Bones.push_back(Bone(channel->mNodeName.data,
				boneInfoMap[channel->mNodeName.data].id, channel));
		}

		m_BoneInfoMap = boneInfoMap;
	}

	void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
	{
		assert(src);

		dest.name = src->mName.data;
		dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
		dest.childrenCount = src->mNumChildren;

		for (int i = 0; i < src->mNumChildren; i++)
		{
			AssimpNodeData newData;
			ReadHierarchyData(newData, src->mChildren[i]);
			dest.children.push_back(newData);
		}
	}
	float m_Duration;
	int m_TicksPerSecond;
	std::vector<Bone> m_Bones;
	AssimpNodeData m_RootNode;
	std::unordered_map<std::string, BoneInfo> m_BoneInfoMap;
};

class Animator
{
public:
	Animator(Animation* animation)
	{
		m_CurrentTime = 0.0;
		m_CurrentAnimation = animation;

		m_FinalBoneMatrices.reserve(100);

		for (int i = 0; i < 100; i++)
			m_FinalBoneMatrices.push_back(glm::mat4(1.0f));
	}

	void UpdateAnimation(float dt)
	{
		if (m_CurrentAnimation)
		{
			m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * dt;
			m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());
			CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
		}
	}

	void PlayAnimation(Animation* pAnimation)
	{
		m_CurrentAnimation = pAnimation;
		m_CurrentTime = 0.0f;
	}

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
	{
		std::string nodeName = node->name;
		glm::mat4 nodeTransform = node->transformation;

		Bone* Bone = m_CurrentAnimation->FindBone(nodeName);

		if (Bone)
		{
			Bone->Update(m_CurrentTime);
			nodeTransform = Bone->GetLocalTransform();
		}

		glm::mat4 globalTransformation = parentTransform * nodeTransform;

		auto boneInfoMap = m_CurrentAnimation->GetBoneIDMap();
		if (boneInfoMap.find(nodeName) != boneInfoMap.end())
		{
			int index = boneInfoMap[nodeName].id;
			glm::mat4 offset = boneInfoMap[nodeName].offset;
			m_FinalBoneMatrices[index] = globalTransformation * offset;
		}

		for (int i = 0; i < node->childrenCount; i++)
			CalculateBoneTransform(&node->children[i], globalTransformation);
	}

	std::vector<glm::mat4> GetFinalBoneMatrices()
	{
		return m_FinalBoneMatrices;
	}

private:
	std::vector<glm::mat4> m_FinalBoneMatrices;
	Animation* m_CurrentAnimation;
	float m_CurrentTime;

};

/*=================================================================================================
	OBJECTS
=================================================================================================*/

//VAO -> the object "as a whole", the collection of buffers that make up its data
//VBOs -> the individual buffers/arrays with data, for ex: one for coordinates, one for color, etc.

GLuint torus_VAO;
GLuint torus_VBO[4];

GLuint normal_VAO;
GLuint normal_VBO[2];

GLuint skybox_VAO;
GLuint skybox_VBO;

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
Animation *animation;
Animator *animator;

std::vector<rectangularPrism> floorTiles;

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

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

void checkCollision()
{
	bool collided = false;

	if (player_pos.y < -50.0f)
		player_pos = respawn_point;

	for (int i = 0; i < floorTiles.size(); i++)
	{
		//Check that player is within x-axis bounds of tile
		if (player_pos.x > floorTiles[i].minX() && player_pos.x < floorTiles[i].maxX())
		{
			//Check that player is within z-axis bounds of tile
			if (player_pos.z > floorTiles[i].minZ() && player_pos.z < floorTiles[i].maxZ())
			{
				//Check if player has fallen into or is standing on the tile
				if (player_pos.y > floorTiles[i].minY() && player_pos.y <= floorTiles[i].maxY())
				{
					player_pos.y = floorTiles[i].maxY();
					jump_displacement = 0.0f;
					fall_start = 0.0f;
					jumping = false;
					standing = true;
					collided = true;

					if (floorTiles[i].isCheckpoint)
					{
						float center_x = (floorTiles[i].maxX() + floorTiles[i].minX()) / 2.0f;
						float center_y = floorTiles[i].maxY();
						float center_z = (floorTiles[i].maxZ() + floorTiles[i].minZ()) / 2.0f;
						
						respawn_point = glm::vec3(center_x, center_y, center_z);
					}
					break;
				}
			}
		}
	}

	if (!collided)
	{
		standing = false;
		if (fall_start == 0.0f)
			fall_start = glfwGetTime();
	}
}

void GamepadInput()
{
	camera_direction_vector = glm::normalize(glm::cross(glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw))), up));

	if (axes[2] > 0.05 || axes[2] < -0.05)
		yaw += axes[2];
	if (axes[3] > 0.1 || axes[3] < -0.1)
		pitch += axes[3];

	if (pitch > 70.0)
		pitch = 70.0;
	if (pitch < 7.0)
		pitch = 7.0;

	if (axes[0] > 0.05 || axes[0] < -0.05)
	{
		direction_vector1 = axes[0] / 2.0f * camera_direction_vector;
		player_pos -= direction_vector1;
		if (animationNum != 8 && !jumping)
		{
			delete animation;
			delete animator;
			animationNum = 8;
			animation = new Animation("models/player.glb", player, animationNum);
			animator = new Animator(animation);
		}
	}
	if (axes[1] > 0.1 || axes[1] < -0.1)
	{
		direction_vector2 = axes[1] / 2.0f * glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw)));
		player_pos += direction_vector2;
		if (animationNum != 8 && !jumping)
		{
			delete animation;
			delete animator;
			animationNum = 8;
			animation = new Animation("models/player.glb", player, animationNum);
			animator = new Animator(animation);
		}
	}
	if (axes[0] < 0.05 && axes[0] > -0.05 && axes[1] < 0.1 && axes[1] > -0.1)
	{
		if (animationNum != 6 && !jumping)
		{
			delete animation;
			delete animator;
			animationNum = 6;
			animation = new Animation("models/player.glb", player, animationNum);
			animator = new Animator(animation);
		}
	}

	//Only allows jump if player is not already jumping and is standing on ground
	if (GLFW_PRESS == buttons[0] && !jumping && standing)
	{
		jump_start = glfwGetTime();
		jump_velocity = sqrt(gravity * jump_height) / 2.0f;
		jumping = true;
		standing = false;
		player_pos.y += 0.1f;

		delete animation;
		delete animator;
		animationNum = 7;
		animation = new Animation("models/player.glb", player, animationNum);
		animator = new Animator(animation);
	}

	float current_time = glfwGetTime();

	checkCollision();

	//Applies gravity if collision is not detected
	if (!standing)
	{
		if (jumping)
		{
			float elapsed_time = current_time - jump_start;
			jump_displacement = jump_velocity - 0.5f * gravity * elapsed_time * elapsed_time;
			//Limits fall speed
			if (jump_displacement < -3.0)
				jump_displacement = -3.0;
			player_pos.y += jump_displacement;
		}
		//Applies gravity when player falls off tile without jumping
		else
		{
			float elapsed_time = current_time - fall_start;
			jump_displacement =  -gravity * elapsed_time * elapsed_time;
			//Limits fall speed
			if (jump_displacement < -3.0)
				jump_displacement = -3.0;
			player_pos.y += jump_displacement;
		}
	}


	if(direction_vector1 != glm::vec3(0.0, 0.0, 0.0) || direction_vector2 != glm::vec3(0.0, 0.0, 0.0))
		player_direction_vector = (-direction_vector1 + direction_vector2) * glm::vec3(0.5, 0, 0.5);

	PerspModelMatrix *= glm::inverse(glm::lookAt(player_pos, player_pos - player_direction_vector, up));
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
	float currentFrame = glfwGetTime();
	if (currentFrame - lastFrame > 0.005)
	{
		lastFrame = currentFrame;
		glutPostRedisplay();
	}
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
			std::cout << respawn_point.x << ", " << respawn_point.y << ", " << respawn_point.z << std::endl;
			break;
		}

		case 'y':
		{
			std::cout << glfwGetTime() << std::endl;
			break;
		}

		case ' ':
		{
			player_pos = glm::vec3(0.0, 0.0, 0.0);
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
}

void passive_motion_func( int x, int y )
{
	if( x < 0 || x > WindowWidth || y < 0 || y > WindowHeight )
		return;

	float px, py;
	window_to_scene( x, y, px, py );
}

void active_motion_func( int x, int y )
{
	if( x < 0 || x > WindowWidth || y < 0 || y > WindowHeight )
		return;

	float px, py;
	window_to_scene( x, y, px, py );
}

void deletePointers()
{
	delete player;
	delete animation;
	delete animator;
	for (int i = 0; i < floorTiles.size(); i++)
		floorTiles[i].deleteMesh();
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

	float currentFrameAnim = glfwGetTime();
	deltaTime = currentFrameAnim - lastFrameAnim;
	lastFrameAnim = currentFrameAnim;
	//Pause animation while in mid-air
	if(glfwGetTime() - jump_start < 0.3 || !jumping)
		animator->UpdateAnimation(deltaTime);

	
	PerspectiveShader.Use();
	PerspectiveShader.SetUniform("projectionMatrix", glm::value_ptr(PerspProjectionMatrix), 4, GL_FALSE, 1);
	PerspectiveShader.SetUniform("viewMatrix", glm::value_ptr(PerspViewMatrix), 4, GL_FALSE, 1);
	PerspectiveShader.SetUniform("modelMatrix", glm::value_ptr(PerspModelMatrix), 4, GL_FALSE, 1);

	for (int i = 0; i < floorTiles.size(); i++) {
		floorTiles[i].Draw();
	}

	auto transforms = animator->GetFinalBoneMatrices();
	for (int i = 0; i < transforms.size(); i++)
	{
		std::string temp = "finalBonesMatrices[" + std::to_string(i) + "]";
		GLchar* finalBonesMatrices = (GLchar*)temp.c_str();
		PerspectiveShader.SetUniform(finalBonesMatrices, glm::value_ptr(transforms[i]), 4, GL_FALSE, 1);
	}
	
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

	//Create skybox buffers
	CreateSkyboxBuffers();

	//Load skybox textures
	CreateTextures();

	//Load controller axes and buttons
	if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
	{
		axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);
		buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &button_count);
	}

	//Create player model and neutral animation
	player = new Model("models/player.glb");
	animation = new Animation("models/player.glb", player, animationNum);
	animator = new Animator(animation);

	floorTiles.push_back(rectangularPrism(-15.5, -0.1, -15.5, 60, 2, 60, false, false));
	floorTiles.push_back(rectangularPrism(-15.5, -0.1, 100.5, 60, 2, 60, true, false));

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

	deletePointers();

	glfwTerminate();
	return EXIT_SUCCESS;
}
