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

#include <Nv/VolumetricLighting/NvVolumetricLighting.h>

#include "common.h"
#include "context_common.h"

////////////////////////////////////////////////////////////////////////////////
namespace Nv { namespace VolumetricLighting {
////////////////////////////////////////////////////////////////////////////////

ContextImp_Common::ContextImp_Common(const ContextDesc * pContextDesc)
{
    isInitialized_ = false;
    jitterIndex_ = 0;
    lastFrameIndex_ = -1;
    nextFrameIndex_ = -0;
    intrinsics::memCopy(&this->contextDesc_, pContextDesc, sizeof(ContextDesc));
}

/*==============================================================================
    API Hooks
==============================================================================*/

    
Status ContextImp_Common::BeginAccumulation(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc, DebugFlags debugFlags)
{
    debugFlags_ = (uint32_t) debugFlags;
    intrinsics::memCopy(&viewerDesc_, pViewerDesc, sizeof(ViewerDesc));
	V_( BeginAccumulation_Start(renderCtx, sceneDepth, pViewerDesc, pMediumDesc) );
	V_( BeginAccumulation_UpdateMediumLUT(renderCtx) );
	V_( BeginAccumulation_CopyDepth(renderCtx, sceneDepth) );
	V_( BeginAccumulation_End(renderCtx, sceneDepth, pViewerDesc, pMediumDesc) );
	return Status::OK;
}

Status ContextImp_Common::RenderVolume(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
	V_( RenderVolume_Start(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc) );
	switch (pLightDesc->eType)
	{
	case LightType::DIRECTIONAL:
		V_( RenderVolume_DoVolume_Directional(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc) );
		break;

	case LightType::SPOTLIGHT:
		V_( RenderVolume_DoVolume_Spotlight(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc) );
		break;

    case LightType::OMNI:
        V_( RenderVolume_DoVolume_Omni(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc) );
        break;
            
	default:
        // Error -- unsupported light type
		return Status::INVALID_PARAMETER;
		break;
	}
	V_( RenderVolume_End(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc) );
	return Status::OK;
}

Status ContextImp_Common::EndAccumulation(PlatformRenderCtx renderCtx)
{
	V_( EndAccumulation_Imp(renderCtx) );
	return Status::OK;
}

Status ContextImp_Common::ApplyLighting(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    V_( ApplyLighting_Start(renderCtx, sceneTarget, sceneDepth, pPostprocessDesc) );
	if (getFilterMode() == FilterMode::TEMPORAL)
	{
		V_( ApplyLighting_Resolve(renderCtx, pPostprocessDesc) );
		V_( ApplyLighting_TemporalFilter(renderCtx, sceneDepth, pPostprocessDesc) );
	}
	else if (isInternalMSAA())
	{
		V_( ApplyLighting_Resolve(renderCtx, pPostprocessDesc) );
	}
	V_( ApplyLighting_Composite(renderCtx, sceneTarget, sceneDepth, pPostprocessDesc) );
	V_( ApplyLighting_End(renderCtx, sceneTarget, sceneDepth, pPostprocessDesc) );

    // Update frame counters as needed
    jitterIndex_ = (jitterIndex_ + 1) % MAX_JITTER_STEPS;
    lastFrameIndex_ = nextFrameIndex_;
    nextFrameIndex_ = (nextFrameIndex_ + 1) % 2;
	return Status::OK;
}

/*==============================================================================
    Helper methods
==============================================================================*/

uint32_t ContextImp_Common::getOutputBufferWidth() const
{
    return contextDesc_.framebuffer.uWidth;
}

uint32_t ContextImp_Common::getOutputBufferHeight() const
{
    return contextDesc_.framebuffer.uHeight;
}

uint32_t ContextImp_Common::getOutputViewportWidth() const
{
    return viewerDesc_.uViewportWidth;
}

uint32_t ContextImp_Common::getOutputViewportHeight() const
{
    return viewerDesc_.uViewportHeight;
}

uint32_t ContextImp_Common::getOutputSampleCount() const
{
    return contextDesc_.framebuffer.uSamples;
}

float ContextImp_Common::getInternalScale() const
{
    switch (contextDesc_.eDownsampleMode)
    {
    default:
    case DownsampleMode::FULL:
        return 1.00f;

    case DownsampleMode::HALF:
        return 0.50f;

    case DownsampleMode::QUARTER:
        return 0.25f;
    }
}

uint32_t ContextImp_Common::getInternalBufferWidth() const
{
    switch (contextDesc_.eDownsampleMode)
    {
    default:
    case DownsampleMode::FULL:
        return contextDesc_.framebuffer.uWidth;

    case DownsampleMode::HALF:
        return contextDesc_.framebuffer.uWidth >> 1;

    case DownsampleMode::QUARTER:
        return contextDesc_.framebuffer.uWidth >> 2;
    }
}

uint32_t ContextImp_Common::getInternalBufferHeight() const
{
    switch (contextDesc_.eDownsampleMode)
    {
    default:
    case DownsampleMode::FULL:
        return contextDesc_.framebuffer.uHeight;

    case DownsampleMode::HALF:
        return contextDesc_.framebuffer.uHeight >> 1;

    case DownsampleMode::QUARTER:
        return contextDesc_.framebuffer.uHeight >> 2;
    }
}

uint32_t ContextImp_Common::getInternalViewportWidth() const
{
    switch (contextDesc_.eDownsampleMode)
    {
    default:
    case DownsampleMode::FULL:
        return viewerDesc_.uViewportWidth;

    case DownsampleMode::HALF:
        return viewerDesc_.uViewportWidth >> 1;

    case DownsampleMode::QUARTER:
        return viewerDesc_.uViewportWidth >> 2;
    }
}

uint32_t ContextImp_Common::getInternalViewportHeight() const
{
    switch (contextDesc_.eDownsampleMode)
    {
    default:
    case DownsampleMode::FULL:
        return viewerDesc_.uViewportHeight;

    case DownsampleMode::HALF:
        return viewerDesc_.uViewportHeight >> 1;

    case DownsampleMode::QUARTER:
        return viewerDesc_.uViewportHeight >> 2;
    }
}

uint32_t ContextImp_Common::getInternalSampleCount() const
{
    switch (contextDesc_.eInternalSampleMode)
    {
    default:
    case MultisampleMode::SINGLE:
        return 1;

    case MultisampleMode::MSAA2:
        return 2;

    case MultisampleMode::MSAA4:
        return 4;
    }
}

bool ContextImp_Common::isOutputMSAA() const
{
    return (getOutputSampleCount() > 1);
}

bool ContextImp_Common::isInternalMSAA() const
{
    return (getInternalSampleCount() > 1);
}

FilterMode ContextImp_Common::getFilterMode() const
{
    return contextDesc_.eFilterMode;
}

NvVec2 ContextImp_Common::getJitter() const
{
    if (getFilterMode() == FilterMode::TEMPORAL)
    {
        auto HaltonSequence = [](int index, int base) -> float
        {
            float result = 0;
            float f = 1;
            int i = index + 1;
            while (i > 0)
            {
                f = f / float(base);
                result += f * (i % base);
                i = i / base;
            }
            return result;
        };

        return NvVec2((HaltonSequence(jitterIndex_, 2) - 0.5f), (HaltonSequence(jitterIndex_, 3) - 0.5f));
    }
    else
    {
        return NvVec2(0, 0);
    }
}

uint32_t ContextImp_Common::getCoarseResolution(const VolumeDesc *pVolumeDesc) const
{
    switch (pVolumeDesc->eTessQuality)
    {
    default:
    case TessellationQuality::HIGH:
        return pVolumeDesc->uMaxMeshResolution / 64;

    case TessellationQuality::MEDIUM:
        return pVolumeDesc->uMaxMeshResolution / 32;

    case TessellationQuality::LOW:
        return pVolumeDesc->uMaxMeshResolution / 16;
    }
}
/*==============================================================================
    Constant Buffer Setup
==============================================================================*/

void ContextImp_Common::SetupCB_PerContext(PerContextCB * cb)
{
    cb->vOutputSize = NvVec2(static_cast<float>(getOutputBufferWidth()), static_cast<float>(getOutputBufferHeight()));
    cb->vOutputSize_Inv = NvVec2(1.f / cb->vOutputSize.x, 1.f / cb->vOutputSize.y);
    cb->vBufferSize = NvVec2(static_cast<float>(getInternalBufferWidth()), static_cast<float>(getInternalBufferHeight()));
    cb->vBufferSize_Inv = NvVec2(1.f / cb->vBufferSize.x, 1.f / cb->vBufferSize.y);
    cb->fResMultiplier = 1.f / getInternalScale();
    cb->uSampleCount = getInternalSampleCount();
}

void ContextImp_Common::SetupCB_PerFrame(ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc, PerFrameCB * cb)
{
    cb->mProj = NVCtoNV(pViewerDesc->mProj);
    cb->mViewProj = NVCtoNV(pViewerDesc->mViewProj);
    cb->mViewProj_Inv = Inverse(cb->mViewProj);
    cb->vOutputViewportSize = NvVec2(static_cast<float>(getOutputViewportWidth()), static_cast<float>(getOutputViewportHeight()));
    cb->vOutputViewportSize_Inv = NvVec2(1.f / cb->vOutputViewportSize.x, 1.f / cb->vOutputViewportSize.y);
    cb->vViewportSize = NvVec2(static_cast<float>(getInternalViewportWidth()), static_cast<float>(getInternalViewportHeight()));
    cb->vViewportSize_Inv = NvVec2(1.f / cb->vViewportSize.x, 1.f / cb->vViewportSize.y);
    cb->vEyePosition = NVCtoNV(pViewerDesc->vEyePosition);
    cb->vJitterOffset = getJitter();
    float proj_33 = cb->mProj(2, 2);
    float proj_34 = cb->mProj(2, 3);
    cb->fZNear = -proj_34 / proj_33;
    cb->fZFar = proj_34 / (1.0f - proj_33);
    
    const float SCATTER_EPSILON = 0.000001f;
    NvVec3 total_scatter = NvVec3(SCATTER_EPSILON, SCATTER_EPSILON, SCATTER_EPSILON);
    cb->uNumPhaseTerms = pMediumDesc->uNumPhaseTerms;
    for (uint32_t p = 0; p < pMediumDesc->uNumPhaseTerms; ++p)
    {
        cb->uPhaseFunc[p][0] = static_cast<uint32_t>(pMediumDesc->PhaseTerms[p].ePhaseFunc);
        NvVec3 density = NVCtoNV(pMediumDesc->PhaseTerms[p].vDensity);
        cb->vPhaseParams[p] = NvVec4(density.x, density.y, density.z, pMediumDesc->PhaseTerms[p].fEccentricity);
        total_scatter += density;
    }
    NvVec3 absorption = NVCtoNV(pMediumDesc->vAbsorption);
    cb->vScatterPower.x = 1-exp(-total_scatter.x);
    cb->vScatterPower.y = 1-exp(-total_scatter.y);
    cb->vScatterPower.z = 1-exp(-total_scatter.z);
    cb->vSigmaExtinction = total_scatter + absorption;
}

void ContextImp_Common::SetupCB_PerVolume(ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc, PerVolumeCB * cb)
{
    cb->mLightToWorld = NVCtoNV(pLightDesc->mLightToWorld);
    cb->vLightIntensity = NVCtoNV(pLightDesc->vIntensity);
    switch (pLightDesc->eType)
    {
    case LightType::DIRECTIONAL:
        cb->vLightDir = NVCtoNV(pLightDesc->Directional.vDirection);
        break;

    case LightType::SPOTLIGHT:
        cb->vLightDir = NVCtoNV(pLightDesc->Spotlight.vDirection);
        cb->vLightPos = NVCtoNV(pLightDesc->Spotlight.vPosition);
        cb->fLightZNear = pLightDesc->Spotlight.fZNear;
        cb->fLightZFar = pLightDesc->Spotlight.fZFar;
        cb->fLightFalloffAngle = pLightDesc->Spotlight.fFalloff_CosTheta;
        cb->fLightFalloffPower = pLightDesc->Spotlight.fFalloff_Power;
        cb->vAttenuationFactors = *reinterpret_cast<const NvVec4 *>(pLightDesc->Spotlight.fAttenuationFactors);
        break;

    case LightType::OMNI:
        cb->vLightPos = NVCtoNV(pLightDesc->Omni.vPosition);
        cb->fLightZNear = pLightDesc->Omni.fZNear;
        cb->fLightZFar = pLightDesc->Omni.fZFar;
        cb->vAttenuationFactors = *reinterpret_cast<const NvVec4 *>(pLightDesc->Omni.fAttenuationFactors);
    default:
        break;
    };
    cb->fDepthBias = pVolumeDesc->fDepthBias;

    cb->uMeshResolution = getCoarseResolution(pVolumeDesc);

    NvVec4 vw1 = cb->mLightToWorld.transform(NvVec4(-1, -1,  1, 1));
    NvVec4 vw2 = cb->mLightToWorld.transform(NvVec4( 1,  1,  1, 1));
    vw1 = vw1 / vw1.w;
    vw2 = vw2 / vw2.w;
    float fCrossLength = ((vw1).getXYZ() - (vw2).getXYZ()).magnitude();
    float fSideLength = sqrtf(0.5f*fCrossLength*fCrossLength);
    cb->fGridSectionSize = fSideLength / static_cast<float>(cb->uMeshResolution);
    cb->fTargetRaySize = pVolumeDesc->fTargetRayResolution;

    for (int i=0;i<MAX_SHADOWMAP_ELEMENTS;++i)
    {
        cb->vElementOffsetAndScale[i].x = (float)pShadowMapDesc->Elements[i].uOffsetX / pShadowMapDesc->uWidth;
        cb->vElementOffsetAndScale[i].y = (float)pShadowMapDesc->Elements[i].uOffsetY / pShadowMapDesc->uHeight;
        cb->vElementOffsetAndScale[i].z = (float)pShadowMapDesc->Elements[i].uWidth   / pShadowMapDesc->uWidth;
        cb->vElementOffsetAndScale[i].w = (float)pShadowMapDesc->Elements[i].uHeight  / pShadowMapDesc->uHeight;
        cb->mLightProj[i] = NVCtoNV(pShadowMapDesc->Elements[i].mViewProj);
        cb->mLightProj_Inv[i] = Inverse(cb->mLightProj[i]);
        cb->uElementIndex[i][0] = pShadowMapDesc->Elements[i].mArrayIndex;
    }

    cb->vShadowMapDim.x = static_cast<float>(pShadowMapDesc->uWidth);
    cb->vShadowMapDim.y = static_cast<float>(pShadowMapDesc->uHeight);
}

void ContextImp_Common::SetupCB_PerApply(PostprocessDesc const* pPostprocessDesc, PerApplyCB * cb)
{
    if (getFilterMode() == FilterMode::TEMPORAL)
    {
        cb->fHistoryFactor = (lastFrameIndex_ == -1) ? 0.0f : pPostprocessDesc->fTemporalFactor;
        cb->fFilterThreshold = pPostprocessDesc->fFilterThreshold;
        if (lastFrameIndex_ == -1)
        {
            lastViewProj_ = NVCtoNV(pPostprocessDesc->mUnjitteredViewProj);
            lastFrameIndex_ = (nextFrameIndex_+1)%2;
        }
        else
        {
            lastViewProj_ = nextViewProj_;
        }
        nextViewProj_ = NVCtoNV(pPostprocessDesc->mUnjitteredViewProj);
        cb->mHistoryXform = lastViewProj_ * Inverse(nextViewProj_);
    }
    else
    {
        cb->mHistoryXform = NvMat44(NvIdentity);
        cb->fFilterThreshold = 0.0f;
        cb->fHistoryFactor = 0.0f;
    }
    cb->vFogLight = NVCtoNV(pPostprocessDesc->vFogLight);
    cb->fMultiScattering = pPostprocessDesc->fMultiscatter;
}

////////////////////////////////////////////////////////////////////////////////
}; /*namespace VolumetricLighting*/ }; /*namespace Nv*/
////////////////////////////////////////////////////////////////////////////////
