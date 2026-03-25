#ifndef SVO_H
#define SVO_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <stdint.h>

/**
 * Part 1: [valid_mask:8][leaf_mask:8][child_ptr:15][far:1]
 * Part 2: [contour_mask:8][contour_ptr:24]
 */
struct ChildDescriptor {
    uint32_t topology; // [valid_mask:8][leaf_mask:8][padding:16]
    uint32_t child_ptr; // 32-bit absolute pointer to child block
    
    static uint32_t makeTopology(uint8_t valid, uint8_t leaf) {
        uint32_t res = 0;
        res |= (uint32_t)valid << 24;
        res |= (uint32_t)leaf << 16;
        return res;
    }
};

class SVO {
public:
    std::vector<ChildDescriptor> nodes;
    std::vector<uint32_t> colors;  // RGBA8
    std::vector<glm::vec3> normals; // RGB32F (for now, uncompressed)

    GLuint nodeSSBO = 0;
    GLuint colorSSBO = 0;
    GLuint normalSSBO = 0;

    SVO() {}

    void initGPU() {
        if (nodeSSBO) glDeleteBuffers(1, &nodeSSBO);
        if (colorSSBO) glDeleteBuffers(1, &colorSSBO);
        if (normalSSBO) glDeleteBuffers(1, &normalSSBO);

        glGenBuffers(1, &nodeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(ChildDescriptor), nodes.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &colorSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, colorSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, colors.size() * sizeof(uint32_t), colors.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &normalSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, normalSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void clear() {
        nodes.clear();
        colors.clear();
        normals.clear();
    }

    ~SVO() {
        if (nodeSSBO) glDeleteBuffers(1, &nodeSSBO);
        if (colorSSBO) glDeleteBuffers(1, &colorSSBO);
        if (normalSSBO) glDeleteBuffers(1, &normalSSBO);
    }
};

#endif
