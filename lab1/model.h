#pragma once

// Standard library
#include <string>
#include <vector>
#include <math.h>

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
#include "maths_funcs.h"
#include "mesh.h"  // Added for Texture struct
#include "shader.h"

class Model {
public:
	mat4 model;
	Material material;
	std::vector<Mesh> meshes;

	Model(const char* path, vec3 position, Shader* shader);
	void Draw();
	void translate(vec3 offset);
	void rotate(vec3 offset);
	void changeMeshMaterials();

private:
	
	std::string directory;
	static std::vector<Texture> textures_loaded;
	GLuint shaderProgramID;
	Shader* shader;
	void loadModel(const char* file_name);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
};