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

#include "common.h"
#include "scene.h"

#include <Nv/VolumetricLighting/NvVolumetricLighting.h>

////////////////////////////////////////////////////////////////////////////////
const uint32_t Scene::SHADOWMAP_RESOLUTION = 1024;
const float Scene::LIGHT_RANGE = 50.0f;
const float Scene::SPOTLIGHT_FALLOFF_ANGLE = Nv::NvPi / 4.0f;
const float Scene::SPOTLIGHT_FALLOFF_POWER = 1.0f;

Scene::Scene()
{
    gwvlctx_ = nullptr;

    isCtxValid_ = false;
    debugMode_ = Nv::Vl::DebugFlags::NONE;
    isPaused_ = false;

    viewpoint_ = 0;
    lightPower_ = 0;
    mediumType_ = 0;
    lightMode_ = eLightMode::SPOTLIGHT;

    sceneTransform_ = Nv::NvMat44(Nv::NvIdentity);
    lightTransform_ = Nv::NvMat44(Nv::NvIdentity);

    //--------------------------------------------------------------------------
    // Default context settings
    contextDesc_.framebuffer.uWidth = 0;
    contextDesc_.framebuffer.uHeight = 0;
    contextDesc_.framebuffer.uSamples = 0;
    contextDesc_.eDownsampleMode = Nv::Vl::DownsampleMode::FULL;
    contextDesc_.eInternalSampleMode = Nv::Vl::MultisampleMode::SINGLE;
    contextDesc_.eFilterMode = Nv::Vl::FilterMode::NONE;

    //--------------------------------------------------------------------------
    // Default post-process settings
    postprocessDesc_.bDoFog = true;
    postprocessDesc_.bIgnoreSkyFog = false;
    postprocessDesc_.eUpsampleQuality = Nv::Vl::UpsampleQuality::BILINEAR;
    postprocessDesc_.fBlendfactor = 1.0f;
    postprocessDesc_.fTemporalFactor = 0.95f;
    postprocessDesc_.fFilterThreshold = 0.20f;
}

void Scene::Release()
{
    if (gwvlctx_) Nv::Vl::ReleaseContext(gwvlctx_);
    isCtxValid_ = false;
}

bool Scene::isCtxValid()
{
    return isCtxValid_;
}

void Scene::invalidateCtx()
{
    isCtxValid_ = false;
}

void Scene::createCtx(const Nv::Vl::PlatformDesc * platform)
{
    // Assumes platformDesc and contextDesc have been updated
    if (gwvlctx_) Nv::Vl::ReleaseContext(gwvlctx_);
    Nv::Vl::Status gwvl_status = Nv::Vl::CreateContext(gwvlctx_, platform, &contextDesc_);
    ASSERT_LOG(gwvl_status == Nv::Vl::Status::OK, "GWVL Error: %d", gwvl_status);
    gwvl_status;
    isCtxValid_ = true;
}

void Scene::updateFramebuffer(uint32_t w, uint32_t h, uint32_t s)
{        
    contextDesc_.framebuffer.uWidth = w;
    contextDesc_.framebuffer.uHeight = h;
    contextDesc_.framebuffer.uSamples = s;
    invalidateCtx();
}

void Scene::setDebugMode(Nv::Vl::DebugFlags d)
{
    debugMode_ = d;
}

void Scene::togglePause()
{
    isPaused_ = !isPaused_;
}

void Scene::toggleDownsampleMode()
{
    switch (contextDesc_.eDownsampleMode)
    {
    case Nv::Vl::DownsampleMode::FULL:
        contextDesc_.eDownsampleMode = Nv::Vl::DownsampleMode::HALF;
        break;

    case Nv::Vl::DownsampleMode::HALF:
        contextDesc_.eDownsampleMode = Nv::Vl::DownsampleMode::QUARTER;
        break;

    case Nv::Vl::DownsampleMode::QUARTER:
        contextDesc_.eDownsampleMode = Nv::Vl::DownsampleMode::FULL;
        break;
    }
    invalidateCtx();
}

