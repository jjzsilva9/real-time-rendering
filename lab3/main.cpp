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
#include "skybox.h"

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
Shader* skyboxShader = nullptr;
DirectionalLight* lightSource = nullptr;
std::vector<Model*> teapots;
std::vector<Model*> cubes;
Skybox* skybox = nullptr;

bool showGUI = false;

float eta = 0.5;
float chromatic = 0.0;
bool rotating = false;
static int shape = 0;
float normal = 1;

float lightPos[3] = { 10.0f, 10.0f, 4.0f };

// Material selection
static int currentMaterial = 0;
static int previousMaterial = -1;

// Pre-loaded material texture IDs (diffuse and normal for each material)
struct MaterialTextures {
	GLuint diffuse;
	GLuint normal;
};
MaterialTextures materialTextures[3]; // brick, wicker, fabric

// Forward declaration - defined in model.cpp
unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);

// Function to update textures on all models
void updateModelTextures(int materialIndex) {
	std::vector<std::vector<Model*>*> allModels = { &cubes, &teapots };
	
	for (auto* modelList : allModels) {
		for (Model* model : *modelList) {
			for (Mesh& mesh : model->meshes) {
				// Clear existing textures and add the new ones
				mesh.textures.clear();
				
				Texture diffuseTex;
				diffuseTex.id = materialTextures[materialIndex].diffuse;
				diffuseTex.type = "texture_diffuse";
				mesh.textures.push_back(diffuseTex);
				
				Texture normalTex;
				normalTex.id = materialTextures[materialIndex].normal;
				normalTex.type = "texture_normal";
				mesh.textures.push_back(normalTex);
			}
		}
	}
}

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
		ImGui::Begin("Shader Controls", &showGUI, ImGuiWindowFlags_AlwaysAutoResize);

		if (ImGui::CollapsingHeader("Object Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Rotate", &rotating);

			const char* items[] = { "Teapot", "Cube" };
			ImGui::Combo("Shape", &shape, items, IM_ARRAYSIZE(items));

			ImGui::DragFloat("Intensity", &normal, 0.1f, 0.0f, 10.0f);
		}
		
		if (ImGui::CollapsingHeader("Material Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
			const char* materials[] = { "Brick", "Wicker", "Fabric" };
			ImGui::Combo("Material", &currentMaterial, materials, IM_ARRAYSIZE(materials));
			
			// Update model textures if material has changed
			if (currentMaterial != previousMaterial) {
				updateModelTextures(currentMaterial);
				previousMaterial = currentMaterial;
			}
		}
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
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDepthFunc(GL_LEQUAL);
	skyboxShader->use();

	glm::mat4 skyboxView = view;
	skyboxView[3][0] = skyboxView[3][1] = skyboxView[3][2] =0.0f;

	int view_mat_location = glGetUniformLocation(skyboxShader->ID, "view");
	int proj_mat_location = glGetUniformLocation(skyboxShader->ID, "projection");
	int skybox_location = glGetUniformLocation(skyboxShader->ID, "skybox");
	int time_location = glGetUniformLocation(skyboxShader->ID, "timeOfDay");

	glUniformMatrix4fv(proj_mat_location,1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(view_mat_location,1, GL_FALSE, glm::value_ptr(skyboxView));
	glUniform1i(skybox_location,0);

	glBindVertexArray(skybox->VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->textureID);
	glDrawArrays(GL_TRIANGLES,0,36);

	glDepthFunc(GL_LESS);
	
	
	glm::vec3 rotation = glm::vec3(0.0, 50.0, 50.0);
	glm::vec3 position;
	
	// Select the current model list based on shape selection
	std::vector<Model*>* currentModels = nullptr;
	switch (shape) {
		case 0: currentModels = &teapots; break;
		case 1: currentModels = &cubes; break;
		default: currentModels = &teapots; break;
	}
	
	shader->use();

	view_mat_location = glGetUniformLocation(shader->ID, "view");
	proj_mat_location = glGetUniformLocation(shader->ID, "proj");

	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));

	int normal_location = glGetUniformLocation(shader->ID, "normalMapIntensity");
	glUniform1f(normal_location, normal);

	int light_pos_location = glGetUniformLocation(shader->ID, "LightPosition");
	glUniform4f(light_pos_location, lightPos[0], lightPos[1], lightPos[2], 1.0f);

	for (int i = 0; i < 3; i++) {
		Model* currentModel = (*currentModels)[i];
		if (rotating) {
			position = glm::vec3(currentModel->model[3][0], currentModel->model[3][1], currentModel->model[3][2]);
			currentModel->translate(-position);
			currentModel->rotate(rotation * delta);
			currentModel->translate(position);
		}
		currentModel->Draw();
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
	skyboxShader = new Shader("skyboxVertexShader.txt", "skyboxFragmentShader.txt");
	
	// Load teapots
	for (int i = 0; i < 3; i++) {
		Model* teapot = new Model("utah_teapot.obj", glm::vec3((7.5f * i) - 7.5f, 0.0, -20.0), shader);
		teapots.push_back(teapot);
	}

	// Load cubes
	for (int i = 0; i < 3; i++) {
		Model* cube = new Model("cube.obj", glm::vec3((7.5f * i) - 7.5f, 0.0, -20.0), shader);
		cube->model = glm::scale(cube->model, glm::vec3(2.0f, 2.0f, 2.0f));
		cubes.push_back(cube);
	}
	
	std::vector<std::string> faces = {
		"textures/cubemaps/posx.jpg",
		"textures/cubemaps/negx.jpg",
		"textures/cubemaps/posy.jpg",
		"textures/cubemaps/negy.jpg",
		"textures/cubemaps/posz.jpg",
		"textures/cubemaps/negz.jpg"
	};
	skybox = new Skybox(faces);

	// Load material textures using TextureFromFile from model.cpp
	materialTextures[0].diffuse = TextureFromFile("textures/brick/diffuse.jpg", "");
	materialTextures[0].normal = TextureFromFile("textures/brick/normal.jpg", "");

	materialTextures[1].diffuse = TextureFromFile("textures/wicker/diffuse.jpg", "");
	materialTextures[1].normal = TextureFromFile("textures/wicker/normal.png", "");

	materialTextures[2].diffuse = TextureFromFile("textures/fabric/diffuse.jpg", "");
	materialTextures[2].normal = TextureFromFile("textures/fabric/normal.png", "");

	// Debug: print texture IDs
	std::cout << "Brick diffuse: " << materialTextures[0].diffuse << ", normal: " << materialTextures[0].normal << std::endl;
	std::cout << "Wicker diffuse: " << materialTextures[1].diffuse << ", normal: " << materialTextures[1].normal << std::endl;
	std::cout << "Fabric diffuse: " << materialTextures[2].diffuse << ", normal: " << materialTextures[2].normal << std::endl;

	// Apply initial material to all models
	updateModelTextures(currentMaterial);
	previousMaterial = currentMaterial;
}

void cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();

	delete shader;
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

