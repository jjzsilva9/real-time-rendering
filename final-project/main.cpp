#pragma warning(disable : 5208)

// Standard library includes
#include <string>
#include <vector>
#include <iostream>
#include <limits>
#include <math.h>
#include <chrono>
#include <fstream>
#include <random>

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
float cubeScale        = 6.0f;
float specularBoost    = 1.0f;
float phongNs          = 80.0f;

int   shadowMapRes     = 1024; // shadow map / light-view map resolution

bool dynamicSVOMode = false;  // rebuild SVO every frame at 64^3, 4 cones

// Dynamic objects
struct DynObject { glm::vec3 basePos; float spawnTime; glm::vec3 color; };
std::vector<DynObject> dynObjects;

static glm::vec3 randomBrightColor() {
    // Generate saturated colours by keeping one channel high, one mid, one low
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float h = dist(rng);               // hue 0-1
    float s = 0.7f + dist(rng) * 0.3f; // saturation 0.7-1.0
    float v = 0.8f + dist(rng) * 0.2f; // value 0.8-1.0
    // HSV -> RGB
    int   i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (i % 6) {
        case 0: return { v, t, p };
        case 1: return { q, v, p };
        case 2: return { p, v, t };
        case 3: return { p, q, v };
        case 4: return { t, p, v };
        default: return { v, p, q };
    }
}

static const glm::vec3 kSpawnPositions[] = {
    {  0.0f, 2.0f,  0.0f },
    {  8.0f, 2.0f,  4.0f },
    { -8.0f, 2.0f,  4.0f },
    {  8.0f, 2.0f, -4.0f },
    { -8.0f, 2.0f, -4.0f },
    {  0.0f, 2.0f,  8.0f },
};
GLuint cubeVAO = 0, cubeVBO = 0;
GLuint whiteTex = 0, flatNormalTex = 0;

// Cube geometry: 36 vertices, each: pos(3) normal(3) uv(2) tangent(3) bitangent(3) = 14 floats
// Positions are in [-0.5, 0.5]^3 local space. Scale by 2 -> [-1, 1]^3 world units per side.
static const float kCubeVerts[] = {
    // +Z face
    -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,  1,0,0,  0,1,0,
     0.5f,-0.5f, 0.5f,  0,0,1,  1,0,  1,0,0,  0,1,0,
     0.5f, 0.5f, 0.5f,  0,0,1,  1,1,  1,0,0,  0,1,0,
    -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,  1,0,0,  0,1,0,
     0.5f, 0.5f, 0.5f,  0,0,1,  1,1,  1,0,0,  0,1,0,
    -0.5f, 0.5f, 0.5f,  0,0,1,  0,1,  1,0,0,  0,1,0,
    // -Z face
     0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,  -1,0,0,  0,1,0,
    -0.5f,-0.5f,-0.5f,  0,0,-1,  1,0,  -1,0,0,  0,1,0,
    -0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,  -1,0,0,  0,1,0,
     0.5f,-0.5f,-0.5f,  0,0,-1,  0,0,  -1,0,0,  0,1,0,
    -0.5f, 0.5f,-0.5f,  0,0,-1,  1,1,  -1,0,0,  0,1,0,
     0.5f, 0.5f,-0.5f,  0,0,-1,  0,1,  -1,0,0,  0,1,0,
    // +X face
     0.5f,-0.5f, 0.5f,  1,0,0,  0,0,  0,0,-1,  0,1,0,
     0.5f,-0.5f,-0.5f,  1,0,0,  1,0,  0,0,-1,  0,1,0,
     0.5f, 0.5f,-0.5f,  1,0,0,  1,1,  0,0,-1,  0,1,0,
     0.5f,-0.5f, 0.5f,  1,0,0,  0,0,  0,0,-1,  0,1,0,
     0.5f, 0.5f,-0.5f,  1,0,0,  1,1,  0,0,-1,  0,1,0,
     0.5f, 0.5f, 0.5f,  1,0,0,  0,1,  0,0,-1,  0,1,0,
    // -X face
    -0.5f,-0.5f,-0.5f,  -1,0,0,  0,0,  0,0,1,  0,1,0,
    -0.5f,-0.5f, 0.5f,  -1,0,0,  1,0,  0,0,1,  0,1,0,
    -0.5f, 0.5f, 0.5f,  -1,0,0,  1,1,  0,0,1,  0,1,0,
    -0.5f,-0.5f,-0.5f,  -1,0,0,  0,0,  0,0,1,  0,1,0,
    -0.5f, 0.5f, 0.5f,  -1,0,0,  1,1,  0,0,1,  0,1,0,
    -0.5f, 0.5f,-0.5f,  -1,0,0,  0,1,  0,0,1,  0,1,0,
    // +Y face
    -0.5f, 0.5f, 0.5f,  0,1,0,  0,0,  1,0,0,  0,0,-1,
     0.5f, 0.5f, 0.5f,  0,1,0,  1,0,  1,0,0,  0,0,-1,
     0.5f, 0.5f,-0.5f,  0,1,0,  1,1,  1,0,0,  0,0,-1,
    -0.5f, 0.5f, 0.5f,  0,1,0,  0,0,  1,0,0,  0,0,-1,
     0.5f, 0.5f,-0.5f,  0,1,0,  1,1,  1,0,0,  0,0,-1,
    -0.5f, 0.5f,-0.5f,  0,1,0,  0,1,  1,0,0,  0,0,-1,
    // -Y face
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0,0,  1,0,0,  0,0,1,
     0.5f,-0.5f,-0.5f,  0,-1,0,  1,0,  1,0,0,  0,0,1,
     0.5f,-0.5f, 0.5f,  0,-1,0,  1,1,  1,0,0,  0,0,1,
    -0.5f,-0.5f,-0.5f,  0,-1,0,  0,0,  1,0,0,  0,0,1,
     0.5f,-0.5f, 0.5f,  0,-1,0,  1,1,  1,0,0,  0,0,1,
    -0.5f,-0.5f, 0.5f,  0,-1,0,  0,1,  1,0,0,  0,0,1,
};