void Scene::toggleMsaaMode()
{
    if (contextDesc_.eDownsampleMode == Nv::Vl::DownsampleMode::FULL)
        return;

    switch (contextDesc_.eInternalSampleMode)
    {
    case Nv::Vl::MultisampleMode::SINGLE:
        contextDesc_.eInternalSampleMode = Nv::Vl::MultisampleMode::MSAA2;
        break;

    case Nv::Vl::MultisampleMode::MSAA2:
        contextDesc_.eInternalSampleMode = Nv::Vl::MultisampleMode::MSAA4;
        break;

    case Nv::Vl::MultisampleMode::MSAA4:
        contextDesc_.eInternalSampleMode = Nv::Vl::MultisampleMode::SINGLE;
        break;
    }
    invalidateCtx();
}

void Scene::toggleFiltering()
{
    if (contextDesc_.eDownsampleMode == Nv::Vl::DownsampleMode::FULL)
        return;

    contextDesc_.eFilterMode = (contextDesc_.eFilterMode == Nv::Vl::FilterMode::TEMPORAL) ? Nv::Vl::FilterMode::NONE : Nv::Vl::FilterMode::TEMPORAL;
    invalidateCtx();
}

void Scene::toggleUpsampleMode()
{
    if (contextDesc_.eDownsampleMode == Nv::Vl::DownsampleMode::FULL)
        return;

    switch (postprocessDesc_.eUpsampleQuality)
    {
    case Nv::Vl::UpsampleQuality::POINT:
        postprocessDesc_.eUpsampleQuality = Nv::Vl::UpsampleQuality::BILINEAR;
        break;

    case Nv::Vl::UpsampleQuality::BILINEAR:
        if (contextDesc_.eFilterMode == Nv::Vl::FilterMode::TEMPORAL)
            postprocessDesc_.eUpsampleQuality = Nv::Vl::UpsampleQuality::BILATERAL;
        else
            postprocessDesc_.eUpsampleQuality = Nv::Vl::UpsampleQuality::POINT;
        break;

    case Nv::Vl::UpsampleQuality::BILATERAL:
        postprocessDesc_.eUpsampleQuality = Nv::Vl::UpsampleQuality::POINT;
        break;
    }
}

void Scene::toggleFog()
{
    postprocessDesc_.bDoFog = !postprocessDesc_.bDoFog;
}

void Scene::toggleViewpoint()
{
    viewpoint_ = (viewpoint_+1)%4;
}

void Scene::toggleIntensity()
{
    lightPower_ = (lightPower_+1)%6;
}

void Scene::toggleMediumType()
{
    mediumType_ = (mediumType_+1)%4;
}


void Scene::toggleLightMode()
{
    switch (lightMode_)
    {
    default:
    case Scene::eLightMode::DIRECTIONAL:
        lightMode_ = Scene::eLightMode::SPOTLIGHT;
        break;

    case Scene::eLightMode::SPOTLIGHT:
        lightMode_ = Scene::eLightMode::OMNI;
        break;

    case Scene::eLightMode::OMNI:
        lightMode_ = Scene::eLightMode::DIRECTIONAL;
        break;
    }
}

Scene::eLightMode Scene::getLightMode()
{
    return lightMode_;
}

