#version 450
out vec4 FragColor;
in vec2 UV;

uniform sampler2D _ColorBuffer;
uniform int _Enabled;
uniform int _Intensity;

#define edgeKernel mat3(-1, -1, -1, -1, 8, -1, -1, -1, -1)

void main() {
	if (_Enabled == 0) {
		vec3 color = texture(_ColorBuffer, UV).rgb;
		FragColor = vec4(color, 1.0);
	} else {
		vec2 texelSize = 1.0 / textureSize(_ColorBuffer, 0).xy;
		vec3 totalColor = vec3(0);
		for (int x = 0; x < 3; x++)
		{
			for (int y = 0; y < 3; y++)
			{
				vec2 offset = vec2(x, y) * texelSize;
				totalColor += texture(_ColorBuffer, UV + offset).rgb * edgeKernel[x][y];
			}
		}

		FragColor = vec4(totalColor,1.0);
	}
}
