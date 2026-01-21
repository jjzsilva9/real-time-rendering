#include "mesh.h"

// Standard library
#include <string>
#include <vector>
#include <math.h>

namespace std {
    using ::sqrt;
    using ::sin;
    using ::acos;
}

// OpenGL
#include <GL/glew.h>
#include <GL/freeglut.h>

// Project includes
#include "shader.h"

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

using namespace std;

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures, Shader* shader) {
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;
	this->shader = shader;
	this->shaderProgramID = shader->ID;
    setupMesh();
}

void Mesh::Draw(mat4 model) {
	Material material;
	for (unsigned int i = 0; i < textures.size(); i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		string name = textures[i].type;
		if (name == "texture_diffuse") {
			glBindTexture(GL_TEXTURE_2D, textures[i].id);
			shader->setInt("ourTexture", 0);
			material = textures[i].material;
		}
		else if (name == "texture_normal") {
			glBindTexture(GL_TEXTURE_2D, textures[i].id);
			shader->setInt("normalMap", 1);
		}
	}

	int diffuse_location = glGetUniformLocation(shaderProgramID, "Kd");
	int specular_location = glGetUniformLocation(shaderProgramID, "Ks");
	int ambient_location = glGetUniformLocation(shaderProgramID, "Ka");
	int exponent_location = glGetUniformLocation(shaderProgramID, "Ns");
	glUniform3fv(diffuse_location, 1, material.Kd.v);
	glUniform3fv(specular_location, 1, material.Ks.v);
	glUniform3fv(ambient_location, 1, material.Ka.v);
	glUniform1fv(exponent_location, 1, &material.Ns);

	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, vertices.size());
	glBindVertexArray(0);

	glActiveTexture(GL_TEXTURE0);
}
    
void Mesh::setupMesh() {

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	GLuint loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	GLuint loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	GLuint loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");
	GLuint loc4 = glGetAttribLocation(shaderProgramID, "vertex_tangent");
	GLuint loc5 = glGetAttribLocation(shaderProgramID, "vertex_bitangent");
	
	glEnableVertexAttribArray(loc1);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

	glEnableVertexAttribArray(loc2);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

	glEnableVertexAttribArray (loc3);
	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TextureCoords));

	glEnableVertexAttribArray(loc4);
	glVertexAttribPointer(loc4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

	glEnableVertexAttribArray(loc5);
	glVertexAttribPointer(loc5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

	glBindVertexArray(0);
	std::cout << "Mesh setup" << "\n";
}