#ifndef LIGHT_VIEW_MAP_H
#define LIGHT_VIEW_MAP_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

class LightViewMap {
public:
    GLuint fbo;
    GLuint depthMap;
    GLuint radianceMap;
    int width, height;

    glm::mat4 lightView;
    glm::mat4 lightProj;

    LightViewMap(int resWidth = 1024, int resHeight = 1024) : width(resWidth), height(resHeight) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Depth texture (Shadow Map)
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

        // Radiance/Albedo texture
        glGenTextures(1, &radianceMap);
        glBindTexture(GL_TEXTURE_2D, radianceMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, radianceMap, 0);

        // Only one color attachment
        GLuint attachments[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, attachments);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "LightViewMap Framebuffer not complete!" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Initial matrices
        updateMatrices(glm::vec3(0.0f, 20.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    }

    void updateMatrices(glm::vec3 lightPos, glm::vec3 target) {
        float orthoSize = 100.0f; // Full SVO coverage (200x200)
        lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 500.0f);
        lightView = glm::lookAt(lightPos, target, glm::vec3(0.0, 1.0, 0.0));
        
        // If the direction is exactly vertical, up needs to be different
        if (glm::abs(glm::dot(glm::normalize(target - lightPos), glm::vec3(0, 1, 0))) > 0.99f) {
            lightView = glm::lookAt(lightPos, target, glm::vec3(0.0, 0.0, 1.0));
        }
    }

    void bindForWriting() {
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    ~LightViewMap() {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &depthMap);
        glDeleteTextures(1, &radianceMap);
    }
};

#endif
