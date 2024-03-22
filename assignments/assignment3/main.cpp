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

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Init shaders and model
	ew::Shader deferredShader = ew::Shader("assets/deferredLit.vert", "assets/deferredLit.frag");
	ew::Shader convolutionShader = ew::Shader("assets/edge.vert", "assets/edge.frag");
	ew::Shader depthShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader gBufferShader = ew::Shader("assets/lit.vert", "assets/geometryPass.frag");
	ew::Shader lightOrbShader = ew::Shader("assets/lightOrb.vert", "assets/lightOrb.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(40, 40, 5));
	ew::Mesh sphereMesh = ew::Mesh(ew::createSphere(1.0f, 8));

	planeTransform.position = glm::vec3(17.5, -1.0, 17.5);

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
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			pointLights[i * 8 + j].color = glm::vec4(rand() % 4, rand() % 4, rand() % 4, 1);
			pointLights[i * 8 + j].radius = 6.0f;
			pointLights[i * 8 + j].position = glm::vec3((i * 5) + 2.5f, -0.5, (j * 5) + 2.5f);
		}
	}

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
	GLuint groundTexture = ew::loadTexture("assets/brick_color.jpg");

	GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		cameraController.move(window, &camera, deltaTime);
		shadowCam.position = shadowCam.target - light.lightDirection * 15.0f;

		// Rotate model around Y axis
		monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));

		// Render Shadow Map
		glBindFramebuffer(GL_FRAMEBUFFER, sb.fbo);
		glViewport(0, 0, sb.resolution, sb.resolution);
		glClear(GL_DEPTH_BUFFER_BIT);

		depthShader.use();
		depthShader.setMat4("_ViewProjection", shadowCam.projectionMatrix() * shadowCam.viewMatrix());

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				monkeyTransform.position = glm::vec3(i * 5, 0, j * 5);
				depthShader.setInt("_MainTex", 0);
				depthShader.setMat4("_Model", monkeyTransform.modelMatrix());
				monkeyModel.draw();
			}
		}

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

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				monkeyTransform.position = glm::vec3(i * 5, 0, j * 5);
				gBufferShader.setInt("_MainTex", 0);
				gBufferShader.setMat4("_Model", monkeyTransform.modelMatrix());
				monkeyModel.draw();
			}
		}

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
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gb.fbo);
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
		}

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