// GPU timing
GLuint timerQueries[3] = { 0, 0, 0 }; // [0]=injection, [1]=filtering, [2]=forward
double gpuTimeInjection = 0.0, gpuTimeFiltering = 0.0, gpuTimeForward = 0.0;
double svoBuildTimeMs = 0.0;

// ---- Benchmark system ----
enum class BenchState { Idle, Warmup, Measure, Done };

struct BenchConfig { int svoRes, numCones, numDynObjs, shadowRes; const char* label; };
struct BenchResult  { BenchConfig cfg; double buildMs, injMs, filtMs, fwdMs, fps; };

static const BenchConfig kBenchConfigs[] = {
    // SVO resolution sweep: 6 cones, no dynamics, 1024 shadow map
    { 64,  6, 0, 1024, "64^3, 6 cones"   },
    {128,  6, 0, 1024, "128^3, 6 cones"  },
    {256,  6, 0, 1024, "256^3, 6 cones"  },
    // Cone count sweep: 128^3, no dynamics, 1024 shadow map (6-cone already above)
    {128,  1, 0, 1024, "128^3, 1 cone"   },
    {128,  3, 0, 1024, "128^3, 3 cones"  },
    {128,  5, 0, 1024, "128^3, 5 cones"  },
    // Dynamic objects: 128^3, 6 cones, 1024 shadow map
    {128,  6, 1, 1024, "128^3, 1 dyn"    },
    {128,  6, 3, 1024, "128^3, 3 dyn"    },
    {128,  6, 5, 1024, "128^3, 5 dyn"    },
    // Shadow map resolution sweep: 128^3, 6 cones, no dynamics (1024 already above)
    {128,  6, 0,  512, "128^3, sm=512"   },
    {128,  6, 0, 2048, "128^3, sm=2048"  },
    {128,  6, 0, 4096, "128^3, sm=4096"  },
};
static const int kNumBenchConfigs = (int)(sizeof(kBenchConfigs) / sizeof(kBenchConfigs[0]));

static const double kWarmupSecs  = 1.0;
static const double kMeasureSecs = 2.0;

BenchState            benchState     = BenchState::Idle;
int                   benchConfigIdx = 0;
double                benchTimer     = 0.0;
double                benchFpsAccum  = 0.0;
double                benchInjAccum  = 0.0;
double                benchFiltAccum = 0.0;
double                benchFwdAccum  = 0.0;
int                   benchSamples   = 0;
std::vector<BenchResult> benchResults;

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


void applyBenchConfig(int idx); // forward declaration

