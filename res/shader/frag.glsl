#version 450

layout(set = 0, binding = 1) uniform sampler2D tex;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(tex, fragTexCoord);
    vec3 color = mix(fragColor, texColor.rgb, min(texColor.a, 0.8));
    outColor = vec4(color, 1.0);
}
