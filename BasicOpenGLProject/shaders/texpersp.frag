#version 400

in vec4 vert_Color;
in vec2 vert_TexCoord;

out vec4 frag_Color;

uniform sampler2D texId;

void main( void )
{
	frag_Color = vert_Color * texture( texId, vert_TexCoord );
}
