#version 430 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in mat3 TBN;

uniform sampler2D ourTexture;
uniform sampler2D normalMap;
uniform float normalMapIntensity;

void main()
{    
    // Position
    gPosition = FragPos;
    
    // Normal (with normal map applied via TBN)
    vec3 tangentNormal = texture(normalMap, TexCoords).rgb;
    tangentNormal = normalize(tangentNormal * 2.0 - 1.0);
    vec3 blendedTangentNormal = normalize(mix(vec3(0.0, 0.0, 1.0), tangentNormal, normalMapIntensity));
    gNormal = normalize(TBN * blendedTangentNormal);
    
    // Albedo
    gAlbedoSpec.rgb = texture(ourTexture, TexCoords).rgb;
    gAlbedoSpec.a = 1.0; // Placeholder for specularity
}
