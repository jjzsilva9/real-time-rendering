#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoord;
out vec3 WorldFragPos;
out vec3 WorldNormal;
out vec3 TangentLightPos;
out vec3 TangentViewPos;
out vec3 TangentFragPos;
out mat3 vTBN;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform vec4 LightPosition;
uniform vec3 viewPos;

void main() {
    TexCoord = aTexCoords;
    WorldFragPos = vec3(model * vec4(aPos, 1.0));
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    WorldNormal = normalize(normalMatrix * aNormal);

    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    mat3 TBN = mat3(T, B, N);
    vTBN = TBN;
    
    mat3 invTBN = transpose(TBN);
    TangentLightPos = invTBN * LightPosition.xyz;
    TangentViewPos  = invTBN * viewPos;
    TangentFragPos  = invTBN * WorldFragPos;

    gl_Position = proj * view * model * vec4(aPos, 1.0);
}