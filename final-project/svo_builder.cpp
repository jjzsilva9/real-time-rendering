#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "svo_builder.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>

struct TempNode {
    bool isLeaf = false;
    TempNode* children[8] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    SVOBuilder::VoxelData data;
};

void deleteTempTree(TempNode* node) {
    if (!node) return;
    for (int i = 0; i < 8; i++) deleteTempTree(node->children[i]);
    delete node;
}

bool planeBoxOverlap(const glm::vec3& normal, const glm::vec3& vert, const glm::vec3& maxbox) {
    glm::vec3 vmin, vmax;
    for (int q = 0; q <= 2; q++) {
        float v = vert[q];
        if (normal[q] > 0.0f) {
            vmin[q] = -maxbox[q] - v;
            vmax[q] = maxbox[q] - v;
        } else {
            vmin[q] = maxbox[q] - v;
            vmax[q] = -maxbox[q] - v;
        }
    }
    if (glm::dot(normal, vmin) > 0.0f) return false;
    if (glm::dot(normal, vmax) >= 0.0f) return true;
    return false;
}

#define AXISTEST_X01(a, b, fa, fb)			   \
	p0 = a * v0.y - b * v0.z;			       \
	p2 = a * v2.y - b * v2.z;			       \
    if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z;   \
	if(min>rad || max<-rad) return false;

#define AXISTEST_X2(a, b, fa, fb)			   \
	p0 = a * v0.y - b * v0.z;			       \
	p1 = a * v1.y - b * v1.z;			       \
    if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z;   \
	if(min>rad || max<-rad) return false;

#define AXISTEST_Y02(a, b, fa, fb)			   \
	p0 = -a * v0.x + b * v0.z;		      \
	p2 = -a * v2.x + b * v2.z;		      \
    if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.z;   \
	if(min>rad || max<-rad) return false;

#define AXISTEST_Y1(a, b, fa, fb)			   \
	p0 = -a * v0.x + b * v0.z;		      \
	p1 = -a * v1.x + b * v1.z;		      \
    if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.z;   \
	if(min>rad || max<-rad) return false;

#define AXISTEST_Z12(a, b, fa, fb)			   \
	p1 = a * v1.x - b * v1.y;			       \
	p2 = a * v2.x - b * v2.y;			       \
    if(p1<p2) {min=p1; max=p2;} else {min=p2; max=p1;} \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y;   \
	if(min>rad || max<-rad) return false;

#define AXISTEST_Z0(a, b, fa, fb)			   \
	p0 = a * v0.x - b * v0.y;				   \
	p1 = a * v1.x - b * v1.y;				   \
    if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y;   \
	if(min>rad || max<-rad) return false;

bool triBoxOverlap(const glm::vec3& boxcenter, const glm::vec3& boxhalfsize, const glm::vec3 triverts[3]) {
    glm::vec3 v0 = triverts[0] - boxcenter;
    glm::vec3 v1 = triverts[1] - boxcenter;
    glm::vec3 v2 = triverts[2] - boxcenter;

    glm::vec3 e0 = v1 - v0;
    glm::vec3 e1 = v2 - v1;
    glm::vec3 e2 = v0 - v2;

    float min, max, p0, p1, p2, rad;
    float fex = fabsf(e0.x); float fey = fabsf(e0.y); float fez = fabsf(e0.z);
    AXISTEST_X01(e0.z, e0.y, fez, fey);
    AXISTEST_Y02(e0.z, e0.x, fez, fex);
    AXISTEST_Z12(e0.y, e0.x, fey, fex);

    fex = fabsf(e1.x); fey = fabsf(e1.y); fez = fabsf(e1.z);
    AXISTEST_X01(e1.z, e1.y, fez, fey);
    AXISTEST_Y02(e1.z, e1.x, fez, fex);
    AXISTEST_Z0(e1.y, e1.x, fey, fex);

    fex = fabsf(e2.x); fey = fabsf(e2.y); fez = fabsf(e2.z);
    AXISTEST_X2(e2.z, e2.y, fez, fey);
    AXISTEST_Y1(e2.z, e2.x, fez, fex);
    AXISTEST_Z12(e2.y, e2.x, fey, fex);

    min = (std::min)((std::min)(v0.x, v1.x), v2.x); max = (std::max)((std::max)(v0.x, v1.x), v2.x);
    if (min > boxhalfsize.x || max < -boxhalfsize.x) return false;
    min = (std::min)((std::min)(v0.y, v1.y), v2.y); max = (std::max)((std::max)(v0.y, v1.y), v2.y);
    if (min > boxhalfsize.y || max < -boxhalfsize.y) return false;
    min = (std::min)((std::min)(v0.z, v1.z), v2.z); max = (std::max)((std::max)(v0.z, v1.z), v2.z);
    if (min > boxhalfsize.z || max < -boxhalfsize.z) return false;

    glm::vec3 normal = glm::cross(e0, e1);
    if (!planeBoxOverlap(normal, v0, boxhalfsize)) return false;

    return true;
}

