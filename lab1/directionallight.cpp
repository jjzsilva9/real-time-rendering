#pragma once

// Standard library
#include <string>
#include <vector>
#include <math.h>

// Project includes - needed for definitions
#include "maths_funcs.h"
#include "shader.h"
#include "directionallight.h"

DirectionalLight::DirectionalLight(vec4 position, vec3 diffuse, vec3 specular, vec3 ambient, GLuint shaderProgramID, bool dayCycle) {
	this->position = position;
	this->diffuse = diffuse;
	this->specular = specular;
	this->ambient = ambient;
	this->dayCycle = dayCycle;
	this->shaderProgramID = shaderProgramID;
	this->timeOfDay = 0.5f;
	this->cycleDuration = 60.0f;
}

void DirectionalLight::Draw(float deltaTime) {
	this->Update(deltaTime);
	int position_location = glGetUniformLocation(shaderProgramID, "LightPosition");
	glUniform3fv(position_location, 1, position.v);

	int diffuse_location = glGetUniformLocation(shaderProgramID, "Ld");
	glUniform3fv(diffuse_location, 1, diffuse.v);

	int specular_location = glGetUniformLocation(shaderProgramID, "Ls");
	glUniform3fv(specular_location, 1, specular.v);

	int ambient_location = glGetUniformLocation(shaderProgramID, "La");
	glUniform3fv(ambient_location, 1, ambient.v);
}

void DirectionalLight::Update(float deltaTime) {
	if (dayCycle) {
		timeOfDay += deltaTime / cycleDuration;
		if (timeOfDay > 1.0f) {
			timeOfDay -= 1.0f;
		}
	}
	position = getLightPosition(timeOfDay);

	diffuse = getLightColor(timeOfDay);
	specular = getLightColor(timeOfDay);
	ambient = getAmbientColor(timeOfDay);
}

vec3 DirectionalLight::getLightColor(float time) {
	vec3 nightColor = vec3(0.1f, 0.1f, 0.2f);      // Dark blue
	vec3 dawnColor = vec3(1.0f, 0.6f, 0.3f);       // Orange/red
	vec3 dayColor = vec3(1.0f, 1.0f, 0.95f);       // Bright white/yellow
	vec3 duskColor = vec3(1.0f, 0.5f, 0.2f);       // Deep orange

	if (time < 0.25f) {
		// Midnight to sunrise (0.0 - 0.25)
		float t = time / 0.25f;
		return vec3(
			nightColor.v[0] + t * (dawnColor.v[0] - nightColor.v[0]),
			nightColor.v[1] + t * (dawnColor.v[1] - nightColor.v[1]),
			nightColor.v[2] + t * (dawnColor.v[2] - nightColor.v[2])
		);
	}
	else if (time < 0.5f) {
		// Sunrise to noon (0.25 - 0.5)
		float t = (time - 0.25f) / 0.25f;
		return vec3(
			dawnColor.v[0] + t * (dayColor.v[0] - dawnColor.v[0]),
			dawnColor.v[1] + t * (dayColor.v[1] - dawnColor.v[1]),
			dawnColor.v[2] + t * (dayColor.v[2] - dawnColor.v[2])
		);
	}
	else if (time < 0.75f) {
		// Noon to sunset (0.5 - 0.75)
		float t = (time - 0.5f) / 0.25f;
		return vec3(
			dayColor.v[0] + t * (duskColor.v[0] - dayColor.v[0]),
			dayColor.v[1] + t * (duskColor.v[1] - dayColor.v[1]),
			dayColor.v[2] + t * (duskColor.v[2] - dayColor.v[2])
		);
	}
	else {
		// Sunset to midnight (0.75 - 1.0)
		float t = (time - 0.75f) / 0.25f;
		return vec3(
			duskColor.v[0] + t * (nightColor.v[0] - duskColor.v[0]),
			duskColor.v[1] + t * (nightColor.v[1] - duskColor.v[1]),
			duskColor.v[2] + t * (nightColor.v[2] - duskColor.v[2])
		);
	}
}

vec3 DirectionalLight::getAmbientColor(float time) {
	vec3 nightAmbient = vec3(0.15f, 0.15f, 0.2f);   // Very dark blue
	vec3 dayAmbient = vec3(0.5f, 0.5f, 0.6f);      // Light gray/blue

	float angle = time * 2.0f * 3.14f;
	float sunHeight = sin(angle);
	float t = (sunHeight + 1.0f) / 2.0f;

	return vec3(
		nightAmbient.v[0] + t * (dayAmbient.v[0] - nightAmbient.v[0]),
		nightAmbient.v[1] + t * (dayAmbient.v[1] - nightAmbient.v[1]),
		nightAmbient.v[2] + t * (dayAmbient.v[2] - nightAmbient.v[2])
	);
}

vec3 DirectionalLight::getLightPosition(float time) {
	float angle = time * 2.0f * 3.14f;

	float radius = 1000.0f;

	float x = radius * cos(angle);
	float y = radius * sin(angle);
	float z = 0.0f;

	return vec3(x, y, z);
}