void renderGUI() {
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)width, (float)height);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGLUT_NewFrame();
	ImGui::NewFrame();

	if (showGUI) {
		ImGui::Begin("Controls", &showGUI, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("SVO Build: %.1f ms", svoBuildTimeMs);
		ImGui::Text("GPU Injection: %.2f ms", gpuTimeInjection);
		ImGui::Text("GPU Filtering: %.2f ms", gpuTimeFiltering);
		ImGui::Text("GPU Forward:   %.2f ms", gpuTimeForward);
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
			if (sponza && svo) {
				auto _t0 = std::chrono::high_resolution_clock::now();
				SVOBuilder::build(sponza, svoResolution, *svo);
				auto _t1 = std::chrono::high_resolution_clock::now();
				svoBuildTimeMs = std::chrono::duration<double, std::milli>(_t1 - _t0).count();
			}
			if (svo) svo->initGPU();
		}
		ImGui::SameLine();
		if (ImGui::Button(dynamicSVOMode ? "Stop Dynamic" : "Dynamic SVO")) {
			dynamicSVOMode = !dynamicSVOMode;
			if (dynamicSVOMode) {
				svoResolution = 64;
				numCones = 4;
			}
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
		ImGui::SliderFloat("Specular Boost", &specularBoost, 0.0f, 2.5f);
		ImGui::SliderFloat("Shininess (Ns)", &phongNs, 1.0f, 512.0f);

		ImGui::Separator();
		if (ImGui::DragFloat3("Light Position", lightPos, 0.5f)) {
			if (lightViewMap) lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f, 0.0f, 0.0f));
		}

		ImGui::Separator();
		ImGui::Text("Dynamic Objects (%d)", (int)dynObjects.size());
		ImGui::SliderFloat("Cube Scale", &cubeScale, 1.0f, 20.0f);
		if (ImGui::Button("Spawn Object")) {
			float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
			int idx = (int)dynObjects.size() % 6;
			dynObjects.push_back({ kSpawnPositions[idx], t, randomBrightColor() });
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			dynObjects.clear();
		}

		ImGui::Separator();
		ImGui::Text("Benchmark");
		if (benchState == BenchState::Idle) {
			if (ImGui::Button("Run Benchmark")) {
				benchResults.clear();
				benchConfigIdx = 0;
				applyBenchConfig(0);
				benchState = BenchState::Warmup;
				benchTimer = 0.0;
			}
		} else if (benchState == BenchState::Done) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Complete! See benchmark_results.csv");
			if (ImGui::BeginTable("benchTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("Config");
				ImGui::TableSetupColumn("Build(ms)");
				ImGui::TableSetupColumn("Inj(ms)");
				ImGui::TableSetupColumn("Filt(ms)");
				ImGui::TableSetupColumn("Fwd(ms)");
				ImGui::TableSetupColumn("FPS");
				ImGui::TableHeadersRow();
				for (const auto& r : benchResults) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::Text("%s", r.cfg.label);
					ImGui::TableNextColumn(); ImGui::Text("%.0f", r.buildMs);
					ImGui::TableNextColumn(); ImGui::Text("%.2f", r.injMs);
					ImGui::TableNextColumn(); ImGui::Text("%.2f", r.filtMs);
					ImGui::TableNextColumn(); ImGui::Text("%.2f", r.fwdMs);
					ImGui::TableNextColumn(); ImGui::Text("%.1f", r.fps);
				}
				ImGui::EndTable();
			}
			if (ImGui::Button("Reset")) {
				benchResults.clear();
				benchState    = BenchState::Idle;
				svoResolution = 128;
				numCones      = 6;
				dynObjects.clear();
			}
		} else {
			const char* stateStr = (benchState == BenchState::Warmup) ? "Warmup" : "Measuring";
			double totalSecs = (benchState == BenchState::Warmup) ? kWarmupSecs : kMeasureSecs;
			ImGui::Text("%s  cfg %d/%d  (%.1fs/%.1fs)", stateStr,
				benchConfigIdx + 1, kNumBenchConfigs, benchTimer, totalSecs);
			ImGui::Text("  %s", kBenchConfigs[benchConfigIdx].label);
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void initCubeAssets() {
    const int stride = 14 * sizeof(float);

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVerts), kCubeVerts, GL_STATIC_DRAW);
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

    // Always bind whiteTex so both the forward and light-view shaders sample a neutral base
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    s->setInt("ourTexture", 0);
    if (!modelOnly) {
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
        m = glm::scale(m, glm::vec3(cubeScale));
        glUniformMatrix4fv(glGetUniformLocation(s->ID, "model"), 1, GL_FALSE, glm::value_ptr(m));
        s->setVec3("Kd", obj.color); // always set — both forward and light-view shaders use Kd
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    s->setVec3("Kd", glm::vec3(1.0f)); // reset to white for subsequent draws
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


void applyBenchConfig(int idx) {
    const BenchConfig& cfg = kBenchConfigs[idx];
    svoResolution = cfg.svoRes;
    numCones      = cfg.numCones;
    displayMode   = 0; // always Final mode for timing

    if (cfg.shadowRes != shadowMapRes) {
        shadowMapRes = cfg.shadowRes;
        delete lightViewMap;
        lightViewMap = new LightViewMap(shadowMapRes, shadowMapRes);
        lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f));
    }

    dynObjects.clear();
    for (int i = 0; i < cfg.numDynObjs; i++)
        dynObjects.push_back({ kSpawnPositions[i % 6], 0.0f, randomBrightColor() });

    if (sponza && svo) {
        auto t0 = std::chrono::high_resolution_clock::now();
        SVOBuilder::build(sponza, cfg.svoRes, *svo);
        auto t1 = std::chrono::high_resolution_clock::now();
        svoBuildTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        svo->initGPU();
    }
}

