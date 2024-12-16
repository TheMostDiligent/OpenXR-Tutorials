// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

cbuffer CameraConstants : register(b0) // Registers are irrelevant in Diligent Engine
{
    float4x4 viewProj;
    float4x4 modelViewProj;
    float4x4 model;
    float4 color;
    float4 pad1;
    float4 pad2;
    float4 pad3;
};
cbuffer Normals : register(b1) // Registers are irrelevant in Diligent Engine
{
    float4 normals[6];
};

struct VS_IN
{
    uint vertexId : SV_VertexId;
    // Vertex input attributes in Diligent Engine need to have ATTRIBx semantic
    float4 a_Positions : ATTRIB0;
};
struct VS_OUT
{
    float4 o_Position : SV_Position;
    nointerpolation float2 o_TexCoord : TEXCOORD0;
    float3 o_Normal : TEXCOORD1;
    nointerpolation float3 o_Color : TEXCOORD2;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.o_Position = mul(modelViewProj, IN.a_Positions);
    int face = IN.vertexId / 6;
    OUT.o_TexCoord = float2(float(face), 0);
    OUT.o_Normal = (mul(model, normals[face])).xyz;
    OUT.o_Color = color.rgb;
    return OUT;
}
