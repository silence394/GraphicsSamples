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

#ifndef CONTEXT_D3D11_H
#define CONTEXT_D3D11_H
////////////////////////////////////////////////////////////////////////////////
#include <Nv/VolumetricLighting/NvVolumetricLighting.h>

#include <vector> // TODO: remove this dependency
#include "context_common.h"
/*==============================================================================
   Forward Declarations
==============================================================================*/
// D3D11 Types
struct ID3D11VertexShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11GeometryShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
struct ID3D11InputLayout;
struct ID3D11Buffer;

// Helper Types
class Texture2D;
class RenderTarget;
class DepthTarget;
template <class T> class ConstantBuffer;

////////////////////////////////////////////////////////////////////////////////
namespace Nv { namespace VolumetricLighting {
////////////////////////////////////////////////////////////////////////////////

class ContextImp_D3D11 : public ContextImp_Common
{
public:
    // Creates the context and resources
    static Status Create(ContextImp_D3D11 ** out_ctx, const PlatformDesc * pPlatformDesc, const ContextDesc * pContextDesc);

	// Clean up the context on delete
	virtual ~ContextImp_D3D11();

protected:
	// BeginAccumulation
    Status BeginAccumulation_Start(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc);
	Status BeginAccumulation_UpdateMediumLUT(PlatformRenderCtx renderCtx);
	Status BeginAccumulation_CopyDepth(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth);
	Status BeginAccumulation_End(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc);

	// RenderVolume
    Status RenderVolume_Start(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);
	Status RenderVolume_DoVolume_Directional(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);
	Status RenderVolume_DoVolume_Spotlight(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);
    Status RenderVolume_DoVolume_Omni(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);
	Status RenderVolume_End(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc);

	// EndAccumulation
	Status EndAccumulation_Imp(PlatformRenderCtx renderCtx);

	// ApplyLighting
    Status ApplyLighting_Start(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc);
	Status ApplyLighting_Resolve(PlatformRenderCtx renderCtx, PostprocessDesc const* pPostprocessDesc);
	Status ApplyLighting_TemporalFilter(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc);
	Status ApplyLighting_Composite(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc);
	Status ApplyLighting_End(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc);

private:
	// Private default constructor
	// Should never be called
	ContextImp_D3D11() {};

	// Initializing Destructor
	// Setup all code that can't fail
	ContextImp_D3D11(const ContextDesc * pContextDesc);

    // Called by Create internally
    Status CreateResources(ID3D11Device * device);

    // Do a fullscreen pass with the bound pixel-shader
    Status DrawFullscreen(ID3D11DeviceContext * dxCtx);

    Status DrawFrustumGrid(ID3D11DeviceContext * dxCtx, uint32_t resolution);
    Status DrawFrustumBase(ID3D11DeviceContext * dxCtx, uint32_t resolution);
    Status DrawFrustumCap(ID3D11DeviceContext * dxCtx, uint32_t resolution);
    Status DrawOmniVolume(ID3D11DeviceContext * dxCtx, uint32_t resolution);

	struct
	{
		ID3D11PixelShader ** Apply_PS;
        ID3D11ComputeShader ** ComputeLightLUT_CS;
		ID3D11PixelShader ** ComputePhaseLookup_PS;
		ID3D11PixelShader ** Debug_PS;
		ID3D11PixelShader ** DownsampleDepth_PS;
		ID3D11VertexShader ** Quad_VS;
		ID3D11VertexShader ** RenderVolume_VS;
		ID3D11HullShader ** RenderVolume_HS;
		ID3D11DomainShader ** RenderVolume_DS;
		ID3D11PixelShader ** RenderVolume_PS;
		ID3D11PixelShader ** Resolve_PS;
		ID3D11PixelShader ** TemporalFilter_PS;
	} shaders;

    ConstantBuffer<PerContextCB> * pPerContextCB;
    ConstantBuffer<PerFrameCB> * pPerFrameCB;
    ConstantBuffer<PerVolumeCB> * pPerVolumeCB;
    ConstantBuffer<PerApplyCB> * pPerApplyCB;

	DepthTarget * pDepth_;
    RenderTarget * pPhaseLUT_;
	RenderTarget * pLightLUT_P_[2];
    RenderTarget * pLightLUT_S1_[2];
    RenderTarget * pLightLUT_S2_[2];
	RenderTarget * pAccumulation_;
	RenderTarget * pResolvedAccumulation_;
	RenderTarget * pResolvedDepth_;
	RenderTarget * pFilteredAccumulation_[2];
	RenderTarget * pFilteredDepth_[2];

    RenderTarget * pAccumulatedOutput_;

    struct
    {
        struct
        {
            ID3D11RasterizerState * cull_none;
            ID3D11RasterizerState * cull_front;
            ID3D11RasterizerState * wireframe;
        } rs;

        struct
        {
            ID3D11SamplerState * point;
            ID3D11SamplerState * linear;
        } ss;

        struct  
        {
            ID3D11DepthStencilState * no_depth;
            ID3D11DepthStencilState * write_only_depth;
            ID3D11DepthStencilState * read_only_depth;
            ID3D11DepthStencilState * render_volume;
            ID3D11DepthStencilState * render_volume_boundary;
            ID3D11DepthStencilState * remove_volume_base;
            ID3D11DepthStencilState * render_volume_cap;
            ID3D11DepthStencilState * finish_volume;
        } ds;

        struct
        {
            ID3D11BlendState * no_color;
            ID3D11BlendState * no_blending;
            ID3D11BlendState * additive;
            ID3D11BlendState * additive_modulate;
            ID3D11BlendState * debug_blend;
        } bs;
    } states;
};

////////////////////////////////////////////////////////////////////////////////
}; /*namespace VolumetricLighting*/ }; /*namespace Nv*/
////////////////////////////////////////////////////////////////////////////////
#endif // CONTEXT_D3D11_H
