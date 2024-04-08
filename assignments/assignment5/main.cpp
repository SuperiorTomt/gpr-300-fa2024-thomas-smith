#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>
#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/procGen.h>

#include <tslib/framebuffer.h>
#include <tslib/shadowbuffer.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

ew::Camera camera;
ew::Camera shadowCam;
ew::CameraController cameraController;
ew::Transform monkeyTransform;
ew::Transform planeTransform;

// Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;
int shadowMapResolution = 2048;

tslib::Shadowbuffer sb;
tslib::Framebuffer gb;

struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shininess = 128;
}material;

struct EdgeDetect {
	bool enabled = 0;
}edge;

struct Light {
	glm::vec3 lightDirection = glm::vec3(-1.0, -1.0, -1.0);
	glm::vec3 lightColor = glm::vec3(1);
}light;

struct PointLight {
	glm::vec3 position;
	float radius;
	glm::vec4 color;
};
const int MAX_POINT_LIGHTS = 64;
PointLight pointLights[MAX_POINT_LIGHTS];

struct KeyFrame {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
	float time;
};

float InvLerp(float prevTime, float nextTime, float currentTime) {
	float t = (currentTime - prevTime) / (nextTime - prevTime);

	return t;
}

glm::vec3 Lerp(glm::vec3 prevKey, glm::vec3 nextKey, float time) {
	glm::vec3 v = ((nextKey - prevKey) * time) + prevKey;

	return v;
}

glm::quat Slerp(glm::quat q1, glm::quat q2, float t) {
	float angle = acos(dot(q1, q2));
	float denom = sin(angle);

	if (denom == 0)
		return q1;

	return (q1 * sin((1 - t) * angle) + q2 * sin(t * angle)) / denom;
}

glm::mat4 CalcTransform(glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
	glm::mat4 m = glm::mat4(1.0f);
	m = glm::translate(m, position);
	m *= glm::mat4_cast(rotation);
	m = glm::scale(m, scale);

	return m;
}

struct AnimationClip {
	KeyFrame keyFrames[10];
	int numKeyFrames = 0;
	float localTime = 0.0f;
	float duration;

	glm::mat4 Update(float dt) {
		localTime += dt;

		if (localTime > duration)
			localTime = 0.0f;

		glm::vec3 newPos = glm::vec3(0);
		glm::quat newRot = glm::quat(1, 0, 0, 0);
		glm::vec3 newScale = glm::vec3(1);

		for (int i = 0; i < numKeyFrames; i++) {
			if (keyFrames[i].time > localTime) {
				float t = InvLerp(keyFrames[i - 1].time, keyFrames[i].time, localTime);
				newPos = Lerp(keyFrames[i - 1].position, keyFrames[i].position, t);
				newRot = Slerp(keyFrames[i - 1].rotation, keyFrames[i].rotation, t);
				newScale = Lerp(keyFrames[i - 1].scale, keyFrames[i].scale, t);

				break;
			}
		}

		return CalcTransform(newPos, newRot, newScale);
	}
};

void AddFrameToAnim(AnimationClip* anim, float time, glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
	KeyFrame newFrame;
	newFrame.time = time;
	newFrame.position = position;
	newFrame.rotation = rotation;
	newFrame.scale = scale;

	if (anim->duration < time)
		anim->duration = time;
	anim->keyFrames[anim->numKeyFrames] = newFrame;
	anim->numKeyFrames++;
}

struct Node {
	glm::mat4 localTransform;
	glm::mat4 globalTransform;
	Node* parent;
	Node* children[10];
	unsigned int numChildren;

	AnimationClip animation;

	void Update(float dt) {
		if (animation.numKeyFrames != 0)
			localTransform = animation.Update(dt);
	}
};

void SolveFKRecursive(Node* node) {
	if (node->parent == NULL)
		node->globalTransform = node->localTransform;
	else
		node->globalTransform = node->parent->globalTransform * node->localTransform;
	for (int i = 0; i < node->numChildren; i++) {
		SolveFKRecursive(node->children[i]);
	}
}

Node* AddNode(Node* parent, AnimationClip anim) {
	Node* newNode = new Node;

	newNode->parent = parent;
	newNode->numChildren = 0;
	newNode->animation = anim;
	newNode->localTransform = CalcTransform(anim.keyFrames[0].position, anim.keyFrames[0].rotation, anim.keyFrames[0].scale);

	if (parent != NULL) {
		parent->children[parent->numChildren] = newNode;
		parent->numChildren++;
	}

	return newNode;
}

