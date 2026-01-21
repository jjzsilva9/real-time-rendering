#pragma once

// Standard library
#include <string>
#include <vector>
#include <math.h>

// Project includes - needed for definitions
#include "maths_funcs.h"
#include "shader.h"

class DirectionalLight {
public:
	vec3 position;
	vec3 diffuse;
	vec3 specular;
	vec3 ambient;
	bool dayCycle;
	float timeOfDay;
	GLuint shaderProgramID;

	DirectionalLight(vec4 position, vec3 diffuse, vec3 specular, vec3 ambient, GLuint shaderProgramID, bool dayCycle = false);
	void Draw(float deltaTime);
	
private:
	
	float cycleDuration;
	void Update(float deltaTime);
	vec3 getLightColor(float time);
	vec3 getAmbientColor(float time);
	vec3 getLightPosition(float time);

};