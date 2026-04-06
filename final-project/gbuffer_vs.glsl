#version 430 core

layout (location = 0) in vec3 vertex_position;
layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec2 vertex_texture;
layout (location = 3) in vec3 vertex_tangent;
layout (location = 4) in vec3 vertex_bitangent;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
    vec4 worldPos = model * vec4(vertex_position, 1.0);
    FragPos = worldPos.xyz; 
    TexCoords = vertex_texture;
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalize(normalMatrix * vertex_normal);

    vec3 T = normalize(normalMatrix * vertex_tangent);
    vec3 N = Normal;
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt re-orthogonalize
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    gl_Position = proj * view * worldPos;
}