Node* AddNode(Node* parent, glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
	Node* newNode = new Node;

	newNode->parent = parent;
	newNode->localTransform = CalcTransform(position, rotation, scale);
	newNode->numChildren = 0;

	if (parent != NULL) {
		parent->children[parent->numChildren] = newNode;
		parent->numChildren++;
	}

	return newNode;
}

//void CalcLocalTransformsRecursive(Node* node) {
//	glm::mat4 m = glm::mat4(1.0f);
//	m = glm::translate(m, node->localPosition);
//	m *= glm::mat4_cast(node->localRotation);
//	m = glm::scale(m, node->localScale);
//	node->localTransform = m;
//
//	for (int i = 0; i < node->numChildren; i++) {
//		CalcLocalTransformsRecursive(node->children[i]);
//	}
//}

void UpdateAnimsRecursive(Node* node, float dt) {
	node->Update(dt);

	for (int i = 0; i < node->numChildren; i++)
		UpdateAnimsRecursive(node->children[i], dt);
}

void DrawNodesRecursive(ew::Shader shader, ew::Model model, Node* node) {
	shader.setInt("_MainTex", 0);
	shader.setMat4("_Model", node->globalTransform);
	model.draw();

	for (int i = 0; i < node->numChildren; i++)
		DrawNodesRecursive(shader, model, node->children[i]);
}

void ClearNodesRecursive(Node* node) {
	for (int i = 0; i < node->numChildren; i++)
		ClearNodesRecursive(node->children[i]);

	delete(node);
}

