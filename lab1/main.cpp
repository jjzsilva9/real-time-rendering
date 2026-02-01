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

// Assimp
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Project includes
#include "maths_funcs.h"
#include "shader.h"
#include "model.h"
#include "directionallight.h"

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl3.h"

#define CAMERASPEED 50.0f


typedef struct
{
	vec3 position = vec3(0.0f, 0.0f, 0.0f);
	vec3 direction = vec3(0.0f, 0.0f, -1.0f);
	vec3 up = vec3(0.0f, 1.0f, 0.0f);
} Camera;
Camera camera;

using namespace std;

int width = 800;
int height = 600;

// Root of the Hierarchy
mat4 view = translate(identity_mat4(), vec3(0.0, 0.0, -10.0f));
mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);

float delta;
float yaw, pitch;
int lastX = width / 2;
int lastY = height / 2;
bool firstMouse = true;

Shader* phongShader = nullptr;
Shader* goochShader = nullptr;
Shader* orenNayarShader = nullptr;
std::vector<Shader*> shaders;
DirectionalLight* lightSource = nullptr;
std::vector<Model*> teapots;

bool showGUI = false;

float phong_Ld[3] = {0.7f, 0.7f, 0.7f};
float phong_Ls[3] = {1.0f, 1.0f, 1.0f};
float phong_La[3] = {0.25f, 0.35f, 0.425f};
float phong_Kd[3] = {1.0f, 1.0f, 1.0f};
float phong_Ks[3] = {1.0f, 1.0f, 1.0f};
float phong_Ka[3] = {1.0f, 1.0f, 1.0f};
float phong_Ns = 10.0f;

float gooch_cool[3] = {0.0f, 0.0f, 0.6f};
float gooch_warm[3] = {0.6f, 0.0f, 0.0f};
float gooch_Ns = 10.0f;

float orenNayar_Ld[3] = {0.7f, 0.7f, 0.7f};
float orenNayar_Kd[3] = {1.0f, 1.0f, 1.0f};
float orenNayar_roughness = 0.5f;

float lightPos[3] = {10.0f, 10.0f, 4.0f};

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
	persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);

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
		ImGui::Begin("Shader Controls", &showGUI, ImGuiWindowFlags_AlwaysAutoResize);
		
		// Shared Light Position
		if (ImGui::CollapsingHeader("Light Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat3("Light Position", lightPos, -50.0f, 50.0f);
		}

		// Phong Shader Controls
		if (ImGui::CollapsingHeader("Phong Shader (Left Teapot)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Light Properties:");
			ImGui::ColorEdit3("Ld - Diffuse Light", phong_Ld);
			ImGui::ColorEdit3("Ls - Specular Light", phong_Ls);
			ImGui::ColorEdit3("La - Ambient Light", phong_La);
			
			ImGui::Separator();
			ImGui::Text("Material Properties:");
			ImGui::ColorEdit3("Kd - Diffuse Material", phong_Kd);
			ImGui::ColorEdit3("Ks - Specular Material", phong_Ks);
			ImGui::ColorEdit3("Ka - Ambient Material", phong_Ka);
			ImGui::SliderFloat("Ns - Shininess", &phong_Ns, 1.0f, 200.0f);
		}

		// Gooch Shader Controls
		if (ImGui::CollapsingHeader("Gooch Shader (Middle Teapot)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit3("Cool Color", gooch_cool);
			ImGui::ColorEdit3("Warm Color", gooch_warm);
			ImGui::SliderFloat("Ns - Shininess##Gooch", &gooch_Ns, 1.0f, 200.0f);
		}

		// Oren-Nayar Shader Controls
		if (ImGui::CollapsingHeader("Oren-Nayar Shader (Right Teapot)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit3("Ld - Diffuse Light##ON", orenNayar_Ld);
			ImGui::ColorEdit3("Kd - Diffuse Material##ON", orenNayar_Kd);
			ImGui::SliderFloat("Roughness", &orenNayar_roughness, 0.0f, 1.0f);
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void display() {
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDepthFunc(GL_LESS);
	
	
	vec3 rotation = vec3(0.0, 50.0, 50.0);
	vec3 position;
	Model* teapot;
	for (int i = 0; i < 3; i++) {
		shaders[i]->use();

		int view_mat_location = glGetUniformLocation(shaders[i]->ID, "view");
		int proj_mat_location = glGetUniformLocation(shaders[i]->ID, "proj");

		glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
		glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);

		// Set shared light position for all shaders
		int light_pos_location = glGetUniformLocation(shaders[i]->ID, "LightPosition");
		glUniform4f(light_pos_location, lightPos[0], lightPos[1], lightPos[2], 1.0f);

		// Set shader-specific uniforms
		if (i == 0) { // Phong Shader
			shaders[i]->setVec3("Ld", vec3(phong_Ld[0], phong_Ld[1], phong_Ld[2]));
			shaders[i]->setVec3("Ls", vec3(phong_Ls[0], phong_Ls[1], phong_Ls[2]));
			shaders[i]->setVec3("La", vec3(phong_La[0], phong_La[1], phong_La[2]));
			shaders[i]->setVec3("Kd", vec3(phong_Kd[0], phong_Kd[1], phong_Kd[2]));
			shaders[i]->setVec3("Ks", vec3(phong_Ks[0], phong_Ks[1], phong_Ks[2]));
			shaders[i]->setVec3("Ka", vec3(phong_Ka[0], phong_Ka[1], phong_Ka[2]));
			shaders[i]->setFloat("Ns", phong_Ns);
		}
		else if (i == 1) { // Gooch Shader
			shaders[i]->setVec3("cool", vec3(gooch_cool[0], gooch_cool[1], gooch_cool[2]));
			shaders[i]->setVec3("warm", vec3(gooch_warm[0], gooch_warm[1], gooch_warm[2]));
			shaders[i]->setFloat("Ns", gooch_Ns);
		}
		else if (i == 2) { // Oren-Nayar Shader
			shaders[i]->setVec3("Ld", vec3(orenNayar_Ld[0], orenNayar_Ld[1], orenNayar_Ld[2]));
			shaders[i]->setVec3("Kd", vec3(orenNayar_Kd[0], orenNayar_Kd[1], orenNayar_Kd[2]));
			shaders[i]->setFloat("roughness", orenNayar_roughness);
		}

		teapot = teapots[i];
		position = vec3(teapot->model.m[12], teapot->model.m[13], teapot->model.m[14]);
		teapot->translate(vec3(-position.v[0], -position.v[1], -position.v[2]));
		teapot->rotate(rotation*delta);
		teapot->translate(position);
		teapot->Draw();
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

	view = look_at(camera.position, camera.position + camera.direction, camera.up);
	
	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	phongShader = new Shader("vertexShader.txt", "fragmentShader.txt");
	goochShader = new Shader("vertexShader.txt", "goochFS.txt");
	orenNayarShader = new Shader("vertexShader.txt", "orenNayarFS.txt");
	shaders.push_back(phongShader);
	shaders.push_back(goochShader);
	shaders.push_back(orenNayarShader);
	
	Model* teapot;
	for (int i = 0; i < 3; i++) {
		teapot = new Model("utah_teapot.obj", vec3((7.5f * i) - 7.5f, 0.0, -20.0), shaders[i]);
		teapots.push_back(teapot);
	}
}

void cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();

	delete phongShader;
	delete goochShader;
	delete orenNayarShader;
}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
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
	ImGui_ImplOpenGL3_Init("#version 330");

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

