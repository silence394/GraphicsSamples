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

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader

VS_OUTPUT main( uint id : SV_VERTEXID )
{
    VS_OUTPUT output;

    // Faces
    // +X, +Y, +Z, -X, -Y, -Z
    // Vertices
    // 0 ---15
    // |   / |
    // |  /  |
    // | /   |
    // 24--- 3
    
    uint face_idx = id / 6;
    uint vtx_idx = id % 6;
    float3 P;
    P.x = ((vtx_idx % 3) == 2) ? -1 : 1;
    P.y = ((vtx_idx % 3) == 1) ? -1 : 1;
    P.z = 0;
    if ((face_idx % 3) == 0)
        P.yzx = P.xyz;
    else if ((face_idx % 3) == 1)
        P.xzy = P.xyz;
    // else if ((face_idx % 3) == 2)
    //    P.xyz = P.xyz;
    P *= ((vtx_idx / 3) == 0) ? 1 : -1;
    
    float3 N;
    N.x = ((face_idx % 3) == 0) ? 1 : 0;
    N.y = ((face_idx % 3) == 1) ? 1 : 0;
    N.z = ((face_idx % 3) == 2) ? 1 : 0;
    N *= ((face_idx / 3) == 0) ? 1 : -1;
    P += N;

    output.P = mul(c_mObject, float4(P, 1));
    output.ScreenP = mul(c_mViewProj, output.P);
    output.N = mul(c_mObject, float4(N, 0)).xyz;
    return output;
}
