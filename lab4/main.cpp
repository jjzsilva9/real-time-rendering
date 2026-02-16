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
std::vector<Model*> cubes;

bool showGUI = false;
float normal = 1;

float lightPos[3] = { 10.0f, 10.0f, 4.0f };

// Material selection
static int currentMaterial = 0;
static int previousMaterial = -1;

enum class MipmapMode : int {
	Trilinear = 0,
	Bilinear = 1,
	Nearest = 2,
	NoMip = 3
};

static MipmapMode currentMipmapMode = MipmapMode::Trilinear;


float textureScale =1.0f;

// Pre-loaded material texture IDs (diffuse and normal for each material)
struct MaterialTextures {
	GLuint diffuse;
	GLuint normal;
};
MaterialTextures materialTextures[3]; // brick, wicker, fabric

// Forward declaration - defined in model.cpp
unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);

void applyMipmapMode(MipmapMode mode) {
	GLenum minFilter;
	switch (mode)
	{
	case MipmapMode::Trilinear:
		minFilter = GL_LINEAR_MIPMAP_LINEAR;
		break;
	case MipmapMode::Bilinear:
		minFilter = GL_LINEAR_MIPMAP_NEAREST;
		break;
	case MipmapMode::Nearest:
		minFilter = GL_NEAREST_MIPMAP_NEAREST;
		break;
	case MipmapMode::NoMip:
		minFilter = GL_LINEAR;
		break;
	default:
		minFilter = GL_LINEAR_MIPMAP_LINEAR;
		break;
	}
	for (const Texture& tex : Model::textures_loaded) {
		glBindTexture(GL_TEXTURE_2D, tex.id);
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	for (int i = 0; i < 3; i++) {
		glBindTexture(GL_TEXTURE_2D, materialTextures[i].diffuse);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);

		glBindTexture(GL_TEXTURE_2D, materialTextures[i].normal);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	}
}

// Function to update textures on all models
void updateModelTextures(int materialIndex) {
	std::vector<std::vector<Model*>*> allModels = { &cubes };
	
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

	// Forward/Backward and sideways movement
	if (key == 'w') {
		camera.position += camera.direction * CAMERASPEED * delta;
	}
	if (key == 'a') {
		camera.position -= glm::normalize(cross(camera.direction, camera.up)) * CAMERASPEED * delta;
	}
	if (key == 's') {
		camera.position -= camera.direction * CAMERASPEED * delta;
	}
	if (key == 'd') {
		camera.position += glm::normalize(cross(camera.direction, camera.up)) * CAMERASPEED * delta;
	}

	// Up/Down Movement
	if (key == 'e') {
		camera.position += camera.up * CAMERASPEED * delta;
	}
	if (key == 'q') {
		camera.position -= camera.up * CAMERASPEED * delta;
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
	// If the mouse has just entered the window
	// Set lastX and lastY to the current x and y
	if (firstMouse) {
		lastX = x;
		lastY = y;
		firstMouse = false;
	}

	// Calculate difference from last frame
	float xoffset = x - lastX;
	float yoffset = lastY - y;

	lastX = x;
	lastY = y;

	// Convert to yaw and pitch
	yaw += xoffset * 0.1f;
	pitch += yoffset * 0.1f;

	// Clip max and min pitch
	if (pitch > 89.0f) {
		pitch = 89.0f;
	}
	if (pitch < -89.0f) {
		pitch = -89.0f;
	}

	glm::vec3 viewDirection;
	viewDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	viewDirection.y = sin(glm::radians(pitch));
	viewDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camera.direction = glm::normalize(viewDirection);

	// If the cursor is near the edge of the screen, warp it to the centre
	if ((x < width / 100) || (x > 99 * width / 100) || (y < height / 100) || (y > 99 * height / 100)) {
		glutWarpPointer(width / 2, height / 2);
		lastX = width / 2;
		lastY = height / 2;
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

		if (ImGui::CollapsingHeader("MipMap Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

			const char* mipmapItems[] = { "Trilinear", "Bilinear", "Nearest", "No Mipmapping" };
			int modeIndex = static_cast<int>(currentMipmapMode);
			if (ImGui::CollapsingHeader("Texture Filtering", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImGui::Combo("Mipmap Mode", &modeIndex, mipmapItems, IM_ARRAYSIZE(mipmapItems))) {
					currentMipmapMode = static_cast<MipmapMode>(modeIndex);
					applyMipmapMode(currentMipmapMode);
				}
			}

			ImGui::SliderFloat("Texture Scale", &textureScale,1.0f,25.0f, "x %.1f");
		}
		
		if (ImGui::CollapsingHeader("Material Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
			const char* materials[] = { "Brick", "Wicker", "Checker" };
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

	glDepthFunc(GL_LESS);
	
	
	glm::vec3 rotation = glm::vec3(0.0, 50.0, 50.0);
	glm::vec3 position;
	
	shader->use();

	int view_mat_location = glGetUniformLocation(shader->ID, "view");
	int proj_mat_location = glGetUniformLocation(shader->ID, "proj");

	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));

	int texScaleLocation = glGetUniformLocation(shader->ID, "textureScale");
	glUniform1f(texScaleLocation, textureScale);

	cubes[0]->Draw();
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

	Model* cube = new Model("cube.obj", glm::vec3(0.0f ,0.0, -20.0), shader);
	
	float size = 1000.0f;

	cube->model = glm::scale(glm::mat4(1.0f), glm::vec3(size, size, size));
	
	float halfHeight = 1.2f * size;
	
	cube->model[3][0] = 0.0f;
	cube->model[3][1] = -halfHeight;
	cube->model[3][2] = -800.0f;
	

	cubes.push_back(cube);

	// Load material textures using TextureFromFile from model.cpp
	materialTextures[0].diffuse = TextureFromFile("textures/brick/diffuse.jpg", "");
	materialTextures[0].normal = TextureFromFile("textures/brick/normal.jpg", "");

	materialTextures[1].diffuse = TextureFromFile("textures/wicker/diffuse.jpg", "");
	materialTextures[1].normal = TextureFromFile("textures/wicker/normal.png", "");

	materialTextures[2].diffuse = TextureFromFile("textures/chess.png", "");
	materialTextures[2].normal = TextureFromFile("textures/wicker/normal.png", "");

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

