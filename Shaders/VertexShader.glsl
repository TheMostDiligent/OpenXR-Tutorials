#version 450
#extension GL_KHR_vulkan_glsl : enable
// Color Vertex Shader
layout(std140, binding = 0) uniform CameraConstants {
    mat4 viewProj;
    mat4 modelViewProj;
    mat4 model;
};
layout(std140, binding = 1) uniform Normals {
    vec4 normals[6];
};
layout(location = 0) in vec4 a_Positions;
layout(location = 0) out flat uvec2 o_TexCoord;
layout(location = 1) out flat vec3 o_Normal;
void main() {
    gl_Position = modelViewProj * a_Positions;
    int face = gl_VertexIndex / 6;
    o_TexCoord = uvec2(face, 0);
    o_Normal = (model*normals[face]).xyz;
}