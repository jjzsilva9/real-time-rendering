#pragma warning(disable : 5208)

// Standard library includes
#include <string>
#include <vector>
#include <iostream>
#include <limits>
#include <math.h>

namespace std {
  using ::sqrt;
    using ::sin;
    using ::acos;
}

// Windows specific
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

// OpenGL
#include <GL/glew.h>
#include <GL/freeglut.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Project includes
#include "shader.h"
#include "model.h"
#include "directionallight.h"
#include "gbuffer.h"
#include "svo_builder.h"
#include "svo.h"

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl3.h"

#define CAMERASPEED 50.0f


typedef struct
{
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
} Camera;
Camera camera;

using namespace std;

int width = 800;
int height = 600;

// Root of the Hierarchy
glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -10.0f));
glm::mat4 persp_proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

float delta;
float yaw, pitch;
int lastX = width / 2;
int lastY = height / 2;
bool firstMouse = true;

Shader* shader = nullptr;
Shader* gShader = nullptr;
Shader* rectShader = nullptr;
Shader* svoShader = nullptr;
GBuffer* gBuffer = nullptr;
DirectionalLight* lightSource = nullptr;
Model* sponza = nullptr;
SVO* svo = nullptr;

GLuint quadVAO = 0, quadVBO;
int displayMode = 0; // 0: Combined, 1: Pos, 2: Norm, 3: Albedo, 4: SVO Raycast

bool showGUI = false;

float normal = 1.0f;  // normal map intensity

float lightPos[3] = { 0.0f, 8.0f, 0.0f };

// Forward declaration - defined in model.cpp
unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);

#pragma region INPUT_FUNCTIONS

static bool g_keyPressed = false;

void keypress(unsigned char key, int x, int y) {
	ImGui_ImplGLUT_KeyboardFunc(key, x, y);
	
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();
	
	if (key == 'g') {
		showGUI = !showGUI;
		if (showGUI) {
			glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
		}
		else {
			glutSetCursor(GLUT_CURSOR_NONE);
		}
		return;
	}

	if (io.WantCaptureKeyboard) {
		return;
	}

	// WASD camera movement
	float speed = CAMERASPEED * delta;
	glm::vec3 right = glm::normalize(glm::cross(camera.direction, glm::vec3(0.0f, 1.0f, 0.0f)));
	if (key == 'w') camera.position += camera.direction * speed;
	if (key == 's') camera.position -= camera.direction * speed;
	if (key == 'a') camera.position -= right * speed;
	if (key == 'd') camera.position += right * speed;
	if (key == 'q') camera.position.y -= speed;
	if (key == 'e') camera.position.y += speed;

	// ESC Key to quit
	if (key == 27) {
		glutLeaveMainLoop();
	}
}

void mouse(int x, int y) {
	ImGui_ImplGLUT_MotionFunc(x, y);
	
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();
	
	if (io.WantCaptureMouse || showGUI) {
		return;
	}

	float sensitivity = 0.1f;
	if (firstMouse) { lastX = x; lastY = y; firstMouse = false; }
	yaw   += (x - lastX) * sensitivity;
	pitch += (lastY - y) * sensitivity;
	pitch  = glm::clamp(pitch, -89.0f, 89.0f);
	lastX = x; lastY = y;

	glm::vec3 dir;
	dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	dir.y = sin(glm::radians(pitch));
	dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camera.direction = glm::normalize(dir);
}

void mouseClick(int button, int state, int x, int y) {
	ImGui_ImplGLUT_MouseFunc(button, state, x, y);
	
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();
	
	if (io.WantCaptureMouse || showGUI) {
		return;
	}
}

void mouseDrag(int x, int y) {
	ImGui_ImplGLUT_MotionFunc(x, y);
	
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureMouse || showGUI) {
		return;
	}
}

void reshape(int x, int y) {
	width = x;
	height = y;
	glViewport(0, 0, x, y);
	persp_proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

	if (gBuffer) {
		delete gBuffer;
		gBuffer = new GBuffer(width, height);
	}

	ImGui_ImplGLUT_ReshapeFunc(x, y);
}

#pragma endregion INPUT_FUNCTIONS