void SVOBuilder::build(Model* model, int resolution, SVO& outSvo) {
    outSvo.clear();
    
    glm::vec3 minB(-100.0f, -20.0f, -100.0f);
    float size = 200.0f; 
    float voxelW = size / (float)resolution;
    glm::vec3 boxHalfSize(voxelW * 0.5f);
    
    std::cout << "[SVO] Building Resolution: " << resolution << "^3..." << std::endl;

    std::vector<VoxelData> denseGrid(resolution * resolution * resolution);

    for (auto& mesh : model->meshes) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            glm::vec3 tri[3];
            for (int j = 0; j < 3; j++) tri[j] = glm::vec3(model->model * glm::vec4(mesh.vertices[mesh.indices[i + j]].Position, 1.0f));
            
            glm::vec3 tMin = glm::min(tri[0], glm::min(tri[1], tri[2]));
            glm::vec3 tMax = glm::max(tri[0], glm::max(tri[1], tri[2]));

            int minX = (std::max)(0, (int)((tMin.x - minB.x) / voxelW));
            int minY = (std::max)(0, (int)((tMin.y - minB.y) / voxelW));
            int minZ = (std::max)(0, (int)((tMin.z - minB.z) / voxelW));
            int maxX = (std::min)(resolution - 1, (int)((tMax.x - minB.x) / voxelW));
            int maxY = (std::min)(resolution - 1, (int)((tMax.y - minB.y) / voxelW));
            int maxZ = (std::min)(resolution - 1, (int)((tMax.z - minB.z) / voxelW));

            for (int vz = minZ; vz <= maxZ; vz++) {
                for (int vy = minY; vy <= maxY; vy++) {
                    for (int vx = minX; vx <= maxX; vx++) {
                        int idx = vx + (vy * resolution) + (vz * resolution * resolution);
                        if (denseGrid[idx].occupied) continue; 

                        glm::vec3 boxCenter = minB + glm::vec3(vx, vy, vz) * voxelW + boxHalfSize;
                        if (triBoxOverlap(boxCenter, boxHalfSize, tri)) {
                            denseGrid[idx] = { true, glm::vec3(1.0f), glm::normalize(glm::cross(tri[1] - tri[0], tri[2] - tri[0])) };
                        }
                    }
                }
            }
        }
    }

    // Tree Construction
    TempNode* root = new TempNode();
    int maxDepth = (int)std::log2(resolution);
    int uniqueVoxels = 0;

    for (int vz = 0; vz < resolution; vz++) {
        for (int vy = 0; vy < resolution; vy++) {
            for (int vx = 0; vx < resolution; vx++) {
                int gIdx = vx + (vy * resolution) + (vz * resolution * resolution);
                if (!denseGrid[gIdx].occupied) continue;
                uniqueVoxels++;

                TempNode* curr = root;
                for (int d = maxDepth - 1; d >= 0; d--) {
                    int childIdx = ((vx >> d) & 1) | (((vy >> d) & 1) << 1) | (((vz >> d) & 1) << 2);
                    if (!curr->children[childIdx]) curr->children[childIdx] = new TempNode();
                    curr = curr->children[childIdx];
                    if (d == 0) { 
                        curr->isLeaf = true; 
                        curr->data = denseGrid[gIdx]; 
                    }
                }
            }
        }
    }

    std::cout << "[SVO] Unique voxels: " << uniqueVoxels << ". Serializing..." << std::endl;

    std::queue<std::pair<TempNode*, uint32_t>> queue;
    outSvo.nodes.push_back({0, 0}); 
    queue.push({root, 0});

    while (!queue.empty()) {
        std::pair<TempNode*, uint32_t> front = queue.front(); 
        queue.pop();
        TempNode* parentTemp = front.first;
        uint32_t poolIdx = front.second;

        uint8_t valid = 0, leaf = 0;
        std::vector<TempNode*> nonLeafChildren;
        for (int i = 0; i < 8; i++) {
            if (parentTemp->children[i]) {
                valid |= (1 << i);
                if (parentTemp->children[i]->isLeaf) {
                    leaf |= (1 << i);
                    outSvo.colors.push_back(0xFFFFFFFF); 
                    outSvo.normals.push_back(parentTemp->children[i]->data.normal);
                } else {
                    nonLeafChildren.push_back(parentTemp->children[i]);
                }
            }
        }

        if (!nonLeafChildren.empty()) {
            uint32_t childBlockStart = (uint32_t)outSvo.nodes.size();
            outSvo.nodes[poolIdx].topology = ChildDescriptor::makeTopology(valid, leaf);
            outSvo.nodes[poolIdx].child_ptr = childBlockStart;
            for (auto* child : nonLeafChildren) {
                uint32_t cIdx = (uint32_t)outSvo.nodes.size();
                outSvo.nodes.push_back({0, 0});
                queue.push({child, cIdx});
            }
        } else {
            outSvo.nodes[poolIdx].topology = ChildDescriptor::makeTopology(valid, leaf);
            outSvo.nodes[poolIdx].child_ptr = 0;
        }
    }
    std::cout << "[SVO] Final Nodes: " << outSvo.nodes.size() << std::endl;
    deleteTempTree(root);
}
