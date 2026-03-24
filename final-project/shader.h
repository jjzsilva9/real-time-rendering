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

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader {
public:
	GLuint ID;
	
	Shader(const char* vertexPath, const char* fragmentPath) {
		ID = glCreateProgram();
		if (ID == 0) {
			std::cerr << "Error creating shader program..." << std::endl;
			exit(1);
		}

		AddShader(ID, vertexPath, GL_VERTEX_SHADER);
		AddShader(ID, fragmentPath, GL_FRAGMENT_SHADER);
		linkProgram();
	}

	Shader(const char* vertexPath, const char* geometryPath, const char* fragmentPath) {
		ID = glCreateProgram();
		if (ID == 0) {
			std::cerr << "Error creating shader program..." << std::endl;
			exit(1);
		}

		AddShader(ID, vertexPath, GL_VERTEX_SHADER);
		AddShader(ID, geometryPath, GL_GEOMETRY_SHADER);
		AddShader(ID, fragmentPath, GL_FRAGMENT_SHADER);
		linkProgram();
	}

	Shader(const char* computePath) {
		ID = glCreateProgram();
		if (ID == 0) {
			std::cerr << "Error creating shader program..." << std::endl;
			exit(1);
		}

		AddShader(ID, computePath, GL_COMPUTE_SHADER);
		linkProgram();
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

	void setVec3(const std::string& name, const glm::vec3& value) const {
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
	}

	void setMat4(const std::string& name, const glm::mat4& value) const {
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
	}

	void bindSSBO(GLuint buffer, GLuint bindingPoint) const {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, buffer);
	}

	void bindAtomicCounter(GLuint buffer, GLuint bindingPoint) const {
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, bindingPoint, buffer);
	}

	void dispatch(GLuint groupsX, GLuint groupsY, GLuint groupsZ) const {
		glDispatchCompute(groupsX, groupsY, groupsZ);
	}

	void waitMemory(GLbitfield barriers) const {
		glMemoryBarrier(barriers);
	}

private:
	void linkProgram() {
		GLint Success = 0;
		GLchar ErrorLog[1024] = { '\0' };

		glLinkProgram(ID);
		glGetProgramiv(ID, GL_LINK_STATUS, &Success);
		if (Success == 0) {
			glGetProgramInfoLog(ID, sizeof(ErrorLog), NULL, ErrorLog);
			std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
			exit(1);
		}

		glValidateProgram(ID);
		glGetProgramiv(ID, GL_VALIDATE_STATUS, &Success);
		if (!Success) {
			glGetProgramInfoLog(ID, sizeof(ErrorLog), NULL, ErrorLog);
			std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
			exit(1);
		}
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