void Scene::getLightViewpoint(Nv::NvVec3 & vPos, Nv::NvMat44 & mViewProj)
{
    Nv::NvVec3 vOrigin = Nv::NvVec3(0, 0, 0);
    Nv::NvVec3 vUp = Nv::NvVec3(0, 1, 0);
    switch (getLightMode())
    {
        case Scene::eLightMode::OMNI:
        {
            vPos = lightTransform_.transform(Nv::NvVec3(15, 10, 0));
            Nv::NvMat44 mLightView = Nv::NvMat44(Nv::NvVec3(1,0,0), Nv::NvVec3(0,1,0), Nv::NvVec3(0,0,1), -vPos);
            Nv::NvMat44 mLightProj = Nv::NvMat44(Nv::NvIdentity);
            mViewProj = mLightProj*mLightView;
        }
        break;

        case Scene::eLightMode::SPOTLIGHT:
        {
            vPos = Nv::NvVec3(10, 15, 5);
            Nv::NvMat44 mLightView = LookAtTransform(vPos, vOrigin, vUp);
            Nv::NvMat44 mLightProj = PerspectiveProjLH(
                SPOTLIGHT_FALLOFF_ANGLE,
                1.0f,
                0.50f, LIGHT_RANGE);
            mViewProj = mLightProj*mLightView;
        }
        break;

        default:
        case eLightMode::DIRECTIONAL:
        {
            vPos = Nv::NvVec3(10, 15, 5);
            Nv::NvMat44 mLightView = LookAtTransform(vPos, vOrigin, vUp);
            Nv::NvMat44 mLightProj = OrthographicProjLH(25.0f, 25.0f, 0.50f, LIGHT_RANGE);

            mViewProj = mLightProj*mLightView;
        }
        break;
    }
}

Nv::NvVec3 Scene::getLightIntensity()
{
    const Nv::NvVec3 LIGHT_POWER[] = {
        1.00f*Nv::NvVec3(1.00f, 0.95f, 0.90f),
        0.50f*Nv::NvVec3(1.00f, 0.95f, 0.90f),
        1.50f*Nv::NvVec3(1.00f, 0.95f, 0.90f),
        1.00f*Nv::NvVec3(1.00f, 0.75f, 0.50f),
        1.00f*Nv::NvVec3(0.75f, 1.00f, 0.75f),
        1.00f*Nv::NvVec3(0.50f, 0.75f, 1.00f)
    };

    switch (getLightMode())
    {
        case Scene::eLightMode::OMNI:
            return 25000.0f*LIGHT_POWER[lightPower_];

        case Scene::eLightMode::SPOTLIGHT:
            return 50000.0f*LIGHT_POWER[lightPower_];

        default:
        case eLightMode::DIRECTIONAL:
            return 250.0f*LIGHT_POWER[lightPower_];
    }
}

const Nv::Vl::ViewerDesc * Scene::getViewerDesc()
{
    static Nv::Vl::ViewerDesc viewerDesc;
    Nv::NvVec3 vOrigin = Nv::NvVec3(0, 0, 0);
    Nv::NvVec3 vUp = Nv::NvVec3(0, 1, 0);
    Nv::NvVec3 VIEWPOINTS[] = {
        Nv::NvVec3(0, 0, -17.5f),
        Nv::NvVec3(0, 17.5f, 0),
        Nv::NvVec3(0, -17.5f, 0),
        Nv::NvVec3(17.5f, 0, 0),
    };
    Nv::NvVec3 VIEWPOINT_UP[] = {
        Nv::NvVec3(0, 1, 0),
        Nv::NvVec3(0, 0, -1),
        Nv::NvVec3(0, 0, -1),
        Nv::NvVec3(0, 1, 0),
    };
    Nv::NvVec3 vEyePos = VIEWPOINTS[viewpoint_];
    Nv::NvMat44 mEyeView = LookAtTransform(vEyePos, vOrigin, VIEWPOINT_UP[viewpoint_]);
    Nv::NvMat44 mEyeProj = PerspectiveProjLH(
        Nv::NvPiDivFour,
        static_cast<float>(contextDesc_.framebuffer.uWidth)/static_cast<float>(contextDesc_.framebuffer.uHeight),
        0.50f, 50.0f );
    Nv::NvMat44 mEyeViewProj = mEyeProj*mEyeView;

    viewerDesc.mProj = NVtoNVC(mEyeProj);
    viewerDesc.mViewProj = NVtoNVC(mEyeViewProj);
    viewerDesc.vEyePosition = NVtoNVC(vEyePos);
    viewerDesc.uViewportWidth = contextDesc_.framebuffer.uWidth;
    viewerDesc.uViewportHeight = contextDesc_.framebuffer.uHeight;

    return &viewerDesc;
}

