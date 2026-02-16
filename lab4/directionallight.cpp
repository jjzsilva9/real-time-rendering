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
#include "directionallight.h"

DirectionalLight::DirectionalLight(glm::vec4 position, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 ambient, GLuint shaderProgramID, bool dayCycle) {
	this->position = glm::vec3(position);
	this->diffuse = diffuse;
	this->specular = specular;
	this->ambient = ambient;
	this->dayCycle = dayCycle;
	this->shaderProgramID = shaderProgramID;
	this->timeOfDay =0.5f;
	this->cycleDuration =60.0f;
}

void DirectionalLight::Draw(float deltaTime) {
	this->Update(deltaTime);
	int position_location = glGetUniformLocation(shaderProgramID, "LightPosition");
	glUniform3fv(position_location,1, glm::value_ptr(position));

	int diffuse_location = glGetUniformLocation(shaderProgramID, "Ld");
	glUniform3fv(diffuse_location,1, glm::value_ptr(diffuse));

	int specular_location = glGetUniformLocation(shaderProgramID, "Ls");
	glUniform3fv(specular_location,1, glm::value_ptr(specular));

	int ambient_location = glGetUniformLocation(shaderProgramID, "La");
	glUniform3fv(ambient_location,1, glm::value_ptr(ambient));
}

void DirectionalLight::Update(float deltaTime) {
	if (dayCycle) {
		timeOfDay += deltaTime / cycleDuration;
		if (timeOfDay >1.0f) {
			timeOfDay -=1.0f;
		}
	}
	position = getLightPosition(timeOfDay);

diffuse = getLightColor(timeOfDay);
specular = getLightColor(timeOfDay);
ambient = getAmbientColor(timeOfDay);
}

glm::vec3 DirectionalLight::getLightColor(float time) {
	glm::vec3 nightColor = glm::vec3(0.1f,0.1f,0.2f); // Dark blue
	glm::vec3 dawnColor = glm::vec3(1.0f,0.6f,0.3f); // Orange/red
	glm::vec3 dayColor = glm::vec3(1.0f,1.0f,0.95f); // Bright white/yellow
	glm::vec3 duskColor = glm::vec3(1.0f,0.5f,0.2f); // Deep orange

	if (time <0.25f) {
		// Midnight to sunrise (0.0 -0.25)
		float t = time /0.25f;
		return glm::mix(nightColor, dawnColor, t);
	}
	else if (time <0.5f) {
		// Sunrise to noon (0.25 -0.5)
		float t = (time -0.25f) /0.25f;
		return glm::mix(dawnColor, dayColor, t);
	}
	else if (time <0.75f) {
		// Noon to sunset (0.5 -0.75)
		float t = (time -0.5f) /0.25f;
		return glm::mix(dayColor, duskColor, t);
	}
	else {
		// Sunset to midnight (0.75 -1.0)
		float t = (time -0.75f) /0.25f;
		return glm::mix(duskColor, nightColor, t);
	}
}

glm::vec3 DirectionalLight::getAmbientColor(float time) {
	glm::vec3 nightAmbient = glm::vec3(0.15f,0.15f,0.2f); // Very dark blue
	glm::vec3 dayAmbient = glm::vec3(0.5f,0.5f,0.6f); // Light gray/blue

	float angle = time *2.0f *3.14f;
	float sunHeight = sin(angle);
	float t = (sunHeight +1.0f) /2.0f;

	return glm::mix(nightAmbient, dayAmbient, t);
}

glm::vec3 DirectionalLight::getLightPosition(float time) {
	float angle = time *2.0f *3.14f;

	float radius =1000.0f;

	float x = radius * cos(angle);
	float y = radius * sin(angle);
	float z =0.0f;

	return glm::vec3(x, y, z);
}