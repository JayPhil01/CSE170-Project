#version 400

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;
layout(location=2) in vec2 in_TexCoord;
layout(location=3) in ivec4 boneIds;
layout(location=4) in vec4 weights;

out vec4 vert_Pos;
out vec4 vert_Normal;
out vec2 vert_TexCoord;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main( void )
{
	vec4 totalPosition = vec4(0.0f);
	for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		if(boneIds[i] == -1)
			continue;
		if(boneIds[i] >= MAX_BONES)
		{
			totalPosition = vec4(in_Position, 1.0f);
			break;
		}
		vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(in_Position, 1.0f);
		totalPosition += localPosition * weights[i];
	}

	gl_Position = projectionMatrix * viewMatrix * modelMatrix * totalPosition;
	
	vert_Pos      = totalPosition;
	vert_Normal   = vec4(in_Normal, 1.0f);
	vert_TexCoord = in_TexCoord;
}
