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
DirectionalLight* lightSource = nullptr;
Model* sponza = nullptr;

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
		ImGui::DragFloat("Normal Map", &normal, 0.1f, 0.0f, 10.0f);
		ImGui::DragFloat3("Light Position", lightPos, 0.1f);

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Custom function to print glm::mat4
void printMatrix(const glm::mat4& matrix, const std::string& name) {
	std::cout << name << ":\n";
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			std::cout << matrix[i][j] << " ";
		}
		std::cout << "\n";
	}
}

void display() {
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// --- Scene pass ---
	shader->use();
	glUniformMatrix4fv(glGetUniformLocation(shader->ID, "proj"),  1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(glGetUniformLocation(shader->ID, "view"),  1, GL_FALSE, glm::value_ptr(view));
	glUniform1f(glGetUniformLocation(shader->ID, "normalMapIntensity"), normal);
	glUniform4f(glGetUniformLocation(shader->ID, "LightPosition"), lightPos[0], lightPos[1], lightPos[2], 1.0f);

	if (sponza) sponza->Draw();

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

	// Camera start position — inside the Sponza atrium
	camera.position  = glm::vec3(0.0f, 2.0f, 0.0f);
	camera.direction = glm::vec3(0.0f, 0.0f, -1.0f);

	// Sponza
	std::cout << "Loading Sponza..." << std::endl;
	sponza = new Model("sponza/Sponza.gltf", glm::vec3(0.0f, 0.0f, 0.0f), shader);
	std::cout << "Sponza meshes: " << sponza->meshes.size() << std::endl;
	for (size_t i = 0; i < sponza->meshes.size(); i++)
		std::cout << "  Mesh " << i << " textures: " << sponza->meshes[i].textures.size() << std::endl;
}

void cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();
	delete shader;
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

