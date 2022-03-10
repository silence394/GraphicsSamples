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

#ifndef CONTEXT_COMMON_H
#define CONTEXT_COMMON_H
////////////////////////////////////////////////////////////////////////////////

#include <Nv/VolumetricLighting/NvVolumetricLighting.h>

#include "common.h"

/*==============================================================================
   Forward Declarations
==============================================================================*/

#pragma warning(disable: 4100)
////////////////////////////////////////////////////////////////////////////////
namespace Nv { namespace VolumetricLighting {
////////////////////////////////////////////////////////////////////////////////

/*==============================================================================
    Constants
==============================================================================*/
const uint32_t MAX_JITTER_STEPS = 8;

// These need to match the values in ComputeLightLUT_CS.hlsl
const uint32_t LIGHT_LUT_DEPTH_RESOLUTION = 128;
const uint32_t LIGHT_LUT_WDOTV_RESOLUTION = 512;

/*==============================================================================
    Base Context Implementation
==============================================================================*/

class ContextImp_Common
{
public:
    // Clean up common resources
    virtual ~ContextImp_Common() {};

    //--------------------------------------------------------------------------
    // API Hooks
    Status BeginAccumulation(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc, DebugFlags debugFlags);
    Status RenderVolume(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);
    Status EndAccumulation(PlatformRenderCtx renderCtx);
    Status ApplyLighting(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc);

protected:

    //--------------------------------------------------------------------------
    // Implementation Stubs

    // BeginAccumulation
    virtual Status BeginAccumulation_Start(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc) = 0;
    virtual Status BeginAccumulation_UpdateMediumLUT(PlatformRenderCtx renderCtx) = 0;
    virtual Status BeginAccumulation_CopyDepth(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth) = 0;
    virtual Status BeginAccumulation_End(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc) = 0;

    // RenderVolume
    virtual Status RenderVolume_Start(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc) = 0;
    virtual Status RenderVolume_DoVolume_Directional(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc) = 0;
    virtual Status RenderVolume_DoVolume_Spotlight(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc) = 0;
    virtual Status RenderVolume_DoVolume_Omni(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc) = 0;
    virtual Status RenderVolume_End(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc) = 0;

    // EndAccumulation
    virtual Status EndAccumulation_Imp(PlatformRenderCtx renderCtx) = 0;

    // ApplyLighting
    virtual Status ApplyLighting_Start(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc) = 0;
    virtual Status ApplyLighting_Resolve(PlatformRenderCtx renderCtx, PostprocessDesc const* pPostprocessDesc) = 0;
    virtual Status ApplyLighting_TemporalFilter(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc) = 0;
    virtual Status ApplyLighting_Composite(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc) = 0;
    virtual Status ApplyLighting_End(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc) = 0;

    //--------------------------------------------------------------------------
    // Constructors
    // Protected constructor - Should only ever instantiate children
    ContextImp_Common() {};

    // Call this from base-class for actual initialization
    ContextImp_Common(const ContextDesc * pContextDesc);

    //--------------------------------------------------------------------------
    // Helper functions
    NvVec2 getJitter() const;
    uint32_t getOutputBufferWidth() const;
    uint32_t getOutputBufferHeight() const;
    uint32_t getOutputViewportWidth() const;
    uint32_t getOutputViewportHeight() const;
    uint32_t getOutputSampleCount() const;
    float getInternalScale() const;
    uint32_t getInternalBufferWidth() const;
    uint32_t getInternalBufferHeight() const;
    uint32_t getInternalViewportWidth() const;
    uint32_t getInternalViewportHeight() const;
    uint32_t getInternalSampleCount() const;
    bool isOutputMSAA() const;
    bool isInternalMSAA() const;
    FilterMode getFilterMode() const;
    uint32_t getCoarseResolution(const VolumeDesc * pVolumeDesc) const;


    //--------------------------------------------------------------------------
    // Constant Buffers

    struct PerContextCB
    {
        // c0
        NvVec2 vOutputSize;
        NvVec2 vOutputSize_Inv;
        // c1
        NvVec2 vBufferSize;
        NvVec2 vBufferSize_Inv;
        // c2
        float fResMultiplier;
        uint32_t uSampleCount;
        float pad1[2];
    };
    void SetupCB_PerContext(PerContextCB * cb);

    struct PerFrameCB
    {
        // c0+4
        NvMat44 mProj;
        // c4+4
        NvMat44 mViewProj;
        // c8+4
        NvMat44 mViewProj_Inv;
        // c12
        NvVec2 vOutputViewportSize;
        NvVec2 vOutputViewportSize_Inv;
        // c13
        NvVec2 vViewportSize;
        NvVec2 vViewportSize_Inv;
        // c14
        NvVec3 vEyePosition;
        float pad1[1];
        // c15
        NvVec2 vJitterOffset;
        float fZNear;
        float fZFar;
        // c16
        NvVec3 vScatterPower;
        uint32_t uNumPhaseTerms;
        // c17
        NvVec3 vSigmaExtinction;
        float pad2[1];
        // c18+MAX_PHASE_TERMS (4)
        uint32_t uPhaseFunc[MAX_PHASE_TERMS][4];
        // c22
        NvVec4 vPhaseParams[MAX_PHASE_TERMS];
    };
    void SetupCB_PerFrame(ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc, PerFrameCB * cb);

    struct PerVolumeCB
    {
        // c0+4
        NvMat44 mLightToWorld;
        // c4
        float fLightFalloffAngle;
        float fLightFalloffPower;
        float fGridSectionSize;
        float fLightToEyeDepth;
        // c5
        float fLightZNear;
        float fLightZFar;
        float pad[2];
        // c6
        NvVec4 vAttenuationFactors;
        // c7+16
        NvMat44 mLightProj[4];
        // c23+16
        NvMat44 mLightProj_Inv[4];
        // c39
        NvVec3 vLightDir;
        float fDepthBias;
        // c40
        NvVec3 vLightPos;
        uint32_t uMeshResolution;
        // c41
        NvVec3 vLightIntensity;
        float fTargetRaySize;
        // c42+4
        NvVec4 vElementOffsetAndScale[4];
        // c46
        NvVec4 vShadowMapDim;
        // c47+4
        // Only first index of each "row" is used.
        // (Need to do this because HLSL can only stride arrays by full offset)
        uint32_t uElementIndex[4][4];
    };
    void SetupCB_PerVolume(ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc, PerVolumeCB * cb);

    struct PerApplyCB
    {
        // c0
        NvMat44 mHistoryXform;
        // c4
        float fFilterThreshold;
        float fHistoryFactor;
        float pad1[2];
        // c5
        NvVec3 vFogLight;
        float fMultiScattering;
    };    
    void SetupCB_PerApply(PostprocessDesc const* pPostprocessDesc, PerApplyCB * cb);

    //--------------------------------------------------------------------------
    // Miscellaneous internal state
    bool isInitialized_;
    uint32_t debugFlags_;
    ContextDesc contextDesc_;
    ViewerDesc viewerDesc_;
    uint32_t jitterIndex_;
    int32_t lastFrameIndex_;
    int32_t nextFrameIndex_;
    NvMat44 lastViewProj_;
    NvMat44 nextViewProj_;
};

////////////////////////////////////////////////////////////////////////////////
}; /*namespace VolumetricLighting*/ }; /*namespace Nv*/
////////////////////////////////////////////////////////////////////////////////
#endif // CONTEXT_COMMON_H
