#version 450

// Vertex attributes
layout(location = 0) in vec3 vPos; // Vertex position in model space
layout(location = 1) in vec3 vNormal; // Vertex position in model space
layout(location = 2) in vec2 vTexCoord; // Vertex texture coordinate (UV)

out vec2 UV;

vec4 vertices[3] = {
	vec4(-1,-1,0,0), //Bottom left (X,Y,U,V)
	vec4(3,-1,2,0),  //Bottom right (X,Y,U,V)
	vec4(-1,3,0,2)   //Top left (X,Y,U,V)
};

void main(){
	UV = vertices[gl_VertexID].zw;
	gl_Position = vec4(vertices[gl_VertexID].xy,0,1);
}
