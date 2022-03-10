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

- SHADOWMAPTYPE:
    - SHADOWMAPTYPE_ATLAS
    - SHADOWMAPTYPE_ARRAY

- CASCADECOUNT:
    - CASCADECOUNT_1: 1
    - CASCADECOUNT_2: 2
    - CASCADECOUNT_3: 3
    - CASCADECOUNT_4: 4

- VOLUMETYPE:
	- VOLUMETYPE_FRUSTUM
	- VOLUMETYPE_PARABOLOID

%% MUX_END %%
*/

#include "ShaderCommon.h"

#define COARSE_CASCADE (CASCADECOUNT-1)

#if (SHADOWMAPTYPE == SHADOWMAPTYPE_ATLAS)
Texture2D<float> tShadowMap : register(t1);
#elif (SHADOWMAPTYPE == SHADOWMAPTYPE_ARRAY)
Texture2DArray<float> tShadowMap : register(t1);
#endif

float SampleShadowMap(float2 tex_coord, int cascade)
{
	float depth_value = 1.0f;
	float2 lookup_coord = g_vElementOffsetAndScale[cascade].zw * tex_coord + g_vElementOffsetAndScale[cascade].xy;
#if (SHADOWMAPTYPE == SHADOWMAPTYPE_ATLAS)
	depth_value = tShadowMap.SampleLevel( sBilinear, lookup_coord, 0).x;
#elif (SHADOWMAPTYPE == SHADOWMAPTYPE_ARRAY)
	depth_value = tShadowMap.SampleLevel( sBilinear, float3( lookup_coord, (float)g_uElementIndex[cascade] ), 0).x;
#endif
	return depth_value;
}

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

float3 ParaboloidUnproject(float3 P, float zNear, float zFar)
{
	// Use a quadratic to find the Z component
	// then reverse the projection to find the unit vector, and scale
	float L = P.z*(zFar-zNear) + zNear;

	float qa = P.x*P.x + P.y*P.y + 1;
	float qb = 2*(P.x*P.x + P.y*P.y);
	float qc = P.x*P.x + P.y*P.y - 1;
	float z = (-qb + sqrt(qb*qb - 4*qa*qc)) / (2*qa);

	float3 outP;
	outP.x = P.x * (z + 1);
	outP.y = P.y * (z + 1);
	outP.z = z;
	return outP*L;
}

HS_POLYGONAL_CONSTANT_DATA_OUTPUT Unused(HS_POLYGONAL_CONSTANT_DATA_OUTPUT input)
{
	return input;
}

[domain("quad")]
PS_POLYGONAL_INPUT main( HS_POLYGONAL_CONSTANT_DATA_OUTPUT input, float2 uv : SV_DOMAINLOCATION, const OutputPatch<HS_POLYGONAL_CONTROL_POINT_OUTPUT, 4> Patch )
{
	Unused(input);//Fix a compiler warning with pssl.

	PS_POLYGONAL_INPUT output = (PS_POLYGONAL_INPUT)0;

	float3 vClipIn1 = lerp(Patch[0].vClipPos.xyz, Patch[1].vClipPos.xyz, uv.x);
	float3 vClipIn2 = lerp(Patch[3].vClipPos.xyz, Patch[2].vClipPos.xyz, uv.x);
	float3 vClipIn = lerp(vClipIn1, vClipIn2, uv.y);

	float4 vPos1 = lerp(Patch[0].vWorldPos, Patch[1].vWorldPos, uv.x);
	float4 vPos2 = lerp(Patch[3].vWorldPos, Patch[2].vWorldPos, uv.x);
	float4 vWorldPos = lerp(vPos1, vPos2, uv.y);

	if (VOLUMETYPE == VOLUMETYPE_FRUSTUM)
	{
		if (all(abs(vClipIn.xy) < EDGE_FACTOR))
		{
			int iCascade = -1;
			float4 vClipPos = float4(0,0,0,1);

			[unroll]
			for (int i = COARSE_CASCADE;i >= 0; --i)
			{
				// Try to refetch from finer cascade
				float4 vClipPosCascade = mul( g_mLightProj[i], vWorldPos );
				vClipPosCascade *= 1.f / vClipPosCascade.w;
				if (all(abs(vClipPosCascade.xy) < 1.0f))
				{
					
					float2 vTex = float2(0.5*vClipPosCascade.x + 0.5, -0.5*vClipPosCascade.y + 0.5);
					float depthSample = SampleShadowMap(vTex, i);
					if (depthSample < 1.0f)
					{
						
						vClipPos.xy = vClipPosCascade.xy;
						vClipPos.z = depthSample;
						iCascade = i;
					}
				}
			}

			if (iCascade >= 0)
			{
				vWorldPos = mul( g_mLightProjInv[iCascade], float4(vClipPos.xyz, 1) );
				vWorldPos *= 1.0f / vWorldPos.w;
				vWorldPos.xyz = g_vEyePosition + (1.0f-g_fGodrayBias)*(vWorldPos.xyz-g_vEyePosition);
			}
		}
		else
		{
			vWorldPos = mul(g_mLightToWorld, float4(vClipIn.xy, 1, 1));
			vWorldPos *= 1.0f / vWorldPos.w;
		}
	}
	else if (VOLUMETYPE == VOLUMETYPE_PARABOLOID)
	{
        vClipIn.xyz = normalize(vClipIn.xyz);
		float4 shadowPos = mul(g_mLightProj[0], vWorldPos);
		shadowPos.xyz = shadowPos.xyz/shadowPos.w;
		uint hemisphereID = (shadowPos.z > 0) ? 0 : 1;
		shadowPos.z = abs(shadowPos.z);
		shadowPos.xyz = ParaboloidProject(shadowPos.xyz, g_fLightZNear, g_fLightZFar);
		float2 shadowTC = float2(0.5f, -0.5f)*shadowPos.xy + 0.5f;
        float depthSample = SampleShadowMap(shadowTC, hemisphereID);
		float sceneDepth = depthSample*(g_fLightZFar-g_fLightZNear)+g_fLightZNear;
		vWorldPos = mul( g_mLightProjInv[0], float4(vClipIn.xyz * sceneDepth, 1));
        vWorldPos *= 1.0f / vWorldPos.w;
	}

	// Transform world position with viewprojection matrix
	output.vWorldPos = vWorldPos;
    output.vPos = mul( g_mViewProj, output.vWorldPos );
    return output;
}