const Nv::Vl::LightDesc * Scene::getLightDesc()
{
    static Nv::Vl::LightDesc lightDesc;
    const float LIGHT_RANGE = 50.0f;

    Nv::NvVec3 vLightPos;
    Nv::NvMat44 mLightViewProj;
    getLightViewpoint(vLightPos, mLightViewProj);
    Nv::NvMat44 mLightViewProj_Inv = Inverse(mLightViewProj);

    lightDesc.vIntensity = NVtoNVC(getLightIntensity());
    lightDesc.mLightToWorld = NVtoNVC(mLightViewProj_Inv);

    switch (getLightMode())
    {
        case Scene::eLightMode::OMNI:
        {
            lightDesc.eType = Nv::Vl::LightType::OMNI;
            lightDesc.Omni.fZNear = 0.5f;
            lightDesc.Omni.fZFar = LIGHT_RANGE;
            lightDesc.Omni.vPosition = NVtoNVC(vLightPos);
            lightDesc.Omni.eAttenuationMode = Nv::Vl::AttenuationMode::INV_POLYNOMIAL;
            const float LIGHT_SOURCE_RADIUS = 0.5f; // virtual radius of a spheroid light source
            lightDesc.Omni.fAttenuationFactors[0] = 1.0f;
            lightDesc.Omni.fAttenuationFactors[1] = 2.0f / LIGHT_SOURCE_RADIUS;
            lightDesc.Omni.fAttenuationFactors[2] = 1.0f / (LIGHT_SOURCE_RADIUS*LIGHT_SOURCE_RADIUS);
            lightDesc.Omni.fAttenuationFactors[3] = 0.0f;
        }
        break;

        case Scene::eLightMode::SPOTLIGHT:
        {
            Nv::NvVec3 vLightDirection = -vLightPos;
            vLightDirection.normalize();
            lightDesc.eType = Nv::Vl::LightType::SPOTLIGHT;
            lightDesc.Spotlight.fZNear = 0.5f;
            lightDesc.Spotlight.fZFar = LIGHT_RANGE;
            lightDesc.Spotlight.eFalloffMode = Nv::Vl::SpotlightFalloffMode::FIXED;
            lightDesc.Spotlight.fFalloff_Power = SPOTLIGHT_FALLOFF_POWER;
            lightDesc.Spotlight.fFalloff_CosTheta = Nv::intrinsics::cos(SPOTLIGHT_FALLOFF_ANGLE);
            lightDesc.Spotlight.vDirection = NVtoNVC(vLightDirection);
            lightDesc.Spotlight.vPosition = NVtoNVC(vLightPos);
            lightDesc.Spotlight.eAttenuationMode = Nv::Vl::AttenuationMode::INV_POLYNOMIAL;
            const float LIGHT_SOURCE_RADIUS = 1.0f;  // virtual radius of a spheroid light source
            lightDesc.Spotlight.fAttenuationFactors[0] = 1.0f;
            lightDesc.Spotlight.fAttenuationFactors[1] = 2.0f / LIGHT_SOURCE_RADIUS;
            lightDesc.Spotlight.fAttenuationFactors[2] = 1.0f / (LIGHT_SOURCE_RADIUS*LIGHT_SOURCE_RADIUS);
            lightDesc.Spotlight.fAttenuationFactors[3] = 0.0f;
        }
        break;

        default:
        case eLightMode::DIRECTIONAL:
        {
            Nv::NvVec3 vLightDirection = -vLightPos;
            vLightDirection.normalize();
            lightDesc.eType = Nv::Vl::LightType::DIRECTIONAL;
            lightDesc.Directional.vDirection = NVtoNVC(vLightDirection);
        }
    }
    return &lightDesc;
}

