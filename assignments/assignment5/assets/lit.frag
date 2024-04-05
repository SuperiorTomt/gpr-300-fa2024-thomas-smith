#version 450
out vec4 FragColor; //The color of this fragment
in Surface{
	vec3 WorldPos; //Vertex position in world space
	vec3 WorldNormal; //Vertex normal in world space
	vec2 TexCoord;
	vec4 LightSpacePos;
}fs_in;

uniform sampler2D _MainTex; 
uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);
uniform sampler2D _ShadowMap;

uniform float _MinBias;
uniform float _MaxBias;

struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos){
	// calculate slope scale bias
	float minBias = 0.005; 
	float maxBias = 0.015;
	float bias = max(maxBias * (1.0 - dot(fs_in.WorldNormal, -_LightDirection)),minBias);

	//Homogeneous Clip space to NDC [-w,w] to [-1,1]
    vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
    //Convert from [-1,1] to [0,1]
    sampleCoord = sampleCoord * 0.5 + 0.5;
	float myDepth = sampleCoord.z - bias; 
	float shadowMapDepth = texture(shadowMap, sampleCoord.xy).r;

	// PCF filtering
	float totalShadow = 0;

	vec2 texelOffset = 1.0 /  textureSize(_ShadowMap,0);
	for (int y = -1; y <=1; y++) {
		for (int x = -1; x <=1; x++) {
			vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow += step(shadowMapDepth, myDepth);
		}
	}

	totalShadow /= 9.0;

	return totalShadow;
}


void main(){
	//Make sure fragment normal is still length 1 after interpolation.
	vec3 normal = normalize(fs_in.WorldNormal);
	//Light pointing straight down
	vec3 toLight = -_LightDirection;
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);

	vec3 diffuse = _Material.Kd * diffuseFactor * _LightColor;
	vec3 specular = _Material.Ks * specularFactor * _LightColor;
	vec3 ambient = _Material.Ka * _AmbientColor;

	float shadow = calcShadow(_ShadowMap, fs_in.LightSpacePos);
	vec3 light = ambient + (diffuse + specular) * (1.0 - shadow);

	vec3 objectColor = texture(_MainTex,fs_in.TexCoord).rgb;
	FragColor = vec4(objectColor * light,1.0);
}
