#version 450

// Vertex attributes
layout(location = 0) in vec3 vPos; // Vertex position in model space
layout(location = 1) in vec3 vNormal; // Vertex position in model space
layout(location = 2) in vec2 vTexCoord; // Vertex texture coordinate (UV)

uniform mat4 _Model; // Model->World Matrix
uniform mat4 _ViewProjection; // Combined View->Projection Matrix
uniform mat4 _LightViewProj; //view + projection of light source camera

out Surface{
	vec3 WorldPos; // Vertex position in world space
	vec3 WorldNormal; // Vertex normal in world space
	vec2 TexCoord;
	vec4 LightSpacePos;
}vs_out;

void main(){
	vs_out.WorldPos = vec3(_Model * vec4(vPos,1.0));
	vs_out.WorldNormal = transpose(inverse(mat3(_Model))) * vNormal;
	vs_out.TexCoord = vTexCoord;
	vs_out.LightSpacePos = _LightViewProj * _Model * vec4(vPos, 1.0);
	gl_Position = _ViewProjection * _Model * vec4(vPos, 1.0);
}
