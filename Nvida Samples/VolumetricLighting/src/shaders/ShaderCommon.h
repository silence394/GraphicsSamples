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
// Copyright (C) 2013, NVIDIA Corporation. All rights reserved.

/*===========================================================================
Constants
===========================================================================*/

static const float PI = 3.1415926535898f;
static const float EDGE_FACTOR = 1.0f - (2.0f/64.0f) * (1.0f/64.0f);
static const uint MAX_PHASE_TERMS = 4;

#ifdef __PSSL__
static const float2 SAMPLE_POSITIONS[] = {
	// 1x
	float2( 0, 0)/16.f,
	// 2x
	float2(-4, 4)/16.f,
	float2( 4,-4)/16.f,
	// 4x
	float2(-6, 6)/16.f,
	float2( 6,-6)/16.f,
	float2(-2,-2)/16.f,
	float2( 2, 2)/16.f,
	// 8x
	float2(-7,-3)/16.f,
	float2( 7, 3)/16.f,
	float2( 1,-5)/16.f,
	float2(-5, 5)/16.f,
	float2(-3,-7)/16.f,
	float2( 3, 7)/16.f,
	float2( 5,-1)/16.f,
	float2(-1, 1)/16.f
};

// constant buffers
#define cbuffer ConstantBuffer

// textures and samplers
#define Texture2DMS MS_Texture2D
#define Texture2DArray Texture2D_Array
#define SampleLevel SampleLOD
#define GetSamplePosition(s) GetSamplePoint(s)

// semantics
#define SV_DEPTH S_DEPTH_OUTPUT
#define SV_DOMAINLOCATION S_DOMAIN_LOCATION
#define SV_INSIDETESSFACTOR S_INSIDE_TESS_FACTOR
#define SV_INSTANCEID S_INSTANCE_ID
#define SV_ISFRONTFACE S_FRONT_FACE
#define SV_OUTPUTCONTROLPOINTID S_OUTPUT_CONTROL_POINT_ID
#define SV_POSITION S_POSITION
#define SV_POSITION S_POSITION
#define SV_PRIMITIVEID S_PRIMITIVE_ID
#define SV_SAMPLEINDEX S_SAMPLE_INDEX
#define SV_TARGET S_TARGET_OUTPUT
#define SV_TARGET0 S_TARGET_OUTPUT0
#define SV_TARGET1 S_TARGET_OUTPUT1
#define SV_TESSFACTOR S_EDGE_TESS_FACTOR
#define SV_VERTEXID S_VERTEX_ID

// hull and domain shader properties
#define domain DOMAIN_PATCH_TYPE
#define partitioning PARTITIONING_TYPE
#define outputtopology OUTPUT_TOPOLOGY_TYPE
#define outputcontrolpoints OUTPUT_CONTROL_POINTS
#define patchconstantfunc PATCH_CONSTANT_FUNC
#define maxtessfactor MAX_TESS_FACTOR

// need to figure out how to deal with those exactly:
#define shared
#endif

/*===========================================================================
Sampler states
===========================================================================*/
SamplerState 	 sPoint : register(s0);
SamplerState 	 sBilinear : register(s1);

/*===========================================================================
Constant buffers
===========================================================================*/
shared cbuffer cbContext : register(b0)
{
	float2 g_vOutputSize				: packoffset(c0);
	float2 g_vOutputSize_Inv			: packoffset(c0.z);
	float2 g_vBufferSize				: packoffset(c1);
	float2 g_vBufferSize_Inv			: packoffset(c1.z);
	float g_fResMultiplier				: packoffset(c2);
	unsigned int g_uBufferSamples		: packoffset(c2.y);
}

shared cbuffer cbFrame : register(b1)
{
	column_major float4x4 g_mProj		: packoffset(c0);
	column_major float4x4 g_mViewProj	: packoffset(c4);
    column_major float4x4 g_mViewProjInv: packoffset(c8);
    float2 g_vOutputViewportSize        : packoffset(c12);
    float2 g_vOutputViewportSize_Inv    : packoffset(c12.z);
    float2 g_vViewportSize              : packoffset(c13);
    float2 g_vViewportSize_Inv          : packoffset(c13.z);
    float3 g_vEyePosition				: packoffset(c14);
	float2 g_vJitterOffset				: packoffset(c15);
	float g_fZNear						: packoffset(c15.z);
	float g_fZFar						: packoffset(c15.w);
    float3 g_vScatterPower              : packoffset(c16);
    unsigned int g_uNumPhaseTerms       : packoffset(c16.w);
    float3 g_vSigmaExtinction           : packoffset(c17);
    unsigned int g_uPhaseFunc[4]		: packoffset(c18);
    float4 g_vPhaseParams[4]			: packoffset(c22);
};

