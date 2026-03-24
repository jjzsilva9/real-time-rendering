#version 430 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D screenTexture;

void main() {
    FragColor = vec4(texture(screenTexture, TexCoord).rgb, 1.0);
}
