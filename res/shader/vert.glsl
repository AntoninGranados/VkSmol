#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec2 fragPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    fragPos = aPosition*0.5+0.5;
    fragColor = aColor;
    fragTexCoord = aTexCoord;
}
