#version 430 core

layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer AlbedoPool {
    uint albedos[];
};

// vec3 in std430 has 16-byte stride, but C++ glm::vec3 is 12 bytes (tightly packed).
// Use float arrays to guarantee the correct 12-byte stride matching the C++ layout.
layout(std430, binding = 2) buffer NormalPool {
    float normals[];
};

layout(std430, binding = 3) buffer PositionPool {
    float positions[];
};

layout(std430, binding = 4) buffer RadiancePool {
    vec4 radiance[];
};

uniform sampler2D shadowMap;
uniform sampler2D albedoMap;

uniform mat4 lightViewProj;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform uint numLeaves;
uniform int  numPhotons;  // samples per axis: total = numPhotons^2
uniform float voxelSize;  // world-space size of one leaf voxel

vec3 unpackColor(uint c) {
    return vec3(float((c >> 24) & 0xFFu), float((c >> 16) & 0xFFu), float((c >> 8) & 0xFFu)) / 255.0;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= numLeaves) return;

    vec3 worldPos = vec3(positions[idx*3], positions[idx*3+1], positions[idx*3+2]);
    vec3 normal   = vec3(normals[idx*3],   normals[idx*3+1],   normals[idx*3+2]);

    // Build a tangent frame in the light's perpendicular plane for sample offsets.
    // Samples are spread across the voxel's footprint so shadow edges are soft and
    // thin surfaces that miss the single center sample still receive light.
    vec3 lightRight = normalize(cross(-lightDir, vec3(0.0, 1.0, 0.0)));
    if (length(lightRight) < 0.001)
        lightRight = normalize(cross(-lightDir, vec3(1.0, 0.0, 0.0)));
    vec3 lightUp = cross(lightRight, -lightDir);

    float n = float(numPhotons);
    // Sample surface albedo from the light-view color buffer at the voxel center.
    // This gives the real texture color rather than the flat material Kd,
    // enabling correct color bleeding (red walls bleed red, etc.)
    vec4 centerLS = lightViewProj * vec4(worldPos, 1.0);
    vec3 centerUV = (centerLS.xyz / centerLS.w) * 0.5 + 0.5;
    vec3 surfaceAlbedo = (centerUV.z <= 1.0) ? texture(albedoMap, centerUV.xy).rgb : vec3(1.0);

    float totalDiff   = 0.0;
    float totalVis    = 0.0;
    float totalWeight = 0.0;

    for (int i = 0; i < numPhotons; i++) {
        for (int j = 0; j < numPhotons; j++) {
            // Stratified offset in [-0.5, +0.5] of the voxel's footprint
            float u = (float(i) + 0.5) / n - 0.5;
            float v = (float(j) + 0.5) / n - 0.5;
            vec3 samplePos = worldPos + lightRight * (u * voxelSize)
                                      + lightUp    * (v * voxelSize);

            vec4 lightSpacePos = lightViewProj * vec4(samplePos, 1.0);
            vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
            projCoords         = projCoords * 0.5 + 0.5;

            if (projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0)))
                                    || any(greaterThan(projCoords.xy, vec2(1.0)))) {
                totalWeight += 1.0;
                continue; // outside frustum — count as unlit
            }

            float diff  = max(dot(normal, -lightDir), 0.0);
            float bias  = max(0.0001 * (1.0 - diff), 0.00005);
            float depth = texture(shadowMap, projCoords.xy).r;
            float lit   = (projCoords.z <= depth + bias && diff > 0.01) ? 1.0 : 0.0;

            totalDiff   += diff * lit;
            totalVis    += lit;
            totalWeight += 1.0;
        }
    }

    if (totalWeight == 0.0) {
        radiance[idx] = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float avgDiff = totalDiff / totalWeight;
    float avgVis  = totalVis  / totalWeight;

    if (avgVis <= 0.0) {
        radiance[idx] = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        // avgDiff already folds in NdotL and visibility; surfaceAlbedo comes from the
        // light-view texture so it carries the real surface color for color bleeding.
        vec3 color = surfaceAlbedo * lightColor * avgDiff;
        radiance[idx] = vec4(color, 1.0);
    }
}
