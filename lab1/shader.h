#ifndef SHADER_H
#define SHADER_H

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
#include "maths_funcs.h"

class Shader {
public:
	GLuint ID;
	
	Shader(const char* vertexPath, const char* fragmentPath) {
		//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
		ID = glCreateProgram();
		if (ID == 0) {
			std::cerr << "Error creating shader program..." << std::endl;
			std::cerr << "Press enter/return to exit..." << std::endl;
			std::cin.get();
			exit(1);
		}

		// Create two shader objects, one for the vertex, and one for the fragment shader
		AddShader(ID, vertexPath, GL_VERTEX_SHADER);
		AddShader(ID, fragmentPath, GL_FRAGMENT_SHADER);

		GLint Success = 0;
		GLchar ErrorLog[1024] = { '\0' };
		// After compiling all shader objects and attaching them to the program, we can finally link it
		glLinkProgram(ID);
		// check for program related errors using glGetProgramiv
		glGetProgramiv(ID, GL_LINK_STATUS, &Success);
		if (Success == 0) {
			glGetProgramInfoLog(ID, sizeof(ErrorLog), NULL, ErrorLog);
			std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
			std::cerr << "Press enter/return to exit..." << std::endl;
			std::cin.get();
			exit(1);
		}

		// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
		glValidateProgram(ID);
		// check for program related errors using glGetProgramiv
		glGetProgramiv(ID, GL_VALIDATE_STATUS, &Success);
		if (!Success) {
			glGetProgramInfoLog(ID, sizeof(ErrorLog), NULL, ErrorLog);
			std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
			std::cerr << "Press enter/return to exit..." << std::endl;
			std::cin.get();
			exit(1);
		}
	}

	void use() {
		glUseProgram(ID);
	};

	void setBool(const std::string& name, bool value) const {
		glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
	}

	void setInt(const std::string& name, int value) const {
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}

	void setFloat(const std::string& name, float value) const {
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}

private:
	
	char* readShaderSource(const char* shaderFile) {
		FILE* fp;
		fopen_s(&fp, shaderFile, "rb");

		if (fp == NULL) { return NULL; }

		fseek(fp, 0L, SEEK_END);
		long size = ftell(fp);

		fseek(fp, 0L, SEEK_SET);
		char* buf = new char[size + 1];
		fread(buf, 1, size, fp);
		buf[size] = '\0';

		fclose(fp);

		return buf;
	}

	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
	{
		// create a shader object
		GLuint ShaderObj = glCreateShader(ShaderType);

		if (ShaderObj == 0) {
			std::cerr << "Error creating shader..." << std::endl;
			std::cerr << "Press enter/return to exit..." << std::endl;
			std::cin.get();
			exit(1);
		}
		const char* pShaderSource = readShaderSource(pShaderText);

		// Bind the source code to the shader, this happens before compilation
		glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
		// compile the shader and check for errors
		glCompileShader(ShaderObj);
		GLint success;
		// check for shader related errors using glGetShaderiv
		glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
		if (!success) {
			GLchar InfoLog[1024] = { '\0' };
			glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
			std::cerr << "Error compiling "
				<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
				<< " shader program: " << InfoLog << std::endl;
			std::cerr << "Press enter/return to exit..." << std::endl;
			std::cin.get();
			exit(1);
		}
		// Attach the compiled shader object to the program object
		glAttachShader(ShaderProgram, ShaderObj);
	}
};

#endif