#version 430 core

layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer AlbedoPool {
    uint albedos[];
};

layout(std430, binding = 2) buffer NormalPool {
    vec3 normals[];
};

layout(std430, binding = 3) buffer PositionPool {
    vec3 positions[];
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

vec3 unpackColor(uint c) {
    return vec3(float((c >> 24) & 0xFFu), float((c >> 16) & 0xFFu), float((c >> 8) & 0xFFu)) / 255.0;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= numLeaves) return;

    vec3 worldPos = positions[idx];
    vec3 normal = normals[idx];
    vec3 albedo = unpackColor(albedos[idx]);

    // Project into light space
    vec4 lightSpacePos = lightViewProj * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) {
        radiance[idx] = vec4(0.0);
        return;
    }

    // Shadow mapping check
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;

    float diff = max(dot(normal, -lightDir), 0.0);
    
    if (currentDepth > closestDepth + bias || diff <= 0.01) {
        // In shadow or back-facing the sun
        radiance[idx] = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        // Lit
        vec3 color = albedo * lightColor * diff;
        radiance[idx] = vec4(color, 1.0);
    }
}
