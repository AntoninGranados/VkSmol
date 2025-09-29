#version 450

layout(set = 0, binding = 0) uniform UBO {
    float time;
    int frameCount;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D prevTex;

layout(location = 0) in vec2 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise2D(vec2 pos, float time) {
    return hash21(pos * 96.19 + vec2(time * 17.0, time * 29.0));
}

void main() {
    float r = noise2D(fragTexCoord, ubo.time);
    outColor = vec4(r, r, r, 1.0);

    outColor = mix(texture(prevTex, fragPos), outColor, 1.0/ubo.frameCount);
}
