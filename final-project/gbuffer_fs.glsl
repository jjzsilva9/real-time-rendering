#version 430 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D ourTexture;
uniform sampler2D normalMap;
uniform float normalMapIntensity;

void main()
{    
    // Position
    gPosition = FragPos;
    
    // Normal (with normal map)
    vec3 flatNormal = Normal;
    vec3 mappedNormal = texture(normalMap, TexCoords).rgb;
    mappedNormal = normalize(mappedNormal * 2.0 - 1.0);
    // Apply normal map (simplified blend for now)
    gNormal = normalize(mix(flatNormal, mappedNormal, normalMapIntensity));
    
    // Albedo
    gAlbedoSpec.rgb = texture(ourTexture, TexCoords).rgb;
    gAlbedoSpec.a = 1.0; // Placeholder for specularity
}