const Nv::Vl::MediumDesc * Scene::getMediumDesc()
{
    static Nv::Vl::MediumDesc mediumDesc;

    const float SCATTER_PARAM_SCALE = 0.0001f;
    mediumDesc.uNumPhaseTerms = 0;

    uint32_t t = 0;

    mediumDesc.PhaseTerms[t].ePhaseFunc = Nv::Vl::PhaseFunctionType::RAYLEIGH;
    mediumDesc.PhaseTerms[t].vDensity = NVtoNVC(10.00f * SCATTER_PARAM_SCALE * Nv::NvVec3(0.596f, 1.324f, 3.310f));
    t++;

    switch (mediumType_)
    {
    default:
    case 0:
        mediumDesc.PhaseTerms[t].ePhaseFunc = Nv::Vl::PhaseFunctionType::HENYEYGREENSTEIN;
        mediumDesc.PhaseTerms[t].vDensity = NVtoNVC(10.00f * SCATTER_PARAM_SCALE * Nv::NvVec3(1.00f, 1.00f, 1.00f));
        mediumDesc.PhaseTerms[t].fEccentricity = 0.85f;
        t++;
        mediumDesc.vAbsorption = NVtoNVC(5.0f * SCATTER_PARAM_SCALE * Nv::NvVec3(1, 1, 1));
        break;

    case 1:
        mediumDesc.PhaseTerms[t].ePhaseFunc = Nv::Vl::PhaseFunctionType::HENYEYGREENSTEIN;
        mediumDesc.PhaseTerms[t].vDensity = NVtoNVC(15.00f * SCATTER_PARAM_SCALE * Nv::NvVec3(1.00f, 1.00f, 1.00f));
        mediumDesc.PhaseTerms[t].fEccentricity = 0.60f;
        t++;
        mediumDesc.vAbsorption = NVtoNVC(25.0f * SCATTER_PARAM_SCALE * Nv::NvVec3(1, 1, 1));
        break;

    case 2:
        mediumDesc.PhaseTerms[t].ePhaseFunc = Nv::Vl::PhaseFunctionType::MIE_HAZY;
        mediumDesc.PhaseTerms[t].vDensity = NVtoNVC(20.00f * SCATTER_PARAM_SCALE * Nv::NvVec3(1.00f, 1.00f, 1.00f));
        t++;
        mediumDesc.vAbsorption = NVtoNVC(25.0f * SCATTER_PARAM_SCALE * Nv::NvVec3(1, 1, 1));
        break;

    case 3:
        mediumDesc.PhaseTerms[t].ePhaseFunc = Nv::Vl::PhaseFunctionType::MIE_MURKY;
        mediumDesc.PhaseTerms[t].vDensity = NVtoNVC(30.00f * SCATTER_PARAM_SCALE * Nv::NvVec3(1.00f, 1.00f, 1.00f));
        t++;
        mediumDesc.vAbsorption = NVtoNVC(50.0f * SCATTER_PARAM_SCALE * Nv::NvVec3(1, 1, 1));
        break;
    }

    mediumDesc.uNumPhaseTerms = t;    

    return &mediumDesc;
}

const Nv::Vl::PostprocessDesc * Scene::getPostprocessDesc()
{
    postprocessDesc_.vFogLight = NVtoNVC(getLightIntensity());
    postprocessDesc_.fMultiscatter = 0.000002f;
    return &postprocessDesc_;
}

void Scene::beginAccumulation(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformShaderResource sceneDepth)
{
    Nv::Vl::Status gwvl_status;
    gwvl_status = Nv::Vl::BeginAccumulation(gwvlctx_, ctx, sceneDepth, getViewerDesc(), getMediumDesc(), debugMode_);
    ASSERT_LOG(gwvl_status == Nv::Vl::Status::OK, "GWVL Error: %d", gwvl_status);
}

