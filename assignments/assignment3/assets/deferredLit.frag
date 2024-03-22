#version 450
out vec4 FragColor; //The color of this fragment

in vec2 UV;

uniform sampler2D _MainTex;
uniform sampler2D _ShadowMap;
uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);
uniform mat4 _LightViewProj;

struct PointLight{
	vec3 position;
	float radius;
	vec4 color;
};
#define MAX_POINT_LIGHTS 64
uniform PointLight _PointLights[MAX_POINT_LIGHTS];


struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;

uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;

float attenuateExponential(float distance, float radius) {
	float i = clamp(1.0 - pow(distance/radius,4.0),0.0,1.0);
	return i * i;
}

vec3 calcPointLight(PointLight light,vec3 normal,vec3 pos){
	vec3 diff = light.position - pos;

	//Direction toward light position
	vec3 toLight = normalize(diff);

	// Blinn-Phong calculations
	float diffuseFactor = max(dot(normal, toLight),0.0);

	vec3 worldPos = texture(_gPositions, UV).xyz;
	vec3 toEye = normalize(_EyePos - worldPos);
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	vec3 lightColor = (diffuseFactor + specularFactor) * vec3(light.color);

	// Attenuation
	float d = length(diff); //Distance to light
	lightColor *= attenuateExponential(d, light.radius);
	return lightColor;
}


float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, vec3 normal){
	// calculate slope scale bias
	float minBias = 0.005; 
	float maxBias = 0.015;
	float bias = max(maxBias * (1.0 - dot(normal, -_LightDirection)),minBias);

	//Homogeneous Clip space to NDC [-w,w] to [-1,1]
    vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
    //Convert from [-1,1] to [0,1]
    sampleCoord = sampleCoord * 0.5 + 0.5;
	float myDepth = sampleCoord.z - bias; 
	float shadowMapDepth = texture(shadowMap, sampleCoord.xy).r;

	// PCF filtering
	float totalShadow = 0;

	vec2 texelOffset = 1.0 /  textureSize(_ShadowMap,0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow += step(shadowMapDepth, myDepth);
		}
	}

	totalShadow /= 9.0;

	return totalShadow;
}

vec3 calculateLighting(vec3 normal, vec3 worldPos, vec4 lightSpacePos) {
	vec3 toLight = -_LightDirection;
	float diffuseFactor = max(dot(normal, toLight),0.0);
	vec3 toEye = normalize(_EyePos - worldPos);
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	vec3 diffuse = _Material.Kd * diffuseFactor * _LightColor;
	vec3 specular = _Material.Ks * specularFactor * _LightColor;
	vec3 ambient = _Material.Ka * _AmbientColor;

	float shadow = calcShadow(_ShadowMap, lightSpacePos, normal);
	vec3 light = ambient + (diffuse + specular) * (1.0 - shadow);

	return light;
}

void main(){
	vec3 normal = texture(_gNormals, UV).xyz;
	vec3 pos = texture(_gPositions, UV).xyz;
	vec3 albedo = texture(_gAlbedo, UV).xyz;
	vec4 lightSpacePos = _LightViewProj * vec4(pos, 1);

	vec3 totalLight = vec3(0);
	totalLight += calculateLighting(normal, pos, lightSpacePos);
	for (int i=0; i < MAX_POINT_LIGHTS; i++) {
		totalLight += calcPointLight(_PointLights[i], normal, pos);
	}

	FragColor = vec4(albedo * totalLight, 0);
}
