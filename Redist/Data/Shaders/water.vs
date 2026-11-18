#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform float time;

out vec2 fragTexCoord;
out vec4 fragColor;
out float fragShade;

// Rest height is stored in the mesh. These waves stay above the sea floor (y = 0).
float WaveHeight(vec2 xz, float t)
{
	return sin(xz.x * 0.65 + t * 1.7) * 0.050
		+ sin(xz.y * 0.85 - t * 1.3) * 0.035
		+ sin((xz.x + xz.y) * 0.40 + t * 0.9) * 0.025;
}

void main()
{
	vec2 xz = vertexPosition.xz;
	vec3 pos = vertexPosition;
	pos.y += WaveHeight(xz, time);

	float dx = cos(xz.x * 0.65 + time * 1.7) * 0.65 * 0.050
		+ cos((xz.x + xz.y) * 0.40 + time * 0.9) * 0.40 * 0.025;
	float dz = cos(xz.y * 0.85 - time * 1.3) * 0.85 * 0.035
		+ cos((xz.x + xz.y) * 0.40 + time * 0.9) * 0.40 * 0.025;
	fragShade = clamp(0.78 + (dx + dz) * 5.0, 0.62, 1.18);

	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor;
	gl_Position = mvp * vec4(pos, 1.0);
}
