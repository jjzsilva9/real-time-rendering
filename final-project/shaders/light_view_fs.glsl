#version 330 core
layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D ourTexture;
uniform vec3 Kd = vec3(1.0);

void main()
{
    vec4 albedo = texture(ourTexture, TexCoords);
    if (albedo.a < 0.1) discard; // Alpha testing for Sponza's leaves/fences
    FragColor = vec4(albedo.rgb * Kd, albedo.a);
}
