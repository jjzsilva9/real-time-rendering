#pragma once

// Standard library - only what's needed for declarations
#include <string>
#include <vector>

// Forward declare GL type
typedef unsigned int GLuint;

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TextureCoords;
	glm::vec3 Tangent;
	glm::vec3 Bitangent;
};

struct Material {
	glm::vec3 Kd; // Diffuse
	glm::vec3 Ks; // Specular
	glm::vec3 Ka; // Ambient
	float Ns; // Specular Exponent
};

struct Texture {
 unsigned int id;
    std::string type;
	aiString path;
	Material material;
};


class Mesh {
public:
	std::vector<Vertex>       vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture>      textures;
	
	Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures, Shader* shader);

	void Draw(glm::mat4 model);
private:
	unsigned int VAO, VBO, EBO;
	GLuint shaderProgramID;
	Shader* shader;

	void setupMesh();
};