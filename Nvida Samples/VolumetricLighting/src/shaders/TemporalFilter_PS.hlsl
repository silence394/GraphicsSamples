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

#include "ShaderCommon.h"

Texture2D<float4> tCurrBuffer : register(t0);
Texture2D<float4> tLastBuffer : register(t1);
Texture2D<float2> tCurrDepth : register(t2);
Texture2D<float2> tLastDepth : register(t3);

static const int2 NEIGHBOR_OFFSETS[] = {
	int2(-1, -1),	int2( 0, -1),	int2( 1, -1),
	int2(-1,  0),	int2( 0,  0),	int2( 1,  0),
	int2(-1,  1),	int2( 0,  1),	int2( 1,  1)
};

#if 1
static const float NEIGHBOR_WEIGHTS[] = {
	0.015625f,	0.125000f,	0.015625f,
	0.125000f,	1.000000f,	0.125000f,
	0.015625f,	0.125000f,	0.015625f,
};
#else
static const float NEIGHBOR_WEIGHTS[] = {
	0, 0, 0,
	0, 1, 0,
	0, 0, 0,
};
#endif

float RGB_to_Y (float3 input)
{
	return 0.50f*input.g + 0.25f*(input.r + input.b);
}

float3 RGB_to_YCoCg (float3 input)
{
	float3 ret;
	float tmp = 0.25f*(input.r + input.b);
	ret.x = 0.50f*input.g + tmp;
	ret.y = 0.50f*(input.r - input.b);
	ret.z = 0.50f*input.g - tmp;
	return ret;
}

float3 YCoCg_to_RGB(float3 input)
{
	float3 ret;
	float Y_val = input.x; float Co = input.y; float Cg = input.z;
	float tmp = Y_val - Cg;
	ret.r = tmp + Co;
	ret.g = Y_val + Cg;
	ret.b = tmp - Co;
	return ret;
}

float3 Tonemap( float3 sample_rgb )
{
	sample_rgb = sample_rgb / (1 + sample_rgb);
	return RGB_to_YCoCg(sample_rgb);
}

float3 Tonemap_Inv( float3 sample_YCoCg )
{
	float3 sample_rgb = YCoCg_to_RGB(sample_YCoCg);
	return sample_rgb / (1 - sample_rgb);
}

struct FILTER_OUTPUT
{
	float3 color : SV_TARGET0;
	float2 depth : SV_TARGET1;
};

FILTER_OUTPUT main(VS_QUAD_OUTPUT input)
{
	FILTER_OUTPUT output;

	// load neighbors
	float3 curr_sample = float3(0,0,0);
	float2 curr_depth = float2(0,0);
	float neighborhood_bounds_max = 0;
	float neighborhood_bounds_min = 0;
	int2 max_dimensions = int2(g_vViewportSize);
	int2 base_tc = int2(floor(input.vTex.xy*max_dimensions));
	float total_weight = -1.0f;

	[unroll]
	for (int n=0; n<9; ++n)
	{
		int2 sample_tc = max( int2(0,0), min(max_dimensions, base_tc + NEIGHBOR_OFFSETS[n]));
		float3 neighbor_sample = max(float3(0,0,0), tCurrBuffer.Load(int3(sample_tc, 0)).rgb);
		float2 neighbor_depth = tCurrDepth.Load(int3(sample_tc, 0)).rg;
		bool is_valid = all(isfinite(neighbor_sample.xyz));
		if (is_valid)
		{
			neighbor_sample = Tonemap(neighbor_sample);
			float weight = NEIGHBOR_WEIGHTS[n];
			curr_sample += weight*neighbor_sample;
			curr_depth += weight*neighbor_depth;
			if (total_weight <= 0.0f)
			{
				neighborhood_bounds_max = neighbor_sample.x;
				neighborhood_bounds_min = neighbor_sample.x;
				total_weight = weight;
			}
			else
			{
				neighborhood_bounds_max = max(neighborhood_bounds_max, neighbor_sample.x);
				neighborhood_bounds_min = min(neighborhood_bounds_min, neighbor_sample.x);
				total_weight += weight;
			}
		}
	}
	curr_sample = (total_weight > 0) ? curr_sample/total_weight : float3(0,0,0);
	curr_depth =  (total_weight > 0) ? curr_depth/total_weight : float2(1, 1);

	// Transform and apply history
	const float MAX_HISTORY_FACTOR = 0.98f;
	float history_factor = g_fHistoryFactor;

	float4 curr_clip;
	curr_clip.xy = float2(2, -2) * input.vTex.xy + float2(-1, 1);
	curr_clip.z = WarpDepth(curr_depth.x, g_fZNear, g_fZFar);
	curr_clip.w = 1;
	float4 last_clip = mul( g_mHistoryXform, curr_clip );
	last_clip = last_clip/last_clip.w;

	float2 last_tc = saturate((float2(0.5f, -0.5f)*last_clip.xy+float2(0.5f, 0.5f))) * max_dimensions;
	float3 last_sample = tLastBuffer.Load(int3(last_tc, 0)).rgb;
	float2 last_depth = tLastDepth.Load(int3(last_tc, 0)).rg;
	last_sample = all(isfinite(last_sample)) ? Tonemap(last_sample) : curr_sample;

	history_factor = all(abs(last_clip.xy) <= 1.0f) ? history_factor : 0.0f;

	float2 clip_diff = (last_clip.xy - curr_clip.xy) * g_vViewportSize * g_vViewportSize_Inv.xx;
	float clip_dist = length(clip_diff);
	float movement_factor = saturate(1.0f - clip_dist/g_fFilterThreshold);
	history_factor *= movement_factor*movement_factor*movement_factor;

	float depth_diff = abs(curr_depth.r-last_depth.r);
	float local_variance = abs(curr_depth.g - curr_depth.r*curr_depth.r) + abs(last_depth.g - last_depth.r*last_depth.r);
	local_variance = max(local_variance, 0.0001f);
#if 0
	float local_stddev = sqrt(local_variance);
	float depth_factor = saturate(depth_diff-local_stddev);
	depth_factor = local_stddev / (local_stddev + depth_factor);
#else
	float depth_factor = saturate(depth_diff-local_variance);
	depth_factor = local_variance / (local_variance + depth_factor);
#endif
	history_factor *= depth_factor;

	// threshold based on neighbors
	// Convert to Y Co Cg, then clip to bounds of neighborhood
	float3 blended_sample = curr_sample;
	float2 blended_depth = curr_depth;
	if (history_factor > 0.0f)
	{
		const float CLIP_EPSILON = 0.0001f;
		float3 clip_vec = last_sample - curr_sample;
		float clamped_Y = max(neighborhood_bounds_min, min(neighborhood_bounds_max, last_sample.x));
		float clip_factor_Y = (abs(clip_vec.x) > CLIP_EPSILON) ? abs((clamped_Y-curr_sample.x) / clip_vec.x) : 1.0f;
		float clip_factor = clip_factor_Y;
		float3 clipped_history = curr_sample + clip_factor*clip_vec;

		history_factor = min(history_factor, MAX_HISTORY_FACTOR);
		blended_sample = lerp(curr_sample, clipped_history, history_factor);
		blended_depth = lerp(curr_depth, last_depth, history_factor);
	}

	output.color = Tonemap_Inv(blended_sample);
	output.depth = blended_depth;
	return output;
}
