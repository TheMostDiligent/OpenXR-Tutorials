// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_multiview : enable
layout(std140, binding = 0) uniform CameraConstants {
    mat4 viewProj[2];
    mat4 modelViewProj[2];
    mat4 model;
    vec4 color;
    vec4 pad1;
    vec4 pad2;
    vec4 pad3;
};
layout(std140, binding = 1) uniform Normals {
        vec4 normals[6];
};
layout(location = 0) in vec4 a_Positions;
layout(location = 0) out flat uvec2 o_TexCoord;
layout(location = 1) out flat vec3 o_Normal;
layout(location = 2) out flat vec3 o_Color;
void main() {
    gl_Position = modelViewProj[gl_ViewIndex] * a_Positions;
    int face = gl_VertexIndex / 6;
    o_TexCoord = uvec2(face, 0);
    o_Normal = (model * normals[face]).xyz;
    o_Color = color.rgb;
}
