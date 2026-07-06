#ifndef SVO_BUILDER_H
#define SVO_BUILDER_H

#include "svo.h"
#include "model.h"
#include <glm/glm.hpp>
#include <vector>

class SVOBuilder {
public:
    struct VoxelData {
        bool occupied = false;
        glm::vec3 color = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f); // also used as ESVO contour normal
        glm::vec3 position = glm::vec3(0.0f);
        float planeDistance = 0.0f; // ESVO Contour
    };

    // World-space triangle with pre-computed color and normal, used for dynamic objects.
    struct ExtraTriangle {
        glm::vec3 v[3];
        glm::vec3 color;
        glm::vec3 normal;
    };

    /**
     * Builds an SVO from a Model, optionally including additional world-space triangles
     * (e.g. dynamic objects whose geometry is not in the model).
     */
    static void build(Model* model, int resolution, SVO& outSvo,
                      const std::vector<ExtraTriangle>& extraTris = {});
};

#endif
