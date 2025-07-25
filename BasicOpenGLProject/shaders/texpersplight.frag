#version 400

in vec4 vert_Pos;
in vec4 vert_Normal;
in vec2 vert_TexCoord;

out vec4 frag_Color;

uniform sampler2D texId;

uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

vec4 shade( vec4 color )
{
	vec4 la = vec4( 0.7, 0.7, 0.7, 1.0 );
	vec4 ld = vec4( 0.7, 0.7, 0.7, 1.0 );
	vec4 ls = vec4( 1.0, 1.0, 1.0, 1.0 );

	vec4 ka = color;
	vec4 kd = color;
	vec4 ks = vec4( 1.0, 1.0, 1.0, 1.0 );

	float shininess = 32.0f;

	mat4 transf = viewMatrix * modelMatrix;

	vec3 FragPos  = vec3( transf * vert_Pos );
	vec3 FragNorm = mat3( transpose( inverse( transf ) ) ) * vert_Normal.xyz;
	vec3 LightPos = vec3( transf * vec4( 3.0, 0.0, 3.0, 1.0 ) );

	vec3 N = normalize( FragNorm ); // vertex normal
	vec3 L = normalize( LightPos - FragPos ); // light direction
	vec3 R = normalize( reflect( -L, N ) ); // reflected ray
	vec3 V = normalize( vec3( 0.0, 0.0, 1.0 ) ); // view direction

	float dotLN = dot( L, N );
	vec4 amb = ka * la;
	vec4 dif = kd * ld * max( dotLN, 0.0 );
	vec4 spe = ks * ls * pow( max( dot( V, R ), 0.0 ), shininess ) * max( dotLN, 0.0 );

	return amb + dif + spe;
}

void main(void)
{
	vec4 color = texture( texId, vert_TexCoord );
	frag_Color = shade( color );
}
