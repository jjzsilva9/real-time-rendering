#pragma once

// Standard library
#include <string>
#include <vector>
#include <math.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// Project includes - needed for definitions
#include "shader.h"

class DirectionalLight {
public:
	glm::vec3 position;
	glm::vec3 diffuse;
	glm::vec3 specular;
	glm::vec3 ambient;
	bool dayCycle;
	float timeOfDay;
	GLuint shaderProgramID;

	DirectionalLight(glm::vec4 position, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 ambient, GLuint shaderProgramID, bool dayCycle = false);
	void Draw(float deltaTime);
	
private:
	
	float cycleDuration;
	void Update(float deltaTime);
	glm::vec3 getLightColor(float time);
	glm::vec3 getAmbientColor(float time);
	glm::vec3 getLightPosition(float time);

};