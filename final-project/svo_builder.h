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
        glm::vec3 normal = glm::vec3(0.0f);
    };

    /**
     * Builds an SVO from a Model.
     * Starts with a dense voxelization (on CPU) and then hierarchically builds the octree.
     */
    static void build(Model* model, int resolution, SVO& outSvo);
};

#endif
