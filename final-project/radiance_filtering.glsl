#version 430 core

layout(local_size_x = 256) in;

struct ChildDescriptor {
    uint topology;
    uint child_ptr;
    uint leaf_ptr;
    uint contour_ptr;
};

layout(std430, binding = 0) buffer NodePool {
    ChildDescriptor nodes[];
};

layout(std430, binding = 1) buffer LeafRadiancePool {
    vec4 leafRadiance[];
};

layout(std430, binding = 2) buffer FilteredRadiancePool {
    vec4 filteredRadiance[];
};

uniform uint levelStart;
uniform uint levelSize;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= levelSize) return;

    uint nodeIdx = levelStart + idx;
    uint topo = nodes[nodeIdx].topology;
    uint childPtr = nodes[nodeIdx].child_ptr;
    uint leafPtr = nodes[nodeIdx].leaf_ptr;
    
    uint validMask = (topo >> 24) & 0xFFu;
    uint leafMask = (topo >> 16) & 0xFFu;
    
    vec4 sumRadiance = vec4(0.0);
    float count = 0.0;
    
    uint internalOffset = 0;
    uint leafOffset = 0;
    
    for (int i = 0; i < 8; i++) {
        if (((validMask >> i) & 1u) != 0u) {
            if (((leafMask >> i) & 1u) != 0u) {
                sumRadiance += leafRadiance[leafPtr + leafOffset];
                leafOffset++;
            } else {
                sumRadiance += filteredRadiance[childPtr + internalOffset];
                internalOffset++;
            }
            count += 1.0;
        }
    }
    
    // Always divide by 8.0 to correctly mip-map the density/opacity of the volume
    filteredRadiance[nodeIdx] = sumRadiance / 8.0;
}
