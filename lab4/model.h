#pragma once

// Standard library
#include <string>
#include <vector>
#include <math.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Forward declarations
class Shader;
struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;
enum aiTextureType;

// Only needed for declarations
typedef unsigned int GLuint;

// Project includes - needed for definitions
#include "mesh.h"  // Added for Texture struct
#include "shader.h"

class Model {
public:
	glm::mat4 model;
	Material material;
	std::vector<Mesh> meshes;
	static std::vector<Texture> textures_loaded;

	Model(const char* path, glm::vec3 position, Shader* shader);
	void Draw();
	void translate(glm::vec3 offset);
	void rotate(glm::vec3 offset);
	void changeMeshMaterials();

private:
	
	std::string directory;
	GLuint shaderProgramID;
	Shader* shader;
	void loadModel(const char* file_name);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
};