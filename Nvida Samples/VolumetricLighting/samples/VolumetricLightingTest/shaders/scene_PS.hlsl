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
- LIGHTMODE:
    - LIGHTMODE_DIRECTIONAL
    - LIGHTMODE_SPOTLIGHT
    - LIGHTMODE_OMNI
%% MUX_END %%
*/

////////////////////////////////////////////////////////////////////////////////
// Resources

SamplerComparisonState sampShadowmap : register(s0);

#if (LIGHTMODE == LIGHTMODE_DIRECTIONAL || LIGHTMODE == LIGHTMODE_SPOTLIGHT)
    Texture2D<float> tShadowmap : register(t0); 
#elif (LIGHTMODE == LIGHTMODE_OMNI)
    Texture2DArray <float1> tShadowmapArray : register(t0);
#endif
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
    float c_fLightZFar                      : packoffset(c8.y);
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
// Pixel Shader

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

float4 main(VS_OUTPUT input) : SV_Target0
{
    float3 P = input.P.xyz / input.P.w;
    float3 N = normalize(input.N);
    float3 Kd = c_vObjectColor;

    //return float4(0.5*(N+1), 1);
    
    const float SHADOW_BIAS = -0.001f;
    float4 shadow_clip = mul(c_mLightViewProj, float4(P,1));
    shadow_clip = shadow_clip / shadow_clip.w;
    uint hemisphereID = (shadow_clip.z > 0) ? 0 : 1;
    if (LIGHTMODE == LIGHTMODE_OMNI)
    {
        shadow_clip.z = abs(shadow_clip.z);
        shadow_clip.xyz = ParaboloidProject(shadow_clip.xyz, c_fLightZNear, c_fLightZFar);   
    }
    float2 shadow_tc = float2(0.5f, -0.5f)*shadow_clip.xy + 0.5f;
    float receiver_depth = shadow_clip.z+SHADOW_BIAS;

    float total_light = 0;
    const int SHADOW_KERNEL = 2;
    [unroll]
    for (int ox=-SHADOW_KERNEL; ox<=SHADOW_KERNEL; ++ox)
    {
        [unroll]
        for (int oy=-SHADOW_KERNEL; oy<=SHADOW_KERNEL; ++oy)
        {
#if (LIGHTMODE == LIGHTMODE_OMNI)
            total_light += tShadowmapArray.SampleCmpLevelZero(sampShadowmap, float3(shadow_tc, hemisphereID), receiver_depth, int2(ox, oy)).x;
#else
            total_light += tShadowmap.SampleCmpLevelZero(sampShadowmap, shadow_tc, receiver_depth, int2(ox, oy)).x;
#endif
        }
    }
    float shadow_term = total_light / ((2*SHADOW_KERNEL+1) * (2*SHADOW_KERNEL+1));

    float3 output = float3(0,0,0);
    float3 L = -c_vLightDirection;
    if (LIGHTMODE == LIGHTMODE_DIRECTIONAL)
    {
        float3 attenuation = shadow_term*dot(N, L);
        float3 ambient = 0.001f*saturate(0.5f*(dot(N, L)+1.0f));
        output += c_vLightColor*max(attenuation, ambient);

    }
    else if (LIGHTMODE == LIGHTMODE_SPOTLIGHT)
    {
        float light_to_world = length(P - c_vLightPos);
        float3 W = (c_vLightPos - P)/light_to_world;

        float distance_attenuation = 1.0f/(c_vLightAttenuationFactors.x + c_vLightAttenuationFactors.y*light_to_world + c_vLightAttenuationFactors.z*light_to_world*light_to_world) + c_vLightAttenuationFactors.w;

        const float ANGLE_EPSILON = 0.00001f;
        float angle_factor = saturate((dot(N, L)-c_fLightFalloffCosTheta)/(1-c_fLightFalloffCosTheta));
        float spot_attenuation = (angle_factor > ANGLE_EPSILON) ? pow(angle_factor, c_fLightFalloffPower) : 0.0f;

        float3 attenuation = distance_attenuation*spot_attenuation*shadow_term*dot(N, W);
        float3 ambient = 0.00001f*saturate(0.5f*(dot(N, L)+1.0f));
        output += c_vLightColor*max(attenuation, ambient)*exp(-c_vSigmaExtinction*light_to_world);
    }
    else if (LIGHTMODE == LIGHTMODE_OMNI)
    {
        float light_to_world = length(P - c_vLightPos);
        float3 W = (c_vLightPos - P)/light_to_world;
        float distance_attenuation = 1.0f/(c_vLightAttenuationFactors.x + c_vLightAttenuationFactors.y*light_to_world + c_vLightAttenuationFactors.z*light_to_world*light_to_world) + c_vLightAttenuationFactors.w;

        float3 attenuation = distance_attenuation*shadow_term*dot(N, W);
        float3 ambient = 0.00001f*saturate(0.5f*(dot(N, L)+1.0f));
        output += c_vLightColor*max(attenuation, ambient)*exp(-c_vSigmaExtinction*light_to_world);
    }

    return float4(output, 1);
}
