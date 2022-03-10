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

- MAXTESSFACTOR:
    - MAXTESSFACTOR_LOW: 16.0f
    - MAXTESSFACTOR_MEDIUM: 32.0f
    - MAXTESSFACTOR_HIGH: 64.0f
%% MUX_END %%
*/

#define COARSE_CASCADE (CASCADECOUNT-1)

#include "ShaderCommon.h"

float3 NearestPos(float3 vStartPos, float3 vEndPos)
{
    float3 vPos = (g_vEyePosition - vStartPos);
    float3 vLine = (vEndPos - vStartPos);
    float lineLength = length(vLine);
    float t = max(0, min(lineLength, dot(vPos, vLine)/lineLength));
    return vStartPos + (t/lineLength)*vLine;
}

float CalcTessFactor(float3 vStartPos, float3 vEndPos)
{
    float section_size = length(vEndPos - vStartPos);
	float3 vWorldPos = 0.5f*(vStartPos+vEndPos);
	float3 vEyeVec = (vWorldPos.xyz - g_vEyePosition);
	float4 clip_pos = mul( g_mProj, float4(0, 0, length(vEyeVec), 1) );
	float projected_size = abs(section_size * g_mProj._m11 / clip_pos.w);
	float desired_splits = (projected_size*g_vOutputViewportSize.y)/(g_fTargetRaySize);
	return min(MAXTESSFACTOR, max(1, desired_splits));
}

bool IntersectsFrustum(float4 vPos1, float4 vPos2)
{
	return !(vPos1.x > 1.0 && vPos2.x > 1.0 || vPos1.x < -1.0 && vPos2.x < -1.0)
		|| !(vPos1.y > 1.0 && vPos2.y > 1.0 || vPos1.y < -1.0 && vPos2.y < -1.0)
		|| !(vPos1.z < 0.0 && vPos2.z < 0.0);
}

HS_POLYGONAL_CONSTANT_DATA_OUTPUT HS_POLYGONAL_CONSTANT_FUNC( /*uint PatchID : SV_PRIMITIVEID,*/ const OutputPatch<HS_POLYGONAL_CONTROL_POINT_OUTPUT, 4> opPatch)
{
	HS_POLYGONAL_CONSTANT_DATA_OUTPUT output  = (HS_POLYGONAL_CONSTANT_DATA_OUTPUT)0;

	bool bIsVisible = false;
#if 1
	//Frustum cull
	[unroll]
	for (int j=0; j<4; ++j)
	{
		float4 vScreenClip = mul(g_mViewProj, opPatch[j].vWorldPos);
		vScreenClip *= 1.0f / vScreenClip.w;
		float4 vOriginPos = float4(0,0,0,1);
		if (VOLUMETYPE == VOLUMETYPE_FRUSTUM)
		{
			vOriginPos = mul(g_mLightToWorld, float4(opPatch[j].vClipPos.xy, 0, 1)); 
		}
		else if (VOLUMETYPE == VOLUMETYPE_PARABOLOID)
		{
			vOriginPos = float4(g_vLightPos, 1); 
		}
		float4 vScreenClipOrigin = mul(g_mViewProj, vOriginPos);
		vScreenClipOrigin *= 1.0f / vScreenClipOrigin.w; 
		bIsVisible = bIsVisible || IntersectsFrustum(vScreenClip, vScreenClipOrigin);
	}
#else
	bIsVisible = true;
#endif

	if (bIsVisible)
	{
        float3 nearest_pos[4];
        for (int j=0; j < 4; ++j)
        {
            float3 start_pos;
            if (VOLUMETYPE == VOLUMETYPE_FRUSTUM)
            {
                float4 p = mul(g_mLightToWorld, float4(opPatch[j].vClipPos.xy, 0, 1));
                start_pos = p.xyz / p.w;
            }
            else if (VOLUMETYPE == VOLUMETYPE_PARABOLOID)
                start_pos = g_vLightPos;
            else
                start_pos = float3(0, 0, 0);
            nearest_pos[j] = NearestPos(start_pos, opPatch[j].vWorldPos.xyz);
        }

		float tess_factor[4];
		[unroll]
		for (int k=0; k<4; ++k)
		{
            float tess_near = CalcTessFactor(nearest_pos[(k+3)%4], nearest_pos[k]);
            float tess_far = CalcTessFactor(opPatch[(k+3)%4].vWorldPos.xyz, opPatch[k].vWorldPos.xyz);
            tess_factor[k] = max(tess_near, tess_far);
            if (VOLUMETYPE == VOLUMETYPE_FRUSTUM)
            {
                bool bIsEdge = !(all((abs(opPatch[(k + 3) % 4].vClipPos.xy) < EDGE_FACTOR) || (abs(opPatch[k].vClipPos.xy) < EDGE_FACTOR)));
                output.fEdges[k] = (bIsEdge) ? 1.0f : tess_factor[k];
            }
            else if (VOLUMETYPE == VOLUMETYPE_PARABOLOID)
            {
                output.fEdges[k] = tess_factor[k];
            }
            else
            {
                output.fEdges[k] = 1;
            }
            
		}
		output.fInside[0] = max(tess_factor[1], tess_factor[3]);
        output.fInside[1] = max(tess_factor[0], tess_factor[2]);
	}
	else
	{
		output.fEdges[0] = 0;
		output.fEdges[1] = 0;
		output.fEdges[2] = 0;
		output.fEdges[3] = 0;
		output.fInside[0] = 0;
		output.fInside[1] = 0;
	}

	return output;
}
                          
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HS_POLYGONAL_CONSTANT_FUNC")]
[maxtessfactor(MAXTESSFACTOR)]
HS_POLYGONAL_CONTROL_POINT_OUTPUT main( InputPatch<HS_POLYGONAL_INPUT, 4> ipPatch, uint uCPID : SV_OUTPUTCONTROLPOINTID )
{
	HS_POLYGONAL_CONTROL_POINT_OUTPUT output = (HS_POLYGONAL_CONTROL_POINT_OUTPUT)0;
	output.vWorldPos = ipPatch[uCPID].vWorldPos;
	output.vClipPos = ipPatch[uCPID].vClipPos;
    return output;
}	