void updateBenchmark() {
    if (benchState == BenchState::Idle || benchState == BenchState::Done) return;

    benchTimer += (double)delta;

    if (benchState == BenchState::Warmup) {
        if (benchTimer >= kWarmupSecs) {
            benchState     = BenchState::Measure;
            benchTimer     = 0.0;
            benchFpsAccum  = 0.0;
            benchInjAccum  = 0.0;
            benchFiltAccum = 0.0;
            benchFwdAccum  = 0.0;
            benchSamples   = 0;
        }
    } else if (benchState == BenchState::Measure) {
        if (delta > 0.0f) benchFpsAccum += 1.0 / delta;
        benchInjAccum  += gpuTimeInjection;
        benchFiltAccum += gpuTimeFiltering;
        benchFwdAccum  += gpuTimeForward;
        benchSamples++;

        if (benchTimer >= kMeasureSecs) {
            BenchResult r;
            r.cfg    = kBenchConfigs[benchConfigIdx];
            r.buildMs = svoBuildTimeMs;
            r.injMs  = benchSamples > 0 ? benchInjAccum  / benchSamples : 0.0;
            r.filtMs = benchSamples > 0 ? benchFiltAccum / benchSamples : 0.0;
            r.fwdMs  = benchSamples > 0 ? benchFwdAccum  / benchSamples : 0.0;
            r.fps    = benchSamples > 0 ? benchFpsAccum  / benchSamples : 0.0;
            benchResults.push_back(r);

            benchConfigIdx++;
            if (benchConfigIdx >= kNumBenchConfigs) {
                benchState = BenchState::Done;
                // Write CSV
                std::ofstream f("benchmark_results.csv");
                f << "Config,SVO Res,Num Cones,Dyn Objects,Shadow Res,Build (ms),Inject (ms),Filter (ms),Forward (ms),FPS\n";
                for (const auto& res : benchResults)
                    f << res.cfg.label << "," << res.cfg.svoRes << "," << res.cfg.numCones << ","
                      << res.cfg.numDynObjs << "," << res.cfg.shadowRes << "," << res.buildMs << "," << res.injMs << ","
                      << res.filtMs << "," << res.fwdMs << "," << res.fps << "\n";
                f.close();
                std::cout << "[Benchmark] Done. Results in benchmark_results.csv\n";
            } else {
                applyBenchConfig(benchConfigIdx);
                benchState = BenchState::Warmup;
                benchTimer = 0.0;
            }
        }
    }
}

