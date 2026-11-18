#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in float fragShade;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;

out vec4 finalColor;

void main()
{
	// +X is east and +Z is south, so adding to both axes slides the image northwest.
	vec2 uv = fragTexCoord + vec2(time * 0.22, time * 0.22);
	vec4 texelColor = texture(texture0, uv);
	finalColor = texelColor * fragColor * colDiffuse * vec4(vec3(fragShade), 1.0);
}