shared cbuffer cbVolume : register(b2)
{
	column_major float4x4 g_mLightToWorld	: packoffset(c0);
	float g_fLightFalloffAngle				: packoffset(c4.x);
	float g_fLightFalloffPower				: packoffset(c4.y);
	float g_fGridSectionSize				: packoffset(c4.z);
	float g_fLightToEyeDepth				: packoffset(c4.w);
    float g_fLightZNear                     : packoffset(c5);
    float g_fLightZFar                      : packoffset(c5.y);
	float4 g_vLightAttenuationFactors		: packoffset(c6);
	column_major float4x4 g_mLightProj[4]	: packoffset(c7);
	column_major float4x4 g_mLightProjInv[4]: packoffset(c23);
	float3 g_vLightDir						: packoffset(c39);
	float g_fGodrayBias						: packoffset(c39.w);
	float3 g_vLightPos						: packoffset(c40);
    unsigned int g_uMeshResolution          : packoffset(c40.w);
	float3 g_vLightIntensity				: packoffset(c41);
	float g_fTargetRaySize					: packoffset(c41.w);
	float4 g_vElementOffsetAndScale[4]		: packoffset(c42); 
	float4 g_vShadowMapDim					: packoffset(c46);
	unsigned int g_uElementIndex[4]	        : packoffset(c47);
};

shared cbuffer cbApply : register(b3)
{
	column_major float4x4 g_mHistoryXform	: packoffset(c0);	
	float g_fFilterThreshold				: packoffset(c4);
	float g_fHistoryFactor					: packoffset(c4.y);
	float3 g_vFogLight						: packoffset(c5);
	float g_fMultiScattering				: packoffset(c5.w);
};

/*===========================================================================
Shader inputs
===========================================================================*/
struct VS_POLYGONAL_INPUT
{
	float4 vPos : POSITION;
};

struct HS_POLYGONAL_INPUT
{
	float4 vPos : SV_POSITION;
	float4 vWorldPos : TEXCOORD0;
	float4 vClipPos : TEXCOORD1;
};

struct HS_POLYGONAL_CONTROL_POINT_OUTPUT
{
	float4 vWorldPos : TEXCOORD0;
	float4 vClipPos : TEXCOORD1;
};

struct HS_POLYGONAL_CONSTANT_DATA_OUTPUT
{
    float fEdges[4] : SV_TESSFACTOR;
    float fInside[2] : SV_INSIDETESSFACTOR;
    float debug[4] : TEXCOORD2;
};

struct PS_POLYGONAL_INPUT
{
    float4 vPos : SV_POSITION;
    float4 vWorldPos : TEXCOORD0;
#ifdef __PSSL__
	float dummy : CLIPPPOSDUMMY;  //Workaround for compiler exception in polygon hull shaders.
#endif
};

struct VS_QUAD_OUTPUT
{
	float4 vPos : SV_POSITION;
	sample float4 vWorldPos : TEXCOORD0;
	sample float2 vTex : TEXCOORD1;
};

/*===========================================================================
Common functions
===========================================================================*/

float LinearizeDepth(float d, float zn, float zf)
{
	return d * zn / (zf - ((zf - zn) * d));
}

float WarpDepth(float z, float zn, float zf)
{
	return z * (1+zf/zn) / (1+z*zf/zn);
}

float MapDepth(float d, float zn, float zf)
{
	return (d - zn) / (zf - zn);
}

// Approximates a non-normalized gaussian with Sigma == 1
float GaussianApprox(float2 sample_pos, float width)
{
	float x_sqr = sample_pos.x*sample_pos.x + sample_pos.y*sample_pos.y;
	// exp(-0.5*(x/w)^2) ~ (1-(x/(8*w))^2)^32
	float w = saturate(1.0f - x_sqr/(64.0f * width*width));
	w = w*w;	// ^2
	w = w*w;	// ^4
	w = w*w;	// ^8
	w = w*w;	// ^16
	w = w*w;	// ^32
	return w;
}

#if defined(ATTENUATIONMODE)
float AttenuationFunc(float d)
{
    if (ATTENUATIONMODE == ATTENUATIONMODE_POLYNOMIAL)
    {
        // 1-(A+Bx+Cx^2)
        return saturate(1.0f - (g_vLightAttenuationFactors.x + g_vLightAttenuationFactors.y*d + g_vLightAttenuationFactors.z*d*d));
    }
    else if (ATTENUATIONMODE == ATTENUATIONMODE_INV_POLYNOMIAL)
    {
        // 1 / (A+Bx+Cx^2) + D
        return saturate(1.0f / (g_vLightAttenuationFactors.x + g_vLightAttenuationFactors.y*d + g_vLightAttenuationFactors.z*d*d) + g_vLightAttenuationFactors.w);
    }
    else //if (ATTENUATIONMODE == ATTENUATIONMODE_NONE)
    {
        return 1.0f;
    }
}
#endif

float3 GetPhaseFactor(Texture2D tex, float cos_theta)
{
    float2 tc;
    tc.x = 0;
    tc.y = acos(clamp(-cos_theta, -1.0f, 1.0f)) / PI;
    return g_vScatterPower*tex.SampleLevel(sBilinear, tc, 0).rgb;
}