void Scene::renderVolume(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformShaderResource shadowmap)
{
    Nv::NvVec3 vLightPos;
    Nv::NvMat44 mLightViewProj;
    getLightViewpoint(vLightPos, mLightViewProj);

    Nv::Vl::ShadowMapDesc shadowmapDesc;
    {
        shadowmapDesc.eType = (getLightMode() == Scene::eLightMode::OMNI) ? Nv::Vl::ShadowMapLayout::PARABOLOID : Nv::Vl::ShadowMapLayout::SIMPLE;
        shadowmapDesc.uWidth = SHADOWMAP_RESOLUTION;
        shadowmapDesc.uHeight = SHADOWMAP_RESOLUTION;
        shadowmapDesc.uElementCount = 1;
        shadowmapDesc.Elements[0].uOffsetX = 0;
        shadowmapDesc.Elements[0].uOffsetY = 0;
        shadowmapDesc.Elements[0].uWidth = shadowmapDesc.uWidth;
        shadowmapDesc.Elements[0].uHeight = shadowmapDesc.uHeight;
        shadowmapDesc.Elements[0].mViewProj = NVtoNVC(mLightViewProj);
        shadowmapDesc.Elements[0].mArrayIndex = 0;
        if (getLightMode() == Scene::eLightMode::OMNI)
        {
            shadowmapDesc.uElementCount = 2;
            shadowmapDesc.Elements[1].uOffsetX = 0;
            shadowmapDesc.Elements[1].uOffsetY = 0;
            shadowmapDesc.Elements[1].uWidth = shadowmapDesc.uWidth;
            shadowmapDesc.Elements[1].uHeight = shadowmapDesc.uHeight;
            shadowmapDesc.Elements[1].mViewProj = NVtoNVC(mLightViewProj);
            shadowmapDesc.Elements[1].mArrayIndex = 1;
        }
    }

    Nv::Vl::VolumeDesc volumeDesc;
    {
        volumeDesc.fTargetRayResolution = 12.0f;
        volumeDesc.uMaxMeshResolution = SHADOWMAP_RESOLUTION;
        volumeDesc.fDepthBias = 0.0f;
        volumeDesc.eTessQuality = Nv::Vl::TessellationQuality::HIGH;
    }

    Nv::Vl::Status gwvl_status;
    gwvl_status = Nv::Vl::RenderVolume(gwvlctx_, ctx, shadowmap, &shadowmapDesc, getLightDesc(), &volumeDesc);
    ASSERT_LOG(gwvl_status == Nv::Vl::Status::OK, "GWVL Error: %d", gwvl_status);
}

void Scene::endAccumulation(Nv::Vl::PlatformRenderCtx ctx)
{
    Nv::Vl::Status gwvl_status;
    gwvl_status = Nv::Vl::EndAccumulation(gwvlctx_, ctx);
    ASSERT_LOG(gwvl_status == Nv::Vl::Status::OK, "GWVL Error: %d", gwvl_status);
}

void Scene::applyLighting(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformRenderTarget sceneRT, Nv::Vl::PlatformShaderResource sceneDepth)
{
    Nv::Vl::Status gwvl_status;
    gwvl_status = Nv::Vl::ApplyLighting(gwvlctx_, ctx, sceneRT, sceneDepth, getPostprocessDesc());
    ASSERT_LOG(gwvl_status == Nv::Vl::Status::OK, "GWVL Error: %d", gwvl_status);
}

