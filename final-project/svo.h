#ifndef SVO_H
#define SVO_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <stdint.h>

/**
 * ChildDescriptor
 * Contains topology masks and pointers to child blocks and leaf data.
 */
struct ChildDescriptor {
    uint32_t topology;  // [valid_mask:8][leaf_mask:8][padding:16]
    uint32_t child_ptr; // Pointer to child block (internal nodes)
    uint32_t leaf_ptr;  // Pointer to leaf data block
    uint32_t contour_ptr; // Pointer to contour data block (New for ESVO)
    
    static uint32_t makeTopology(uint8_t valid, uint8_t leaf) {
        uint32_t res = 0;
        res |= (uint32_t)valid << 24;
        res |= (uint32_t)leaf << 16;
        return res;
    }
};

/**
 * NeighborDescriptor
 * Stores indices to 6 adjacent nodes in the SVO (X+, X-, Y+, Y-, Z+, Z-).
 */
struct NeighborDescriptor {
    uint32_t neighbors[6]; // X+, X-, Y+, Y-, Z+, Z-
};

/**
 * ContourDescriptor
 * Stores Hermite data (plane normal and distance) for surface approximation.
 */
struct ContourDescriptor {
    glm::vec3 normal;
    float distance;
};

class SVO {
public:
    std::vector<ChildDescriptor> nodes;
    std::vector<NeighborDescriptor> neighborPointers;
    std::vector<ContourDescriptor> contours;
    
    std::vector<uint32_t> colors;     // Albedo RGBA8
    std::vector<glm::vec3> normals;    // Normal RGB32F
    std::vector<glm::vec3> positions;  // World Positions of leaf nodes
    std::vector<glm::vec4> radiance;   // Radiance RGBA32F

    // Level map for bottom-up traversal
    std::vector<uint32_t> levelOffsets; 

    GLuint nodeSSBO = 0;
    GLuint neighborSSBO = 0;
    GLuint contourSSBO = 0;
    GLuint colorSSBO = 0;
    GLuint normalSSBO = 0;
    GLuint positionSSBO = 0;
    GLuint radianceSSBO = 0;
    GLuint filteredRadianceSSBO = 0;

    SVO() {}

    void initGPU() {
        if (nodeSSBO) glDeleteBuffers(1, &nodeSSBO);
        if (neighborSSBO) glDeleteBuffers(1, &neighborSSBO);
        if (contourSSBO) glDeleteBuffers(1, &contourSSBO);
        if (colorSSBO) glDeleteBuffers(1, &colorSSBO);
        if (normalSSBO) glDeleteBuffers(1, &normalSSBO);
        if (positionSSBO) glDeleteBuffers(1, &positionSSBO);
        if (radianceSSBO) glDeleteBuffers(1, &radianceSSBO);
        if (filteredRadianceSSBO) glDeleteBuffers(1, &filteredRadianceSSBO);

        glGenBuffers(1, &nodeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(ChildDescriptor), nodes.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &neighborSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, neighborSSBO);
        if (!neighborPointers.empty()) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, neighborPointers.size() * sizeof(NeighborDescriptor), neighborPointers.data(), GL_STATIC_DRAW);
        } else {
            glBufferData(GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(NeighborDescriptor), NULL, GL_STATIC_DRAW);
        }

        glGenBuffers(1, &contourSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, contourSSBO);
        if (!contours.empty()) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, contours.size() * sizeof(ContourDescriptor), contours.data(), GL_STATIC_DRAW);
        } else {
            glBufferData(GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(ContourDescriptor), NULL, GL_STATIC_DRAW);
        }

        glGenBuffers(1, &colorSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, colorSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, colors.size() * sizeof(uint32_t), colors.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &normalSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, normalSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &positionSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &radianceSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, radianceSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, colors.size() * sizeof(glm::vec4), NULL, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &filteredRadianceSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, filteredRadianceSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(glm::vec4), NULL, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void clear() {
        nodes.clear();
        neighborPointers.clear();
        contours.clear();
        colors.clear();
        normals.clear();
        positions.clear();
        radiance.clear();
        levelOffsets.clear();
    }

    ~SVO() {
        if (nodeSSBO) glDeleteBuffers(1, &nodeSSBO);
        if (neighborSSBO) glDeleteBuffers(1, &neighborSSBO);
        if (contourSSBO) glDeleteBuffers(1, &contourSSBO);
        if (colorSSBO) glDeleteBuffers(1, &colorSSBO);
        if (normalSSBO) glDeleteBuffers(1, &normalSSBO);
        if (positionSSBO) glDeleteBuffers(1, &positionSSBO);
        if (radianceSSBO) glDeleteBuffers(1, &radianceSSBO);
        if (filteredRadianceSSBO) glDeleteBuffers(1, &filteredRadianceSSBO);
    }
};

#endif
