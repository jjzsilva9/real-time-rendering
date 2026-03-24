#version 430 core
layout (location = 0) in vec3 vertex_position;
layout (location = 1) in vec2 vertex_texture;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(vertex_position, 1.0);
    TexCoord = vertex_texture;
}