void Scene::animate(float dt)
{
    static uint64_t time_elapsed_us = 0;

    // Update the scene
    if (!isPaused_)
    {
        time_elapsed_us += static_cast<uint64_t>(dt * 1000000.0);
        if (lightMode_ != eLightMode::OMNI)
        {
            float angle = float(dt) * Nv::NvPiDivFour;
            sceneTransform_ = sceneTransform_ * Nv::NvMat44(
                Nv::NvVec4(Nv::intrinsics::cos(angle), 0, -Nv::intrinsics::sin(angle), 0),
                Nv::NvVec4(0, 1, 0, 0),
                Nv::NvVec4(Nv::intrinsics::sin(angle), 0, Nv::intrinsics::cos(angle), 0),
                Nv::NvVec4(0, 0, 0, 1)
                );
        }
        else
        {
            sceneTransform_ = Nv::NvMat44(Nv::NvIdentity);
        }

        if (lightMode_ == eLightMode::OMNI)
        {
            const uint64_t CYCLE_LENGTH = 60000000;
            float cyclePhase = static_cast<float>(time_elapsed_us % CYCLE_LENGTH) / static_cast<float>(CYCLE_LENGTH);
            lightTransform_ = Nv::NvMat44(
                Nv::NvVec4(Nv::intrinsics::cos(Nv::NvTwoPi*7.0f*cyclePhase), 0, 0, 0),
                Nv::NvVec4(0, Nv::intrinsics::cos(Nv::NvTwoPi*3.0f*cyclePhase), 0, 0),
                Nv::NvVec4(0, 0, 1, 0),
                Nv::NvVec4(0, 0, 0, 1)
                );
        }
    }
}

void Scene::setupObjectCB(ObjectCB * pObject, const Nv::NvVec3 & offset)
{
    pObject->mTransform = sceneTransform_*Nv::NvTransform(offset);
    pObject->vColor[0] = 1.0f;
    pObject->vColor[1] = 1.0f;
    pObject->vColor[2] = 1.0f;
}

void Scene::setupLightViewCB(ViewCB * pView)
{
    getLightViewpoint(pView->vEyePos, pView->mProj);
    pView->zNear = 0.50f;
    pView->zFar = LIGHT_RANGE;
}

void Scene::setupSceneCBs(ViewCB * pView, LightCB * pLight)
{
    const Nv::Vl::ViewerDesc * viewerDesc = getViewerDesc();
    pView->mProj = *reinterpret_cast<const Nv::NvMat44 *>(&viewerDesc->mViewProj);
    pView->vEyePos = *reinterpret_cast<const Nv::NvVec3 *>(&viewerDesc->vEyePosition);
    pView->zNear = 0.50f;
    pView->zFar = 50.0f;

    const Nv::Vl::LightDesc * lightDesc = getLightDesc();
    getLightViewpoint(pLight->vLightPos, pLight->mLightViewProj);
    pLight->vLightDirection = (-pLight->vLightPos);
    pLight->vLightDirection.normalize();
    pLight->fLightFalloffCosTheta = Nv::intrinsics::cos(SPOTLIGHT_FALLOFF_ANGLE);
    pLight->fLightFalloffPower = SPOTLIGHT_FALLOFF_POWER;
    pLight->vLightColor = getLightIntensity();
    if (lightMode_ == eLightMode::SPOTLIGHT)
    {
        pLight->vLightAttenuationFactors = Nv::NvVec4(lightDesc->Spotlight.fAttenuationFactors);
    }
    else if (lightMode_ == eLightMode::OMNI)
    {
        pLight->vLightAttenuationFactors = Nv::NvVec4(lightDesc->Omni.fAttenuationFactors);
    }
    pLight->zNear = 0.50f;
    pLight->zFar = LIGHT_RANGE;

    const Nv::Vl::MediumDesc * mediumDesc = getMediumDesc();
    pLight->vSigmaExtinction = *reinterpret_cast<const Nv::NvVec3 *>(&mediumDesc->vAbsorption);
    for (uint32_t t = 0; t < mediumDesc->uNumPhaseTerms; ++t)
    {
        pLight->vSigmaExtinction = pLight->vSigmaExtinction + *reinterpret_cast<const Nv::NvVec3 *>(&mediumDesc->PhaseTerms[t].vDensity);
    }
}

////////////////////////////////////////////////////////////////////////////////
