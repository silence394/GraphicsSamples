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

#ifndef SCENE_H
#define SCENE_H
////////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include <Nv/VolumetricLighting/NvVolumetricLighting.h>
////////////////////////////////////////////////////////////////////////////////
class Scene
{
public:
    static const uint32_t SHADOWMAP_RESOLUTION;
    static const float LIGHT_RANGE;
    static const float SPOTLIGHT_FALLOFF_ANGLE;
    static const float SPOTLIGHT_FALLOFF_POWER;

    enum class eLightMode
    {
        UNKNOWN = -1,
        DIRECTIONAL,
        SPOTLIGHT,
        OMNI,
        COUNT
    };

    struct ViewCB
    {
        Nv::NvMat44 mProj;
        Nv::NvVec3 vEyePos;
        float pad1[1];
        float zNear;
        float zFar;
        float pad2[2];
    };

    struct ObjectCB
    {
        Nv::NvMat44 mTransform;
        float vColor[3];
        float pad[1];
    };

    struct LightCB
    {
        Nv::NvMat44 mLightViewProj;
        Nv::NvVec3 vLightDirection;
        float fLightFalloffCosTheta;
        Nv::NvVec3 vLightPos;
        float fLightFalloffPower;
        Nv::NvVec3 vLightColor;
        float pad1[1];
        Nv::NvVec4 vLightAttenuationFactors;
        float zNear;
        float zFar;
        float pad2[2];
        Nv::NvVec3 vSigmaExtinction;
        float pad3[1];
    };

    Scene();
    void Release();

    bool isCtxValid();
    void invalidateCtx();
    void createCtx(const Nv::Vl::PlatformDesc * platform);

    void updateFramebuffer(uint32_t w, uint32_t h, uint32_t s);

    void setDebugMode(Nv::Vl::DebugFlags d);
    void togglePause();
    void toggleDownsampleMode();
    void toggleMsaaMode();
    void toggleFiltering();
    void toggleUpsampleMode();
    void toggleFog();
    void toggleViewpoint();
    void toggleIntensity();
    void toggleMediumType();
    void toggleLightMode();

    eLightMode getLightMode();
    void getLightViewpoint(Nv::NvVec3 & vPos, Nv::NvMat44 & mViewProj);
    Nv::NvVec3 getLightIntensity();

    const Nv::Vl::ViewerDesc * getViewerDesc();
    const Nv::Vl::LightDesc * getLightDesc();
    const Nv::Vl::MediumDesc * getMediumDesc();
    const Nv::Vl::PostprocessDesc * getPostprocessDesc();

    void beginAccumulation(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformShaderResource sceneDepth);
    void renderVolume(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformShaderResource shadowmap);
    void endAccumulation(Nv::Vl::PlatformRenderCtx ctx);
    void applyLighting(Nv::Vl::PlatformRenderCtx ctx, Nv::Vl::PlatformRenderTarget sceneRT, Nv::Vl::PlatformShaderResource sceneDepth);

    void setupObjectCB(ObjectCB * pObject, const Nv::NvVec3 & offset);
    void setupLightViewCB(ViewCB * cb);
    void setupSceneCBs(ViewCB * view_cb, LightCB * light_cb);
    void animate(float dt);

private:
    Scene(const Scene & rhs) {rhs;};

    Nv::Vl::Context gwvlctx_;
    uint32_t shadowmapRes_;

    bool isCtxValid_;
    bool isPaused_;

    eLightMode lightMode_;

    Nv::Vl::DebugFlags debugMode_;
    uint32_t viewpoint_;
    uint32_t lightPower_;
    uint32_t mediumType_;

    Nv::NvMat44 sceneTransform_;
    Nv::NvMat44 lightTransform_;

    Nv::Vl::ContextDesc contextDesc_;
    Nv::Vl::PostprocessDesc postprocessDesc_;
};

////////////////////////////////////////////////////////////////////////////////
#endif // SCENE_H
