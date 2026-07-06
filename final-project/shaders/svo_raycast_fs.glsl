#version 430 core

in vec2 TexCoord;
out vec4 FragColor;

struct ChildDescriptor {
    uint topology;
    uint child_ptr;
    uint leaf_ptr;
    uint contour_ptr;
};

struct Neighbors {
    uint n[6];
};

// Contour: vec3 normal (12 bytes) + float distance (4 bytes) = 16 bytes in C++.
// GLSL std430 would pad this to 32 bytes due to vec3's 16-byte base alignment.
// Use a vec4 to match the C++ layout exactly: xyz=normal, w=distance.
// layout(std430, binding = 7) buffer ContourPool reads vec4 contours[] below.

layout(std430, binding = 0) buffer NodePool {
    ChildDescriptor nodes[];
};

layout(std430, binding = 6) buffer NeighborPool {
    Neighbors neighborPointers[];
};

layout(std430, binding = 7) buffer ContourPool {
    vec4 contours[]; // xyz = normal, w = distance; matches C++ Contour (16 bytes, no padding)
};

layout(std430, binding = 1) buffer AlbedoPool {
    uint albedos[];
};

uniform vec3 cameraPos;
uniform mat4 invViewProj;

const int MAX_DEPTH = 12;

bool ray_aabb(vec3 orig, vec3 d, vec3 minB, vec3 maxB, out float tmin, out float tmax) {
    vec3 invDir = 1.0 / (d + vec3(1e-9));
    vec3 t0 = (minB - orig) * invDir;
    vec3 t1 = (maxB - orig) * invDir;
    vec3 tmin_v = min(t0, t1);
    vec3 tmax_v = max(t0, t1);
    tmin = max(max(tmin_v.x, tmin_v.y), tmin_v.z);
    tmax = min(min(tmax_v.x, tmax_v.y), tmax_v.z);
    return tmax >= max(tmin, 0.0);
}

