// This code contains NVIDIA Confidential Information and is disclosed 
// under the Mutual Non-Disclosure Agreement. 
// 
// Notice 
// ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES 
// NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO 
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT, 
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
// 
// NVIDIA Corporation assumes no responsibility for the consequences of use of such 
// information or for any infringement of patents or other rights of third parties that may 
// result from its use. No license is granted by implication or otherwise under any patent 
// or patent rights of NVIDIA Corporation. No third party distribution is allowed unless 
// expressly authorized by NVIDIA.  Details are subject to change without notice. 
// This code supersedes and replaces all information previously supplied. 
// NVIDIA Corporation products are not authorized for use as critical 
// components in life support devices or systems without express written approval of 
// NVIDIA Corporation. 
// 
// Copyright (c) 2003 - 2016 NVIDIA Corporation. All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software and related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is
// strictly prohibited.
//

/*
Define the shader permutations for code generation
%% MUX_BEGIN %%

%% MUX_END %%

*/

////////////////////////////////////////////////////////////////////////////////
// Constant Buffers

cbuffer CameraCB : register( b0 )
{
    column_major float4x4 c_mViewProj   : packoffset(c0);
    float3 c_vEyePos                    : packoffset(c4);
    float c_fZNear                      : packoffset(c5);
    float c_fZFar                       : packoffset(c5.y);
};

cbuffer ObjectCB : register( b1 )
{
    column_major float4x4 c_mObject : packoffset(c0);
    float3 c_vObjectColor : packoffset(c4);
};

cbuffer LightCB : register( b2 )
{
    column_major float4x4 c_mLightViewProj  : packoffset(c0);
    float3 c_vLightDirection                : packoffset(c4);
    float c_fLightFalloffCosTheta           : packoffset(c4.w);
    float3 c_vLightPos                      : packoffset(c5);
    float c_fLightFalloffPower              : packoffset(c5.w);
    float3 c_vLightColor                    : packoffset(c6);
    float4 c_vLightAttenuationFactors       : packoffset(c7);
    float c_fLightZNear                     : packoffset(c8);
    float c_fLightZNFar                     : packoffset(c8.y);
    float3 c_vSigmaExtinction               : packoffset(c9);
};

////////////////////////////////////////////////////////////////////////////////
// IO Structures

struct VS_OUTPUT
{
    float4 ScreenP : SV_POSITION;
    float4 P : TEXCOORD0;
    float3 N : NORMAL0;
};

struct GS_OUTPUT
{
    float4 ScreenP : SV_POSITION;
    float4 P : TEXCOORD0;
    float3 N : NORMAL;
    uint Target : SV_RenderTargetArrayIndex;
    float Wz : SV_ClipDistance0;
};

////////////////////////////////////////////////////////////////////////////////
// Geometry Shader

float3 ParaboloidProject(float3 P, float zNear, float zFar)
{
	float3 outP;
	float lenP = length(P.xyz);
	outP.xyz = P.xyz/lenP;
	outP.x = outP.x / (outP.z + 1);
	outP.y = outP.y / (outP.z + 1);			
	outP.z = (lenP - zNear) / (zFar - zNear);
	return outP;
}

void GenerateOmniTriangle(uint target, VS_OUTPUT vA, VS_OUTPUT vB, VS_OUTPUT vC, inout TriangleStream<GS_OUTPUT> output)
{
    GS_OUTPUT outValue;
    outValue.Target = target;
    outValue.ScreenP = float4(ParaboloidProject(vA.ScreenP.xyz, c_fZNear, c_fZFar), 1);
    outValue.P = vA.P;
    outValue.N = vA.N;
    outValue.Wz = vA.ScreenP.z;
    output.Append(outValue);
    outValue.ScreenP = float4(ParaboloidProject(vB.ScreenP.xyz, c_fZNear, c_fZFar), 1);
    outValue.P = vB.P;
    outValue.N = vB.N;
    outValue.Wz = vB.ScreenP.z;
    output.Append(outValue);
    outValue.ScreenP = float4(ParaboloidProject(vC.ScreenP.xyz, c_fZNear, c_fZFar), 1);
    outValue.P = vC.P;
    outValue.N = vC.N;
    outValue.Wz = vC.ScreenP.z;
    output.Append(outValue);
    output.RestartStrip();
}

[maxvertexcount(6)]
void main(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> output)
{
    float minZ = min(input[0].ScreenP.z, min(input[1].ScreenP.z, input[2].ScreenP.z));
    float maxZ = max(input[0].ScreenP.z, max(input[1].ScreenP.z, input[2].ScreenP.z));

    if (maxZ >= 0)
    {
        GenerateOmniTriangle(0, input[0], input[1], input[2], output);
    }

    if (minZ <= 0)
    {
        input[0].ScreenP.z *= -1;
        input[1].ScreenP.z *= -1;
        input[2].ScreenP.z *= -1;
        GenerateOmniTriangle(1, input[2], input[1], input[0], output);
    }
}