int main() {
	GLFWwindow* window = initWindow("Assignment 5", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Init shaders and model
	ew::Shader deferredShader = ew::Shader("assets/deferredLit.vert", "assets/deferredLit.frag");
	ew::Shader convolutionShader = ew::Shader("assets/edge.vert", "assets/edge.frag");
	ew::Shader depthShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader gBufferShader = ew::Shader("assets/lit.vert", "assets/geometryPass.frag");
	ew::Shader lightOrbShader = ew::Shader("assets/lightOrb.vert", "assets/lightOrb.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));
	ew::Mesh sphereMesh = ew::Mesh(ew::createSphere(1.0f, 8));

	planeTransform.position = glm::vec3(0, -2.0, 0);

	// Setup Camera
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;

	// Create Framebuffer
	tslib::Framebuffer fb = tslib::createFramebuffer(screenWidth, screenHeight, GL_RGBA16);

	// Check for complete framebuffer
	GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer incomplete: %d", fboStatus);
	}

	// Init Point Lights
	/*for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			pointLights[i * 8 + j].color = glm::vec4(rand() % 4, rand() % 4, rand() % 4, 1);
			pointLights[i * 8 + j].radius = 6.0f;
			pointLights[i * 8 + j].position = glm::vec3((i * 5) + 2.5f, -0.5, (j * 5) + 2.5f);
		}
	}*/

	// Create Shadowbuffer
	sb = tslib::createShadowbuffer(shadowMapResolution);

	// Setup Shadow Camera
	shadowCam.target = planeTransform.position;
	shadowCam.position = shadowCam.target - light.lightDirection * 15.0f;
	shadowCam.orthographic = true;
	shadowCam.orthoHeight = 50.0;
	shadowCam.nearPlane = 0.01f;
	shadowCam.farPlane = 50.0f;
	shadowCam.aspectRatio = 1.0;

	// Create GBuffer
	gb = tslib::createGBuffer(screenWidth, screenHeight);

	// Create Dummy VAO
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	// Load Textures
	GLuint monkeyTexture = ew::loadTexture("assets/cork.jpg");
	GLuint groundTexture = ew::loadTexture("assets/cormn.png");

	GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	// Setup Mech
	AnimationClip torsoAnim;
	AddFrameToAnim(&torsoAnim, 0.0f, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1));
	AddFrameToAnim(&torsoAnim, 1.0f, glm::vec3(0, 2, 0), glm::quat(1, 0, 0, 0), glm::vec3(1));
	AddFrameToAnim(&torsoAnim, 5.0f, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1));
	Node* torso = AddNode(NULL, torsoAnim);

	AnimationClip propellorBaseAnim;
	AddFrameToAnim(&propellorBaseAnim, 0.0f, glm::vec3(0, 1.3, 0), glm::quat(1, 0, 0, 0), glm::vec3(0.5));
	AddFrameToAnim(&propellorBaseAnim, 0.3f, glm::vec3(0, 1.3, 0), glm::quat(0, 0, 1, 0), glm::vec3(0.5));
	AddFrameToAnim(&propellorBaseAnim, 0.6f, glm::vec3(0, 1.3, 0), glm::quat(-1, 0, 0, 0), glm::vec3(0.5));
	Node* propellorBase = AddNode(torso, propellorBaseAnim);

	Node* propellorArmL1 = AddNode(propellorBase, glm::vec3(2, 0, 0), glm::quat(0, 0, 1, 0), glm::vec3(.7));
	Node* propellorArmL2 = AddNode(propellorBase, glm::vec3(4, 0, 0), glm::quat(0, 0, 1, 0), glm::vec3(.7));
	Node* propellorArmL3 = AddNode(propellorBase, glm::vec3(6, 0, 0), glm::quat(0, 0, 1, 0), glm::vec3(.7));
	Node* propellorArmR1 = AddNode(propellorBase, glm::vec3(-2, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(.7));
	Node* propellorArmR2 = AddNode(propellorBase, glm::vec3(-4, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(.7));
	Node* propellorArmR3 = AddNode(propellorBase, glm::vec3(-6, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(.7));

	AnimationClip hipAnimL;
	AddFrameToAnim(&hipAnimL, 0.0f, glm::vec3(0.8, -0.8, 0.5), glm::quat(0.924, -0.383, 0, 0), glm::vec3(0.5));
	AddFrameToAnim(&hipAnimL, 0.4f, glm::vec3(0.8, -0.8, 0.5), glm::quat(0.924, 0.383, 0, 0), glm::vec3(0.5));
	AddFrameToAnim(&hipAnimL, 0.8f, glm::vec3(0.8, -0.8, 0.5), glm::quat(0.924, -0.383, 0, 0), glm::vec3(0.5));
	Node* hipL = AddNode(torso, hipAnimL);

	AnimationClip hipAnimR;
	AddFrameToAnim(&hipAnimR, 0.0f, glm::vec3(-0.8, -0.8, 0.5), glm::quat(0.924, 0.383, 0, 0), glm::vec3(0.5));
	AddFrameToAnim(&hipAnimR, 0.4f, glm::vec3(-0.8, -0.8, 0.5), glm::quat(0.924, -0.383, 0, 0), glm::vec3(0.5));
	AddFrameToAnim(&hipAnimR, 0.8f, glm::vec3(-0.8, -0.8, 0.5), glm::quat(0.924, 0.383, 0, 0), glm::vec3(0.5));
	Node* hipR = AddNode(torso, hipAnimR);

	Node* kneeL = AddNode(hipL, glm::vec3(0, -0.8, 0.5), glm::quat(1, 0, 0, 0), glm::vec3(0.5));
	Node* kneeR = AddNode(hipR, glm::vec3(0, -0.8, 0.5), glm::quat(1, 0, 0, 0), glm::vec3(0.5));

	AnimationClip ankleAnim;
	AddFrameToAnim(&ankleAnim, 0.0f, glm::vec3(0, -1.3, 0.5), glm::quat(1, 0, 0, 0), glm::vec3(1));
	AddFrameToAnim(&ankleAnim, 0.3f, glm::vec3(0, -1.3, 0.5), glm::quat(0, 0, 1, 0), glm::vec3(1));
	AddFrameToAnim(&ankleAnim, 0.3f, glm::vec3(0, -1.3, 0.5), glm::quat(-1, 0, 0, 0), glm::vec3(1));
	Node* ankleL = AddNode(kneeL, ankleAnim);
	Node* ankleR = AddNode(kneeR, ankleAnim);

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &camera, deltaTime);
		shadowCam.position = shadowCam.target - light.lightDirection * 15.0f;

		// Update Animations and Solve Transforms
		UpdateAnimsRecursive(torso, deltaTime);
		SolveFKRecursive(torso);

		// Render Shadow Map
		glBindFramebuffer(GL_FRAMEBUFFER, sb.fbo);
		glViewport(0, 0, sb.resolution, sb.resolution);
		glClear(GL_DEPTH_BUFFER_BIT);

		depthShader.use();
		depthShader.setMat4("_ViewProjection", shadowCam.projectionMatrix() * shadowCam.viewMatrix());

		DrawNodesRecursive(depthShader, monkeyModel, torso);

		// Bind textures
		glBindTextureUnit(0, monkeyTexture);
		glBindTextureUnit(1, groundTexture);
		glBindTextureUnit(2, sb.shadowMap);

		// Render to G-Buffer
		glBindFramebuffer(GL_FRAMEBUFFER, gb.fbo);
		glViewport(0, 0, gb.width, gb.height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		gBufferShader.use();
		gBufferShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

		DrawNodesRecursive(gBufferShader, monkeyModel, torso);

		gBufferShader.setInt("_MainTex", 1);
		gBufferShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();

		// Bind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
		glViewport(0, 0, fb.width, fb.height);
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);

		// Shader Setup
		deferredShader.use();

		deferredShader.setInt("_ShadowMap", 3);
		deferredShader.setVec3("_EyePos", camera.position);
		deferredShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		deferredShader.setMat4("_LightViewProj", shadowCam.projectionMatrix() * shadowCam.viewMatrix());
		deferredShader.setVec3("_LightDirection", light.lightDirection);
		deferredShader.setVec3("_LightColor", light.lightColor);

		deferredShader.setFloat("_Material.Ka", material.Ka);
		deferredShader.setFloat("_Material.Kd", material.Kd);
		deferredShader.setFloat("_Material.Ks", material.Ks);
		deferredShader.setFloat("_Material.Shininess", material.Shininess);

		for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
			std::string prefix = "_PointLights[" + std::to_string(i) + "].";
			deferredShader.setVec3(prefix + "position", pointLights[i].position);
			deferredShader.setFloat(prefix + "radius", pointLights[i].radius);
			deferredShader.setVec4(prefix + "color", pointLights[i].color);
		}

		glBindTextureUnit(0, gb.colorBuffers[0]);
		glBindTextureUnit(1, gb.colorBuffers[1]);
		glBindTextureUnit(2, gb.colorBuffers[2]);
		glBindTextureUnit(3, sb.shadowMap);
		
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// Draw Orbs
		/*glBindFramebuffer(GL_READ_FRAMEBUFFER, gb.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fbo);
		glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		lightOrbShader.use();
		lightOrbShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		for (int i = 0; i < MAX_POINT_LIGHTS; i++)
		{
			glm::mat4 m = glm::mat4(1.0f);
			m = glm::translate(m, pointLights[i].position);
			m = glm::scale(m, glm::vec3(0.3f));

			lightOrbShader.setMat4("_Model", m);
			lightOrbShader.setVec3("_Color", pointLights[i].color);
			sphereMesh.draw();
		}*/

		// Bind
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		convolutionShader.use();
		convolutionShader.setInt("_Enabled", edge.enabled);

		glBindTextureUnit(0, fb.colorBuffers[0]);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		drawUI();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glad_glDeleteFramebuffers(1, &fb.fbo);
	glad_glDeleteFramebuffers(1, &sb.fbo);

	ClearNodesRecursive(torso);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	if (ImGui::Button("Reset Camera")) {
		resetCamera(&camera, &cameraController);
	}

	if (ImGui::CollapsingHeader("Material")) {
		ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}

	if (ImGui::CollapsingHeader("Light Direction"))
	{
		ImGui::SliderFloat("X", &light.lightDirection.x, -2.0f, 2.0f);
		ImGui::SliderFloat("Y", &light.lightDirection.y, -2.0f, 2.0f);
		ImGui::SliderFloat("Z", &light.lightDirection.z, -2.0f, 2.0f);
	}

	if (ImGui::Button("Toggle Edge Detect")) {
		edge.enabled = !edge.enabled;
	}

	ImGui::End();

	ImGui::Begin("Shadow Map"); {
		ImGui::BeginChild("Shadow Map");

		ImVec2 windowSize = ImGui::GetWindowSize();

		ImGui::Image((ImTextureID)sb.shadowMap, windowSize, ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndChild();
		ImGui::End();
	}

	ImGui::Begin("GBuffers"); {
		ImVec2 texSize = ImVec2(gb.width / 4, gb.height / 4);
		for (size_t i = 0; i < 3; i++)
		{
			ImGui::Image((ImTextureID)gb.colorBuffers[i], texSize, ImVec2(0, 1), ImVec2(1, 0));
		}
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}