void main() {
    // 1. Scene setup
    vec3 minB = vec3(-100.0, -20.0, -100.0);
    float rootSize = 200.0;
    vec3 maxB = minB + vec3(rootSize);

    // 2. Ray setup
    vec4 clipPos = vec4(TexCoord * 2.0 - 1.0, -1.0, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    worldPos /= worldPos.w;
    vec3 d = normalize(worldPos.xyz - cameraPos);
    vec3 p = cameraPos;

    float t_min, t_max;
    bool rootHit = ray_aabb(p, d, minB, maxB, t_min, t_max);
    t_min = max(t_min, 0.0);
    
    if (!rootHit) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 3. Integer Coordinate Setup (20-bit)
    const float INT_SCALE = 1048576.0; 
    vec3 p_rel = (p + (t_min + 1e-4) * d - minB) / rootSize;
    ivec3 ip = ivec3(clamp(p_rel * INT_SCALE, 0.0, INT_SCALE - 1.0));
    
    uint parentIdx = 0;
    float h = t_max;
    
    struct StackEntry {
        uint pIdx;
        float h;
        ivec3 pos;
    } stack[MAX_DEPTH];
    
    int scale = 0; 
    ivec3 pos = ivec3(0);
    int curSize = int(INT_SCALE);

    int iters = 0;
    while (iters < 1024) {
        iters++;

        int childSize = curSize >> 1;
        ivec3 center = pos + ivec3(childSize);
        int idx = 0;
        if (ip.x >= center.x) idx |= 1;
        if (ip.y >= center.y) idx |= 2;
        if (ip.z >= center.z) idx |= 4;

        ivec3 voxMin = pos + ivec3((idx&1)!=0?childSize:0, (idx&2)!=0?childSize:0, (idx&4)!=0?childSize:0);
        ivec3 voxMax = voxMin + ivec3(childSize);

        vec3 fVoxMin = minB + (vec3(voxMin) / INT_SCALE) * rootSize;
        vec3 fVoxMax = minB + (vec3(voxMax) / INT_SCALE) * rootSize;
        
        float tv_min, tv_max;
        bool intersect = ray_aabb(p, d, fVoxMin, fVoxMax, tv_min, tv_max);
        float real_tv_min = max(tv_min, t_min);

        if (intersect && real_tv_min <= tv_max && real_tv_min <= h) {
            uint topo = nodes[parentIdx].topology;
            uint validMask = (topo >> 24) & 0xFFu;
            uint leafMask = (topo >> 16) & 0xFFu;

            if (((validMask >> idx) & 1u) != 0u) {
                if (((leafMask >> idx) & 1u) != 0u) {
                    uint leafOffset = 0;
                    for(int i = 0; i < idx; i++) {
                        if(((leafMask >> i) & 1u) != 0u) leafOffset++;
                    }
                    uint leafIdx = nodes[parentIdx].leaf_ptr + leafOffset;
                    vec3 color = vec3(
                        float((albedos[leafIdx] >> 24) & 0xFFu),
                        float((albedos[leafIdx] >> 16) & 0xFFu),
                        float((albedos[leafIdx] >>  8) & 0xFFu)
                    ) / 255.0;

                    // ESVO Contour Hit Test
                    uint contourIdx = nodes[parentIdx].contour_ptr + leafOffset;
                    vec4 c = contours[contourIdx];
                    vec3 cNormal = c.xyz;
                    float cDist  = c.w;

                    float denom = dot(cNormal, d);
                    if (abs(denom) > 1e-6) {
                        float t_plane = (cDist - dot(cNormal, p)) / denom;
                        if (t_plane >= tv_min && t_plane <= tv_max) {
                            real_tv_min = t_plane;
                        }
                    }
                    
                    // Wireframe logic (from previous version)
                    vec3 hitP = p + real_tv_min * d;
                    vec3 localPos = (hitP - fVoxMin) / (fVoxMax - fVoxMin + 1e-6);
                    vec3 distToEdge = min(localPos, 1.0 - localPos);
                    float d2 = max(min(distToEdge.x, distToEdge.y), min(max(distToEdge.x, distToEdge.y), distToEdge.z));
                    
                    if (d2 < 0.04) {
                        // Yellowish wireframe on top of radiance
                        FragColor = vec4(mix(color, vec3(1.0, 0.9, 0.0), 0.8), 1.0); 
                    } else {
                        FragColor = vec4(color, 1.0);
                    }
                    
                    return;
                } else {
                    if (scale < MAX_DEPTH - 1) {
                        stack[scale].pIdx = parentIdx;
                        stack[scale].h = h;
                        stack[scale].pos = pos;
                        
                        uint childBlockStart = nodes[parentIdx].child_ptr;
                        uint offset = 0;
                        for(int i=0; i<idx; i++) {
                            if(((validMask >> i) & 1u) != 0u && ((leafMask >> i) & 1u) == 0u) offset++;
                        }
                        
                        parentIdx = childBlockStart + offset;
                        h = tv_max;
                        pos = voxMin;
                        curSize = childSize;
                        scale++;
                        continue;
                    }
                }
            }
        }

        vec3 t_planes = (mix(fVoxMin, fVoxMax, step(vec3(0.0), d)) - p) / (d + 1e-9);
        float t_exit = min(min(t_planes.x, t_planes.y), t_planes.z);
        
        t_min = t_exit + 1e-4; 
        vec3 nextP = (p + t_min * d - minB) / rootSize;
        ip = ivec3(clamp(nextP * INT_SCALE, 0.0, INT_SCALE - 1.0));
        
        ivec3 relIP = ip - pos;
        if (any(lessThan(relIP, ivec3(0))) || any(greaterThanEqual(relIP, ivec3(curSize)))) {
            if (scale > 0) {
                scale--;
                parentIdx = stack[scale].pIdx;
                h = stack[scale].h;
                pos = stack[scale].pos;
                curSize <<= 1;
                continue;
            } else break;
        }
    }

    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
