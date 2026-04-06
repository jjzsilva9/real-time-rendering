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
#include "gbuffer.h"
#include "svo_builder.h"
#include "svo.h"
#include "light_view_map.h"

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
Shader* lightViewShader = nullptr;
Shader* radianceShader = nullptr;
Shader* radianceFilteringShader = nullptr;
GBuffer* gBuffer = nullptr;
LightViewMap* lightViewMap = nullptr;
Model* sponza = nullptr;
SVO* svo = nullptr;

GLuint quadVAO = 0, quadVBO;
int displayMode = 0; // 0: Combined, 1: Pos, 2: Norm, 3: Albedo, 4: SVO Raycast, 5: Light View

bool showGUI = false;

float normal = 1.0f;  // normal map intensity

float lightPos[3] = { 0.0f, 8.0f, 0.0f };

// SVO / cone tracing settings
int   svoResolution    = 128;
float coneApertureDeg  = 60.0f; // displayed in degrees, sent to shader as radians
int   numCones         = 6;
float indirectBoost    = 2.5f;
float specularBoost    = 1.0f;

int   shadowMapRes     = 1024; // shadow map / light-view map resolution

// Dynamic objects
struct DynObject { glm::vec3 basePos; float spawnTime; };
std::vector<DynObject> dynObjects;
GLuint cubeVAO = 0, cubeVBO = 0;
GLuint whiteTex = 0, flatNormalTex = 0;

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
		ImGui::Combo("Display Mode", &displayMode, "Final\0Position\0Normal\0Albedo\0SVO Raycast\0Light View\0\0");

		ImGui::Separator();
		ImGui::Text("SVO");
		const char* resItems[] = { "64", "128", "256" };
		const int   resValues[] = { 64, 128, 256 };
		static int resIdx = 1; // default 128
		ImGui::Combo("Resolution", &resIdx, resItems, 3);
		svoResolution = resValues[resIdx];
		ImGui::SameLine();
		if (ImGui::Button("Rebuild")) {
			if (sponza && svo) SVOBuilder::build(sponza, svoResolution, *svo);
			if (svo) svo->initGPU();
		}

		ImGui::Separator();
		ImGui::Text("Shadow Map");
		{
			static int smIdx = 1; // default 1024
			const char* smItems[] = { "512", "1024", "2048", "4096" };
			const int   smValues[] = { 512, 1024, 2048, 4096 };
			if (ImGui::Combo("Resolution##sm", &smIdx, smItems, 4)) {
				shadowMapRes = smValues[smIdx];
				delete lightViewMap;
				lightViewMap = new LightViewMap(shadowMapRes, shadowMapRes);
				lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f));
			}
		}

		ImGui::Separator();
		ImGui::Text("Cone Tracing");
		ImGui::SliderFloat("Aperture (deg)", &coneApertureDeg, 10.0f, 90.0f);
		ImGui::SliderInt("Num Cones", &numCones, 1, 6);
		ImGui::SliderFloat("Indirect Boost", &indirectBoost, 0.0f, 2.5f);
		ImGui::SliderFloat("Specular Boost", &specularBoost, 0.0f, 1.0f);

		ImGui::Separator();
		if (ImGui::DragFloat3("Light Position", lightPos, 0.5f)) {
			if (lightViewMap) lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f, 0.0f, 0.0f));
		}

		ImGui::Separator();
		ImGui::Text("Dynamic Objects (%d)", (int)dynObjects.size());
		if (ImGui::Button("Spawn Object")) {
			float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
			static const glm::vec3 spawnPositions[] = {
				glm::vec3( 0.0f, 2.0f,  0.0f),
				glm::vec3( 8.0f, 2.0f,  4.0f),
				glm::vec3(-8.0f, 2.0f,  4.0f),
				glm::vec3( 8.0f, 2.0f, -4.0f),
				glm::vec3(-8.0f, 2.0f, -4.0f),
				glm::vec3( 0.0f, 2.0f,  8.0f),
			};
			int idx = (int)dynObjects.size() % 6;
			dynObjects.push_back({ spawnPositions[idx], t });
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			dynObjects.clear();
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void initCubeAssets() {
    // 36 vertices, each: pos(3) normal(3) uv(2) tangent(3) bitangent(3) = 14 floats
    static const float cubeVerts[] = {
        // +Z face — normal 0,0,1 | tangent 1,0,0 | bitangent 0,1,0
        -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,  1,0,0,  0,1,0,
         0.5f,-0.5f, 0.5f,  0,0,1,  1,0,  1,0,0,  0,1,0,
         0.5f, 0.5f, 0.5f,  0,0,1,  1,1,  1,0,0,  0,1,0,
        -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,  1,0,0,  0,1,0,
         0.5f, 0.5f, 0.5f,  0,0,1,  1,1,  1,0,0,  0,1,0,
        -0.5f, 0.5f, 0.5f,  0,0,1,  0,1,  1,0,0,  0,1,0,
        // -Z face — normal 0,0,-1 | tangent -1,0,0 | bitangent 0,1,0
         0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,  -1,0,0,  0,1,0,
        -0.5f,-0.5f,-0.5f,  0,0,-1,  1,0,  -1,0,0,  0,1,0,
        -0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,  -1,0,0,  0,1,0,
         0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,  -1,0,0,  0,1,0,
        -0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,  -1,0,0,  0,1,0,
         0.5f, 0.5f,-0.5f,  0,0,-1,  0,1,  -1,0,0,  0,1,0,
        // +X face — normal 1,0,0 | tangent 0,0,-1 | bitangent 0,1,0
         0.5f,-0.5f, 0.5f,  1,0,0,  0,0,  0,0,-1,  0,1,0,
         0.5f,-0.5f,-0.5f,  1,0,0,  1,0,  0,0,-1,  0,1,0,
         0.5f, 0.5f,-0.5f,  1,0,0,  1,1,  0,0,-1,  0,1,0,
         0.5f,-0.5f, 0.5f,  1,0,0,  0,0,  0,0,-1,  0,1,0,
         0.5f, 0.5f,-0.5f,  1,0,0,  1,1,  0,0,-1,  0,1,0,
         0.5f, 0.5f, 0.5f,  1,0,0,  0,1,  0,0,-1,  0,1,0,
        // -X face — normal -1,0,0 | tangent 0,0,1 | bitangent 0,1,0
        -0.5f,-0.5f,-0.5f,  -1,0,0,  0,0,  0,0,1,  0,1,0,
        -0.5f,-0.5f, 0.5f,  -1,0,0,  1,0,  0,0,1,  0,1,0,
        -0.5f, 0.5f, 0.5f,  -1,0,0,  1,1,  0,0,1,  0,1,0,
        -0.5f,-0.5f,-0.5f,  -1,0,0,  0,0,  0,0,1,  0,1,0,
        -0.5f, 0.5f, 0.5f,  -1,0,0,  1,1,  0,0,1,  0,1,0,
        -0.5f, 0.5f,-0.5f,  -1,0,0,  0,1,  0,0,1,  0,1,0,
        // +Y face — normal 0,1,0 | tangent 1,0,0 | bitangent 0,0,-1
        -0.5f, 0.5f, 0.5f,  0,1,0,  0,0,  1,0,0,  0,0,-1,
         0.5f, 0.5f, 0.5f,  0,1,0,  1,0,  1,0,0,  0,0,-1,
         0.5f, 0.5f,-0.5f,  0,1,0,  1,1,  1,0,0,  0,0,-1,
        -0.5f, 0.5f, 0.5f,  0,1,0,  0,0,  1,0,0,  0,0,-1,
         0.5f, 0.5f,-0.5f,  0,1,0,  1,1,  1,0,0,  0,0,-1,
        -0.5f, 0.5f,-0.5f,  0,1,0,  0,1,  1,0,0,  0,0,-1,
        // -Y face — normal 0,-1,0 | tangent 1,0,0 | bitangent 0,0,1
        -0.5f,-0.5f,-0.5f,  0,-1,0,  0,0,  1,0,0,  0,0,1,
         0.5f,-0.5f,-0.5f,  0,-1,0,  1,0,  1,0,0,  0,0,1,
         0.5f,-0.5f, 0.5f,  0,-1,0,  1,1,  1,0,0,  0,0,1,
        -0.5f,-0.5f,-0.5f,  0,-1,0,  0,0,  1,0,0,  0,0,1,
         0.5f,-0.5f, 0.5f,  0,-1,0,  1,1,  1,0,0,  0,0,1,
        -0.5f,-0.5f, 0.5f,  0,-1,0,  0,1,  1,0,0,  0,0,1,
    };
    const int stride = 14 * sizeof(float);

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(11*sizeof(float)));
    glBindVertexArray(0);

    // 1×1 white diffuse texture
    glGenTextures(1, &whiteTex);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    unsigned char white[4] = { 220, 200, 180, 255 }; // warm off-white
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 1×1 flat normal map (tangent-space up = 0.5, 0.5, 1.0)
    glGenTextures(1, &flatNormalTex);
    glBindTexture(GL_TEXTURE_2D, flatNormalTex);
    unsigned char flatN[4] = { 128, 128, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flatN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void drawCubes(Shader* s, bool modelOnly = false) {
    if (dynObjects.empty() || cubeVAO == 0) return;
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    if (!modelOnly) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        s->setInt("ourTexture", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, flatNormalTex);
        s->setInt("normalMap", 1);
    }

    glBindVertexArray(cubeVAO);
    for (const auto& obj : dynObjects) {
        float elapsed = t - obj.spawnTime;
        glm::vec3 pos = obj.basePos + glm::vec3(0.0f, 1.5f * std::sin(elapsed * 1.2f), 0.0f);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        m = glm::rotate(m, elapsed * 0.8f, glm::vec3(0.3f, 1.0f, 0.2f));
        m = glm::scale(m, glm::vec3(2.0f));
        glUniformMatrix4fv(glGetUniformLocation(s->ID, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
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
	// --- LIGHT-VIEW MAP PASS ---
	if (lightViewMap && lightViewShader) {
		lightViewMap->bindForWriting();
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		lightViewShader->use();
		lightViewShader->setMat4("lightProj", lightViewMap->lightProj);
		lightViewShader->setMat4("lightView", lightViewMap->lightView);
		
		if (sponza) sponza->Draw(lightViewShader);
		drawCubes(lightViewShader, true);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height); // Restore viewport

		// --- RADIANCE INJECTION PASS ---
		if (svo && radianceShader) {
			radianceShader->use();
			radianceShader->setMat4("lightViewProj", lightViewMap->lightProj * lightViewMap->lightView);
			radianceShader->setVec3("lightDir", glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(lightPos[0], lightPos[1], lightPos[2])));
			radianceShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
			radianceShader->setUInt("numLeaves", (unsigned int)svo->colors.size());
			radianceShader->setInt("numPhotons", 1);
			radianceShader->setFloat("voxelSize", 200.0f / (float)svoResolution);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, lightViewMap->depthMap);
			radianceShader->setInt("shadowMap", 0);
			radianceShader->setVec3("lightColor", glm::vec3(1.5f, 1.5f, 1.4f)); // Strong sun
			radianceShader->setUInt("numLeaves", (uint32_t)svo->colors.size());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, lightViewMap->radianceMap);
			radianceShader->setInt("albedoMap", 1);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svo->colorSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, svo->normalSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, svo->positionSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, svo->radianceSSBO);

			int numGroups = ((int)svo->colors.size() + 255) / 256;
			glDispatchCompute(numGroups, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// --- RADIANCE FILTERING PASS (MIB-MAPPING) ---
		if (svo && !svo->levelOffsets.empty() && radianceFilteringShader) {
			radianceFilteringShader->use();
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, svo->nodeSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svo->radianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, svo->filteredRadianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, svo->neighborSSBO);

			// Bottom-up pass
			for (int i = (int)svo->levelOffsets.size() - 1; i >= 0; i--) {
				uint32_t levelStart = svo->levelOffsets[i];
				uint32_t levelEnd = (i == (int)svo->levelOffsets.size() - 1) ? (uint32_t)svo->nodes.size() : svo->levelOffsets[i + 1];
				uint32_t levelSize = levelEnd - levelStart;

				if (levelSize > 0) {
					radianceFilteringShader->setUInt("levelStart", levelStart);
					radianceFilteringShader->setUInt("levelSize", levelSize);
					int numGroups = (levelSize + 255) / 256;
					glDispatchCompute(numGroups, 1, 1);
					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
				}
			}
		}
	}

	if (displayMode == 0) {
		// --- FINAL MODE: Normal Forward Rendering ---
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader->use();
		shader->setMat4("proj",  persp_proj);
		shader->setMat4("view",  view);
		shader->setMat4("model", sponza->model); 
		shader->setFloat("normalMapIntensity", normal);
		shader->setVec3("viewPos", camera.position);
		shader->setMat4("lightViewProj", lightViewMap->lightProj * lightViewMap->lightView);
		shader->setFloat("coneAperture", glm::radians(coneApertureDeg));
		shader->setInt("numCones", numCones);
		shader->setFloat("indirectBoost", indirectBoost);
		shader->setFloat("specularBoost", specularBoost);
		shader->setFloat("svoVoxelSize", 200.0f / (float)svoResolution);
		
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, lightViewMap->depthMap);
		shader->setInt("shadowMap", 2);

		// Directional Light: direction from lightPos to origin
		glm::vec3 lightDirection = glm::normalize(glm::vec3(0.0f) - glm::vec3(lightPos[0], lightPos[1], lightPos[2]));
		shader->setVec3("DirectLightDir", lightDirection);
		
		// shader expects vec4 for LightPosition (still passed for compatibility if needed)
		glUniform4f(glGetUniformLocation(shader->ID, "LightPosition"), lightPos[0], lightPos[1], lightPos[2], 1.0f);

		if (svo && svo->nodeSSBO) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, svo->nodeSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, svo->radianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, svo->filteredRadianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, svo->neighborSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, svo->contourSSBO);
		}

		if (sponza) sponza->Draw();
		drawCubes(shader);
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
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, svo->neighborSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, svo->contourSSBO);
		}
		
		if (svo && svo->radianceSSBO) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, svo->radianceSSBO);
		}
		
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gBuffer->gAlbedoSpec);
		svoShader->setInt("gAlbedo", 1);

		renderQuad();
		glEnable(GL_DEPTH_TEST);
	}

	if (displayMode == 5) {
		// --- LIGHT VIEW VISUALIZATION ---
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		rectShader->use();
		rectShader->setInt("screenTexture", 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, lightViewMap->radianceMap);

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
	lightViewShader = new Shader("light_view_vs.glsl", "light_view_fs.glsl");
	radianceShader = new Shader("radiance_injection.glsl");
	radianceFilteringShader = new Shader("radiance_filtering.glsl");
	gBuffer = new GBuffer(width, height);
	lightViewMap = new LightViewMap(1024, 1024);

	// Light position initialization (shining down from above)
	lightPos[0] = 5.0f; lightPos[1] = 25.0f; lightPos[2] = 2.0f;
	lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f, 0.0f, 0.0f));

	// Sponza
	std::cout << "Loading Sponza..." << std::endl;
	sponza = new Model("sponza/Sponza.gltf", glm::vec3(0.0f, 0.0f, 0.0f), shader);
	sponza->model = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

	// SVO
	svo = new SVO();
	if (sponza) SVOBuilder::build(sponza, svoResolution, *svo);
	svo->initGPU();

	initCubeAssets();

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
	delete lightViewShader;
	delete radianceShader;
	delete radianceFilteringShader;
	delete gBuffer;
	delete lightViewMap;
	delete sponza;
	if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
	if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
	if (whiteTex) glDeleteTextures(1, &whiteTex);
	if (flatNormalTex) glDeleteTextures(1, &flatNormalTex);
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