void display() {
	// --- DYNAMIC SVO REBUILD ---
	if (dynamicSVOMode && sponza && svo) {
		// Build world-space triangles from each dynamic cube
		std::vector<SVOBuilder::ExtraTriangle> extraTris;
		{
			float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
			const int stride = 14; // floats per vertex
			for (const auto& obj : dynObjects) {
				float elapsed = t - obj.spawnTime;
				glm::vec3 pos = obj.basePos + glm::vec3(0.0f, 1.5f * std::sin(elapsed * 1.2f), 0.0f);
				glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
				m = glm::rotate(m, elapsed * 0.8f, glm::vec3(0.3f, 1.0f, 0.2f));
				m = glm::scale(m, glm::vec3(2.0f));
				glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(m)));
				for (int tri = 0; tri < 12; tri++) { // 12 triangles = 36 vertices / 3
					SVOBuilder::ExtraTriangle et;
					for (int v = 0; v < 3; v++) {
						const float* vp = &kCubeVerts[(tri * 3 + v) * stride];
						glm::vec4 wpos = m * glm::vec4(vp[0], vp[1], vp[2], 1.0f);
						et.v[v] = glm::vec3(wpos);
					}
					// Normal from first vertex of the triangle (all three share the same face normal)
					const float* np = &kCubeVerts[tri * 3 * stride + 3];
					et.normal = glm::normalize(normalMat * glm::vec3(np[0], np[1], np[2]));
					et.color = obj.color;
					extraTris.push_back(et);
				}
			}
		}

		auto _t0 = std::chrono::high_resolution_clock::now();
		SVOBuilder::build(sponza, svoResolution, *svo, extraTris);
		auto _t1 = std::chrono::high_resolution_clock::now();
		svoBuildTimeMs = std::chrono::duration<double, std::milli>(_t1 - _t0).count();
		svo->initGPU();
	}

	// --- LIGHT-VIEW MAP PASS ---
	if (lightViewMap && lightViewShader) {
		lightViewMap->bindForWriting();
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		// Disable face culling so back-facing triangles write depth.
		// Without this, interior walls (whose normals face inward) are culled,
		// leaving shadow map texels at 1.0 — making voxels behind those walls appear lit.
		glDisable(GL_CULL_FACE);

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
			glBeginQuery(GL_TIME_ELAPSED, timerQueries[0]);
			glDispatchCompute(numGroups, 1, 1);
			glEndQuery(GL_TIME_ELAPSED);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			GLuint64 injElapsed = 0;
			glGetQueryObjectui64v(timerQueries[0], GL_QUERY_RESULT, &injElapsed);
			gpuTimeInjection = injElapsed * 1e-6;
		}

		// --- RADIANCE FILTERING PASS (MIB-MAPPING) ---
		if (svo && !svo->levelOffsets.empty() && radianceFilteringShader) {
			radianceFilteringShader->use();
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, svo->nodeSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svo->radianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, svo->filteredRadianceSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, svo->neighborSSBO);

			// Bottom-up pass
			glBeginQuery(GL_TIME_ELAPSED, timerQueries[1]);
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
			glEndQuery(GL_TIME_ELAPSED);
			GLuint64 filtElapsed = 0;
			glGetQueryObjectui64v(timerQueries[1], GL_QUERY_RESULT, &filtElapsed);
			gpuTimeFiltering = filtElapsed * 1e-6;
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
		shader->setFloat("Ns", phongNs);
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

		glBeginQuery(GL_TIME_ELAPSED, timerQueries[2]);
		if (sponza) sponza->Draw();
		drawCubes(shader);
		glEndQuery(GL_TIME_ELAPSED);
		GLuint64 fwdElapsed = 0;
		glGetQueryObjectui64v(timerQueries[2], GL_QUERY_RESULT, &fwdElapsed);
		gpuTimeForward = fwdElapsed * 1e-6;
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
		
		if (svo && svo->colorSSBO) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svo->colorSSBO);
		}
		
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

	updateBenchmark();
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
	shader = new Shader("shaders/forward_vs.glsl", "shaders/forward_fs.glsl");
	gShader = new Shader("shaders/gbuffer_vs.glsl", "shaders/gbuffer_fs.glsl");
	rectShader = new Shader("shaders/rect_vs.glsl", "shaders/rect_fs.glsl");
	svoShader = new Shader("shaders/rect_vs.glsl", "shaders/svo_raycast_fs.glsl");
	lightViewShader = new Shader("shaders/light_view_vs.glsl", "shaders/light_view_fs.glsl");
	radianceShader = new Shader("shaders/radiance_injection.glsl");
	radianceFilteringShader = new Shader("shaders/radiance_filtering.glsl");
	gBuffer = new GBuffer(width, height);
	lightViewMap = new LightViewMap(1024, 1024);

	// Light position initialization (shining down from above)
	lightPos[0] = 5.0f; lightPos[1] = 25.0f; lightPos[2] = 2.0f;
	lightViewMap->updateMatrices(glm::vec3(lightPos[0], lightPos[1], lightPos[2]), glm::vec3(0.0f, 0.0f, 0.0f));

	// Sponza
	std::cout << "Loading Sponza..." << std::endl;
	sponza = new Model("sponza/Sponza.gltf", glm::vec3(0.0f, 0.0f, 0.0f), shader);
	sponza->model = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

	// GPU timer queries
	glGenQueries(3, timerQueries);

	// SVO
	svo = new SVO();
	if (sponza) {
		auto _t0 = std::chrono::high_resolution_clock::now();
		SVOBuilder::build(sponza, svoResolution, *svo);
		auto _t1 = std::chrono::high_resolution_clock::now();
		svoBuildTimeMs = std::chrono::duration<double, std::milli>(_t1 - _t0).count();
	}
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
	if (timerQueries[0]) glDeleteQueries(3, timerQueries);
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