void renderGUI() {
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)width, (float)height);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGLUT_NewFrame();
	ImGui::NewFrame();

	if (showGUI) {
		ImGui::Begin("Controls", &showGUI, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("Camera: (%.1f, %.1f, %.1f)", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Separator();
		ImGui::Combo("Display Mode", &displayMode, "Final\0Position\0Normal\0Albedo\0SVO Raycast\0\0");
		if (ImGui::Button("Rebuild SVO")) {
			if (sponza && svo) SVOBuilder::build(sponza, 256, *svo);
			if (svo) svo->initGPU();
		}
		ImGui::DragFloat("Normal Map", &normal, 0.1f, 0.0f, 10.0f);
		ImGui::DragFloat3("Light Position", lightPos, 0.1f);
		if (sponza) {
			static float s = 0.05f;
			if (ImGui::DragFloat("Sponza Scale", &s, 0.001f, 0.001f, 1.0f)) {
				sponza->model = glm::scale(glm::mat4(1.0f), glm::vec3(s));
			}
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void renderQuad() {
	if (quadVAO == 0) {
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void display() {
	if (displayMode == 0) {
		// --- FINAL MODE: Normal Forward Rendering ---
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader->use();
		shader->setMat4("proj",  persp_proj);
		shader->setMat4("view",  view);
		shader->setMat4("model", sponza->model); // Explicitly set model matrix
		shader->setFloat("normalMapIntensity", normal);
		// shader expects vec4 for LightPosition
		glUniform4f(glGetUniformLocation(shader->ID, "LightPosition"), lightPos[0], lightPos[1], lightPos[2], 1.0f);

		if (sponza) sponza->Draw();
	} else if (displayMode >= 1 && displayMode <= 4) {
		// --- G-BUFFER & SVO MODES (Require Geometry Pass) ---

		// 1. Geometry Pass: render scene into G-Buffer
		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer->fbo);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND); // Disable blending for G-Buffer

		gShader->use();
		gShader->setMat4("proj", persp_proj);
		gShader->setMat4("view", view);
		gShader->setFloat("normalMapIntensity", normal);
		
		if (sponza) sponza->Draw(gShader); // Correctly pass gShader
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// 2. Lighting/Display Pass: render quad with G-Buffer textures
		glViewport(0, 0, width, height);
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND); // Re-enable for HUD
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		rectShader->use();
		rectShader->setInt("screenTexture", 0);
		glActiveTexture(GL_TEXTURE0);

		if (displayMode == 1) glBindTexture(GL_TEXTURE_2D, gBuffer->gPosition);
		if (displayMode == 2) glBindTexture(GL_TEXTURE_2D, gBuffer->gNormal);
		if (displayMode == 3) glBindTexture(GL_TEXTURE_2D, gBuffer->gAlbedoSpec);
		
		renderQuad();
		glEnable(GL_DEPTH_TEST);
	}

	if (displayMode == 4) {
		// --- SVO RAYCAST MODE OVERLAY ---
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		svoShader->use();
		svoShader->setVec3("cameraPos", camera.position);
		glm::mat4 invVP = glm::inverse(persp_proj * view);
		svoShader->setMat4("invViewProj", invVP);
		
		if (svo && svo->nodeSSBO) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, svo->nodeSSBO);
		}
		
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gBuffer->gAlbedoSpec);
		svoShader->setInt("gAlbedo", 1);

		renderQuad();
		glEnable(GL_DEPTH_TEST);
	}

	renderGUI();
	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	view = glm::lookAt(
		camera.position,
		camera.position + camera.direction,
		glm::vec3(0.0f,1.0f,0.0f) // Ensure the up vector is consistent and correct
	);
	
	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	shader = new Shader("simpleVertexShader.txt", "simpleFragmentShader.txt");
	gShader = new Shader("gbuffer_vs.glsl", "gbuffer_fs.glsl");
	rectShader = new Shader("rect_vs.glsl", "rect_fs.glsl");
	svoShader = new Shader("rect_vs.glsl", "svo_raycast_fs.glsl"); 
	gBuffer = new GBuffer(width, height);

	// Sponza
	std::cout << "Loading Sponza..." << std::endl;
	sponza = new Model("sponza/Sponza.gltf", glm::vec3(0.0f, 0.0f, 0.0f), shader);
	sponza->model = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

	// SVO
	svo = new SVO();
	if (sponza) SVOBuilder::build(sponza, 256, *svo);
	svo->initGPU();

	// Camera start position — inside the Sponza atrium
	camera.position  = glm::vec3(0.0f, 2.0f, 0.0f);
	camera.direction = glm::vec3(0.0f, 0.0f, -1.0f);
}

void cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();
	delete shader;
	delete gShader;
	delete rectShader;
	delete gBuffer;
	delete sponza;
}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitContextVersion(4, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	//glutInitWindowSize(width, height);
	int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
	int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);
	glutInitWindowSize(screenWidth, screenHeight);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Project");

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}

	ImGuiContext* ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(ctx);
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplGLUT_Init();
	ImGui_ImplOpenGL3_Init("#version 430");

	init();

	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mouseClick);
	glutMotionFunc(mouseDrag);
	glutPassiveMotionFunc(mouse);
	glutReshapeFunc(reshape);
	glutSetCursor(GLUT_CURSOR_NONE);

	atexit(cleanup);
	
	glutMainLoop();
	return 0;
}

