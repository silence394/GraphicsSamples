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
#include "context_d3d11.h"
#include "compiled_shaders_d3d11.h"
#include "d3d11_util.h"

#include <stdint.h>
#include <d3d11.h>

#pragma warning(disable: 4100)
////////////////////////////////////////////////////////////////////////////////
namespace Nv { namespace VolumetricLighting {
////////////////////////////////////////////////////////////////////////////////
const uint8_t STENCIL_REF = 0xFF;

/*==============================================================================
   Create a context and load/allocate/generate resources
==============================================================================*/

Status ContextImp_D3D11::Create(ContextImp_D3D11 ** out_ctx, const PlatformDesc * pPlatformDesc, const ContextDesc * pContextDesc)
{
    ID3D11Device * pDevice = pPlatformDesc->d3d11.pDevice;
    if (pDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
        return Status::UNSUPPORTED_DEVICE;

	*out_ctx = new ContextImp_D3D11(pContextDesc);
    Status result = (*out_ctx)->CreateResources(pDevice);
    if (result != Status::OK)
    {
        delete *out_ctx;
        *out_ctx = nullptr;
        return result;
    }
    else
    {
        return Status::OK;
    }
}

Status ContextImp_D3D11::CreateResources(ID3D11Device * device)
{
    //--------------------------------------------------------------------------
	// Shaders
#   define LOAD_SHADERS(x) LoadShaders(device, ##x##::permutation_code, ##x##::permutation_length, shaders::##x##::PERMUTATION_ENTRY_COUNT, shaders.##x)
	VD3D_(LOAD_SHADERS(Apply_PS));
    VD3D_(LOAD_SHADERS(ComputeLightLUT_CS));
	VD3D_(LOAD_SHADERS(ComputePhaseLookup_PS));
	VD3D_(LOAD_SHADERS(Debug_PS));
	VD3D_(LOAD_SHADERS(DownsampleDepth_PS));
	VD3D_(LOAD_SHADERS(Quad_VS));
	VD3D_(LOAD_SHADERS(RenderVolume_VS));
	VD3D_(LOAD_SHADERS(RenderVolume_HS));
	VD3D_(LOAD_SHADERS(RenderVolume_DS));
	VD3D_(LOAD_SHADERS(RenderVolume_PS));
	VD3D_(LOAD_SHADERS(Resolve_PS));
	VD3D_(LOAD_SHADERS(TemporalFilter_PS));
#   undef LOAD_SHADERS

    //--------------------------------------------------------------------------
    // Constant Buffers
    V_CREATE(pPerContextCB, ConstantBuffer<PerContextCB>::Create(device));
    V_CREATE(pPerFrameCB, ConstantBuffer<PerFrameCB>::Create(device));
    V_CREATE(pPerVolumeCB, ConstantBuffer<PerVolumeCB>::Create(device));
    V_CREATE(pPerApplyCB, ConstantBuffer<PerApplyCB>::Create(device));

    //--------------------------------------------------------------------------
    // Shader Resources
    V_CREATE(pDepth_, DepthTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), getInternalSampleCount(), DXGI_FORMAT_D24_UNORM_S8_UINT, 1, "NvVl::Depth"));
    V_CREATE(pPhaseLUT_, RenderTarget::Create(device, 1, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Phase LUT"));
    V_CREATE(pLightLUT_P_[0], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Point [0]"));
    V_CREATE(pLightLUT_P_[1], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Point [1]"));
    V_CREATE(pLightLUT_S1_[0], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Spot 1 [0]"));
    V_CREATE(pLightLUT_S1_[1], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Spot 1 [1]"));
    V_CREATE(pLightLUT_S2_[0], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Spot 2 [0]"));
    V_CREATE(pLightLUT_S2_[1], RenderTarget::Create(device, LIGHT_LUT_DEPTH_RESOLUTION, LIGHT_LUT_WDOTV_RESOLUTION, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Light LUT Spot 2 [1]"));
    V_CREATE(pAccumulation_, RenderTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), getInternalSampleCount(), DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Accumulation"));

    if (isInternalMSAA() || getFilterMode() == FilterMode::TEMPORAL)
    {
        V_CREATE(pResolvedAccumulation_, RenderTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Resolved Accumulation"));
        V_CREATE(pResolvedDepth_, RenderTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), 1, DXGI_FORMAT_R16G16_FLOAT, "NvVl::Resolved Depth"));
    }

    if (getFilterMode() == FilterMode::TEMPORAL)
    {
    	for (int i=0; i<2; ++i)
    	{
            V_CREATE(pFilteredDepth_[i], RenderTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), 1, DXGI_FORMAT_R16G16_FLOAT, "NvVl::Filtered Depth"));
            V_CREATE(pFilteredAccumulation_[i], RenderTarget::Create(device, getInternalBufferWidth(), getInternalBufferHeight(), 1, DXGI_FORMAT_R16G16B16A16_FLOAT, "NvVl::Filtered Accumulation"));
    	}
    }

    //--------------------------------------------------------------------------
    // States
    // Rasterizer State
    {
        CD3D11_RASTERIZER_DESC rsDesc((CD3D11_DEFAULT()));
        rsDesc.FrontCounterClockwise = TRUE;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.DepthClipEnable = FALSE;
        VD3D_(device->CreateRasterizerState(&rsDesc, &states.rs.cull_none));
    }
    {
        CD3D11_RASTERIZER_DESC rsDesc((CD3D11_DEFAULT()));
        rsDesc.FrontCounterClockwise = TRUE;
        rsDesc.CullMode = D3D11_CULL_FRONT;
        rsDesc.DepthClipEnable = FALSE;
        VD3D_(device->CreateRasterizerState(&rsDesc, &states.rs.cull_front));
    }
    {
        CD3D11_RASTERIZER_DESC rsDesc((CD3D11_DEFAULT()));
        rsDesc.FillMode = D3D11_FILL_WIREFRAME;
        rsDesc.FrontCounterClockwise = TRUE;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.DepthClipEnable = FALSE;
        VD3D_(device->CreateRasterizerState(&rsDesc, &states.rs.wireframe));
    }
    // Sampler State
    {
        CD3D11_SAMPLER_DESC ssDesc((CD3D11_DEFAULT()));
        ssDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        VD3D_(device->CreateSamplerState(&ssDesc, &states.ss.point));
    }
    {
        CD3D11_SAMPLER_DESC ssDesc((CD3D11_DEFAULT()));
        ssDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        VD3D_(device->CreateSamplerState(&ssDesc, &states.ss.linear));
    }
    // Depth-Stencil
    {
        // Depth Disabled
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthEnable = FALSE;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.no_depth));
    }
    {
        // Write-only Depth DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.write_only_depth));
    }
    {
        // Render Volume DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.StencilEnable = TRUE;
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
        dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
        dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.render_volume));
    }
    {
        // Render Volume Boundary DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.StencilEnable = TRUE;
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
        dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.render_volume_boundary));
    }
    {
        // Render volume cap DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthEnable = FALSE;
        dsDesc.StencilEnable = TRUE;
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
        dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
        dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.render_volume_cap));
    }
    {
        // Finish Volume DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthEnable = FALSE;
        dsDesc.StencilEnable = TRUE;
        dsDesc.StencilWriteMask = 0x00;
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_GREATER;
        dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.finish_volume));
    }
    {
        // Read-Only Depth DS
        CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        VD3D_(device->CreateDepthStencilState(&dsDesc, &states.ds.read_only_depth));
    }
    // Blend State
    {
        CD3D11_BLEND_DESC bsDesc((CD3D11_DEFAULT()));
        bsDesc.RenderTarget[0].RenderTargetWriteMask = 0x00000000;
        VD3D_(device->CreateBlendState(&bsDesc, &states.bs.no_color));
    }
    {
        CD3D11_BLEND_DESC bsDesc((CD3D11_DEFAULT()));
        VD3D_(device->CreateBlendState(&bsDesc, &states.bs.no_blending));
    }
    {
        CD3D11_BLEND_DESC bsDesc((CD3D11_DEFAULT()));
        bsDesc.RenderTarget[0].BlendEnable = TRUE;
        bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
        bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR;
        bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        VD3D_(device->CreateBlendState(&bsDesc, &states.bs.additive));
    }
    {
        CD3D11_BLEND_DESC bsDesc((CD3D11_DEFAULT()));
        bsDesc.RenderTarget[0].BlendEnable = TRUE;
        bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
        bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC1_COLOR;
        bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        VD3D_(device->CreateBlendState(&bsDesc, &states.bs.additive_modulate));
    }
    {
        CD3D11_BLEND_DESC bsDesc((CD3D11_DEFAULT()));
        bsDesc.RenderTarget[0].BlendEnable = TRUE;
        bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
        VD3D_(device->CreateBlendState(&bsDesc, &states.bs.debug_blend));
    }
    return Status::OK;
}

/*==============================================================================
   Constructor / Destructor
==============================================================================*/

ContextImp_D3D11::ContextImp_D3D11(const ContextDesc * pContextDesc) : ContextImp_Common(pContextDesc)
{
    shaders.Apply_PS = nullptr;
    shaders.ComputeLightLUT_CS = nullptr;
    shaders.ComputePhaseLookup_PS = nullptr;
    shaders.Debug_PS = nullptr;
    shaders.DownsampleDepth_PS = nullptr;
    shaders.Quad_VS = nullptr;
    shaders.RenderVolume_VS = nullptr;
    shaders.RenderVolume_HS = nullptr;
    shaders.RenderVolume_DS = nullptr;
    shaders.RenderVolume_PS = nullptr;
    shaders.Resolve_PS = nullptr;
    shaders.TemporalFilter_PS = nullptr;

    pPerContextCB = nullptr;
    pPerFrameCB = nullptr;
    pPerVolumeCB = nullptr;
    pPerApplyCB = nullptr;

    pDepth_ = nullptr;
    pPhaseLUT_ = nullptr;
    pLightLUT_P_[0] = nullptr;
    pLightLUT_P_[1] = nullptr;
    pLightLUT_S1_[0] = nullptr;
    pLightLUT_S1_[1] = nullptr;
    pLightLUT_S2_[0] = nullptr;
    pLightLUT_S2_[1] = nullptr;
    pAccumulation_ = nullptr;
    pResolvedAccumulation_ = nullptr;
    pResolvedDepth_ = nullptr;
    for (int i=0; i<2; ++i)
    {
        pFilteredAccumulation_[i] = nullptr;
        pFilteredDepth_[i] = nullptr;
    }

    states.rs.cull_none = nullptr;
    states.rs.cull_front = nullptr;
    states.rs.wireframe = nullptr;
    states.ss.point = nullptr;
    states.ss.linear = nullptr;
    states.ds.no_depth = nullptr;
    states.ds.write_only_depth = nullptr;
    states.ds.read_only_depth = nullptr;
    states.ds.render_volume = nullptr;
    states.ds.render_volume_boundary = nullptr;
    states.ds.render_volume_cap = nullptr;
    states.ds.finish_volume = nullptr;
    states.bs.no_color = nullptr;
    states.bs.no_blending = nullptr;
    states.bs.additive = nullptr;
    states.bs.additive_modulate = nullptr;
    states.bs.debug_blend = nullptr;
}

ContextImp_D3D11::~ContextImp_D3D11()
{
    SAFE_DELETE_ARRAY(shaders.Apply_PS);
    SAFE_DELETE_ARRAY(shaders.ComputeLightLUT_CS);
    SAFE_DELETE_ARRAY(shaders.ComputePhaseLookup_PS);
    SAFE_DELETE_ARRAY(shaders.Debug_PS);
    SAFE_DELETE_ARRAY(shaders.DownsampleDepth_PS);
    SAFE_DELETE_ARRAY(shaders.Quad_VS);
    SAFE_DELETE_ARRAY(shaders.RenderVolume_VS);
    SAFE_DELETE_ARRAY(shaders.RenderVolume_HS);
    SAFE_DELETE_ARRAY(shaders.RenderVolume_DS);
    SAFE_DELETE_ARRAY(shaders.RenderVolume_PS);
    SAFE_DELETE_ARRAY(shaders.Resolve_PS);
    SAFE_DELETE_ARRAY(shaders.TemporalFilter_PS);

    SAFE_DELETE(pPerContextCB);
    SAFE_DELETE(pPerFrameCB);
    SAFE_DELETE(pPerVolumeCB);
    SAFE_DELETE(pPerApplyCB);

    SAFE_DELETE(pDepth_);
    SAFE_DELETE(pPhaseLUT_);
    SAFE_DELETE(pLightLUT_P_[0]);
    SAFE_DELETE(pLightLUT_P_[1]);
    SAFE_DELETE(pLightLUT_S1_[0]);
    SAFE_DELETE(pLightLUT_S1_[1]);
    SAFE_DELETE(pLightLUT_S2_[0]);
    SAFE_DELETE(pLightLUT_S2_[1]);
    SAFE_DELETE(pAccumulation_);
    SAFE_DELETE(pResolvedAccumulation_);
    SAFE_DELETE(pResolvedDepth_);
    for (int i = 0; i < 2; ++i)
    {
        SAFE_DELETE(pFilteredAccumulation_[i]);
        SAFE_DELETE(pFilteredDepth_[i]);
    }

    SAFE_RELEASE(states.rs.cull_none);
    SAFE_RELEASE(states.rs.cull_front);
    SAFE_RELEASE(states.rs.wireframe);
    SAFE_RELEASE(states.ss.point);
    SAFE_RELEASE(states.ss.linear);
    SAFE_RELEASE(states.ds.no_depth);
    SAFE_RELEASE(states.ds.write_only_depth);
    SAFE_RELEASE(states.ds.read_only_depth);
    SAFE_RELEASE(states.ds.render_volume);
    SAFE_RELEASE(states.ds.render_volume_boundary);
    SAFE_RELEASE(states.ds.render_volume_cap);
    SAFE_RELEASE(states.ds.finish_volume);
    SAFE_RELEASE(states.bs.no_color);
    SAFE_RELEASE(states.bs.no_blending);
    SAFE_RELEASE(states.bs.additive);
    SAFE_RELEASE(states.bs.additive_modulate);
    SAFE_RELEASE(states.bs.debug_blend);
}

/*==============================================================================
   BeginAccumulation
==============================================================================*/

Status ContextImp_D3D11::BeginAccumulation_Start(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;

    NV_PERFEVENT_BEGIN(dxCtx, "NvVl::BeginAccumulation");

    if (!isInitialized_)
    {
        // Update the per-context constant buffer on the first frame it's used
        isInitialized_ = true;
        SetupCB_PerContext(pPerContextCB->Map(dxCtx));
        pPerContextCB->Unmap(dxCtx);
    }

    // Setup the constant buffer
    SetupCB_PerFrame(pViewerDesc, pMediumDesc, pPerFrameCB->Map(dxCtx));
    pPerFrameCB->Unmap(dxCtx);

    ID3D11Buffer* pCBs[] = {
        pPerContextCB->getCB(),
        pPerFrameCB->getCB(),
        nullptr, // pPerVolumeCB - Invalid
        nullptr // pPerApplyCB - Invalid
    };
    dxCtx->VSSetConstantBuffers(0, 4, pCBs);
    dxCtx->PSSetConstantBuffers(0, 4, pCBs);

    ID3D11SamplerState * pSamplers[] = {
        states.ss.point,
        states.ss.linear
    };
    dxCtx->PSSetSamplers(0, 2, pSamplers);

	return Status::OK;
}

Status ContextImp_D3D11::BeginAccumulation_UpdateMediumLUT(PlatformRenderCtx renderCtx)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    NV_PERFEVENT(dxCtx, "UpdateMediumLUT");

    FLOAT black[] = {0,0,0,0};
    dxCtx->ClearRenderTargetView(pPhaseLUT_->getRTV(), black);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = 1;
    viewport.Height = LIGHT_LUT_WDOTV_RESOLUTION;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView * RTVs[] = { pPhaseLUT_->getRTV() };
    dxCtx->OMSetRenderTargets(1, RTVs, nullptr);
    dxCtx->OMSetDepthStencilState(states.ds.no_depth, 0);
    dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);
    dxCtx->PSSetShader(shaders.ComputePhaseLookup_PS[ComputePhaseLookup_PS::SINGLE], nullptr, 0);
    V_( DrawFullscreen(dxCtx) );
	return Status::OK;
}

Status ContextImp_D3D11::BeginAccumulation_CopyDepth(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    ID3D11ShaderResourceView * sceneDepth_SRV = sceneDepth;
    NV_PERFEVENT(dxCtx, "CopyDepth");

    dxCtx->ClearDepthStencilView(pDepth_->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getInternalViewportWidth());
    viewport.Height = static_cast<float>(getInternalViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);
    
    dxCtx->PSSetShaderResources(0, 1, &sceneDepth_SRV);
    ID3D11RenderTargetView * NULL_RTVs[] = { nullptr };
    dxCtx->OMSetRenderTargets(1, NULL_RTVs, pDepth_->getDSV());
    dxCtx->OMSetDepthStencilState(states.ds.write_only_depth, 0);
    dxCtx->OMSetBlendState(states.bs.no_color, nullptr, 0xFFFFFFFF);
    DownsampleDepth_PS::Desc ps_desc = DownsampleDepth_PS::Desc(isOutputMSAA() ? DownsampleDepth_PS::SAMPLEMODE_MSAA : DownsampleDepth_PS::SAMPLEMODE_SINGLE);
    dxCtx->PSSetShader(shaders.DownsampleDepth_PS[ps_desc], nullptr, 0);
    V_( DrawFullscreen(dxCtx) );
    return Status::OK;
}

Status ContextImp_D3D11::BeginAccumulation_End(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    FLOAT black[] = {0,0,0,0};
    dxCtx->ClearRenderTargetView(pAccumulation_->getRTV(), black);

    NV_PERFEVENT_END(dxCtx);
	return Status::OK;
}

/*==============================================================================
   RenderVolume
==============================================================================*/
Status ContextImp_D3D11::RenderVolume_Start(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    NV_PERFEVENT_BEGIN(dxCtx, "NvVl::RenderVolume");

    // Setup the constant buffer
    SetupCB_PerVolume(pShadowMapDesc, pLightDesc, pVolumeDesc, pPerVolumeCB->Map(dxCtx));
    pPerVolumeCB->Unmap(dxCtx);

    dxCtx->ClearDepthStencilView(pDepth_->getDSV(), D3D11_CLEAR_STENCIL, 1.0f, STENCIL_REF);

    // Set up all the state common for this pass
    ID3D11Buffer * pCBs[] = {
        pPerContextCB->getCB(),
        pPerFrameCB->getCB(),
        pPerVolumeCB->getCB(),
        nullptr // pPerApplyCB: Invalid
    };
    dxCtx->VSSetConstantBuffers(0, 4, pCBs);
    dxCtx->HSSetConstantBuffers(0, 4, pCBs);
    dxCtx->DSSetConstantBuffers(0, 4, pCBs);
    dxCtx->PSSetConstantBuffers(0, 4, pCBs);
    dxCtx->CSSetConstantBuffers(0, 4, pCBs);

    ID3D11SamplerState * pSamplers[] = {
        states.ss.point,
        states.ss.linear
    };
    dxCtx->VSSetSamplers(0, 2, pSamplers);
    dxCtx->HSSetSamplers(0, 2, pSamplers);
    dxCtx->DSSetSamplers(0, 2, pSamplers);
    dxCtx->PSSetSamplers(0, 2, pSamplers);
    dxCtx->CSSetSamplers(0, 2, pSamplers);

	return Status::OK;
}

Status ContextImp_D3D11::RenderVolume_DoVolume_Directional(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    ID3D11ShaderResourceView * pShadowMap_SRV = shadowMap.d3d11;

    NV_PERFEVENT(dxCtx, "Directional");

    uint32_t mesh_resolution = getCoarseResolution(pVolumeDesc);

    //--------------------------------------------------------------------------
    // Draw tessellated grid

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getInternalViewportWidth());
    viewport.Height = static_cast<float>(getInternalViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);
    dxCtx->RSSetState(states.rs.cull_none);
    dxCtx->OMSetBlendState(states.bs.additive, nullptr, 0xFFFFFFFF);

    ID3D11RenderTargetView * RTVs[] = { pAccumulation_->getRTV() };
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getDSV());

    // Determine DS/HS permutation
    RenderVolume_HS::Desc hs_desc;
    hs_desc.flags.MAXTESSFACTOR = (RenderVolume_HS::eMAXTESSFACTOR) pVolumeDesc->eTessQuality;

    RenderVolume_DS::Desc ds_desc;

    switch (pShadowMapDesc->eType)
    {
    case ShadowMapLayout::SIMPLE:
    case ShadowMapLayout::CASCADE_ATLAS:
        hs_desc.flags.SHADOWMAPTYPE = RenderVolume_HS::SHADOWMAPTYPE_ATLAS;
        ds_desc.flags.SHADOWMAPTYPE = RenderVolume_DS::SHADOWMAPTYPE_ATLAS;
        break;

    case ShadowMapLayout::CASCADE_ARRAY:
        hs_desc.flags.SHADOWMAPTYPE = RenderVolume_HS::SHADOWMAPTYPE_ARRAY;
        ds_desc.flags.SHADOWMAPTYPE = RenderVolume_DS::SHADOWMAPTYPE_ARRAY;
        break;

    default:
        return Status::INVALID_PARAMETER;
    };

    switch (pShadowMapDesc->uElementCount)
    {
    case 0:
        if (pShadowMapDesc->eType != ShadowMapLayout::SIMPLE)
        {
            return Status::INVALID_PARAMETER;
        }
    case 1:
        hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_1;
        ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_1;
        break;

    case 2:
        hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_2;
        ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_2;
        break;

    case 3:
        hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_3;
        ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_3;
        break;

    case 4:
        hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_4;
        ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_4;
        break;

    default:
        return Status::INVALID_PARAMETER;
    };
    hs_desc.flags.VOLUMETYPE = RenderVolume_HS::VOLUMETYPE_FRUSTUM;
    ds_desc.flags.VOLUMETYPE = RenderVolume_DS::VOLUMETYPE_FRUSTUM;

    // Determine PS permutation
    RenderVolume_PS::Desc ps_desc;
    ps_desc.flags.SAMPLEMODE = isInternalMSAA() ? RenderVolume_PS::SAMPLEMODE_MSAA : RenderVolume_PS::SAMPLEMODE_SINGLE;
    ps_desc.flags.LIGHTMODE = RenderVolume_PS::LIGHTMODE_DIRECTIONAL;
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_GEOMETRY;
    ps_desc.flags.ATTENUATIONMODE = RenderVolume_PS::ATTENUATIONMODE_NONE; // unused for directional

    dxCtx->HSSetShader(shaders.RenderVolume_HS[hs_desc], nullptr, 0);
    dxCtx->DSSetShader(shaders.RenderVolume_DS[ds_desc], nullptr, 0);
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);

    ID3D11ShaderResourceView * SRVs[] = {
        nullptr,
        pShadowMap_SRV,
        nullptr,
        nullptr,
        pPhaseLUT_->getSRV(),
    };
    dxCtx->VSSetShaderResources(0, 5, SRVs);
    dxCtx->HSSetShaderResources(0, 5, SRVs);
    dxCtx->DSSetShaderResources(0, 5, SRVs);
    dxCtx->PSSetShaderResources(0, 5, SRVs);

    dxCtx->OMSetDepthStencilState(states.ds.render_volume, STENCIL_REF);

    DrawFrustumGrid(dxCtx, mesh_resolution);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);

    //--------------------------------------------------------------------------
    // Remove the illumination from the base of the scene (re-lit by sky later)
    DrawFrustumBase(dxCtx, mesh_resolution);

    //--------------------------------------------------------------------------
    // Render the bounds of the frustum
    if (debugFlags_ & (uint32_t)DebugFlags::WIREFRAME)
        return Status::OK;

    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_SKY;
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);
    dxCtx->OMSetDepthStencilState(states.ds.render_volume_boundary, STENCIL_REF);
    DrawFullscreen(dxCtx);

    //--------------------------------------------------------------------------
    // Finish the rendering by filling in stenciled gaps
    dxCtx->OMSetDepthStencilState(states.ds.finish_volume, STENCIL_REF);
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getReadOnlyDSV());
    SRVs[2] = pDepth_->getSRV();
    dxCtx->PSSetShaderResources(2, 1, &SRVs[2]);
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_FINAL;
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);
    DrawFullscreen(dxCtx);

	return Status::OK;
}

Status ContextImp_D3D11::RenderVolume_DoVolume_Spotlight(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    ID3D11ShaderResourceView * pShadowMap_SRV = shadowMap.d3d11;

    NV_PERFEVENT(dxCtx, "Spotlight");

    uint32_t mesh_resolution = getCoarseResolution(pVolumeDesc);

    ID3D11ShaderResourceView * SRVs[16] = { nullptr };
    ID3D11UnorderedAccessView * UAVs[16] = { nullptr };

    //--------------------------------------------------------------------------
    // Create look-up table
    if (pLightDesc->Spotlight.eFalloffMode == SpotlightFalloffMode::NONE)
    {
        NV_PERFEVENT(dxCtx, "Generate Light LUT");
        ComputeLightLUT_CS::Desc cs_desc;
        cs_desc.flags.LIGHTMODE = ComputeLightLUT_CS::LIGHTMODE_OMNI;
        cs_desc.flags.ATTENUATIONMODE = (ComputeLightLUT_CS::eATTENUATIONMODE) pLightDesc->Spotlight.eAttenuationMode;

        UAVs[0] = pLightLUT_P_[0]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
        SRVs[4] = pPhaseLUT_->getSRV();
        dxCtx->CSSetShaderResources(0, 6, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_CALCULATE;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(LIGHT_LUT_DEPTH_RESOLUTION / 32, LIGHT_LUT_WDOTV_RESOLUTION / 8, 1);

        UAVs[0] = pLightLUT_P_[1]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
        SRVs[5] = pLightLUT_P_[0]->getSRV();
        dxCtx->CSSetShaderResources(0, 6, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_SUM;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(1, LIGHT_LUT_WDOTV_RESOLUTION / 4, 1);

        dxCtx->CSSetShader(nullptr, nullptr, 0);
        UAVs[0] = nullptr;
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
    }
    else if (pLightDesc->Spotlight.eFalloffMode == SpotlightFalloffMode::FIXED)
    {
        NV_PERFEVENT(dxCtx, "Generate Light LUT");
        ComputeLightLUT_CS::Desc cs_desc;
        cs_desc.flags.LIGHTMODE = ComputeLightLUT_CS::LIGHTMODE_SPOTLIGHT;
        cs_desc.flags.ATTENUATIONMODE = (ComputeLightLUT_CS::eATTENUATIONMODE) pLightDesc->Spotlight.eAttenuationMode;

        UAVs[0] = pLightLUT_P_[0]->getUAV();
        UAVs[1] = pLightLUT_S1_[0]->getUAV();
        UAVs[2] = pLightLUT_S2_[0]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 3, UAVs, 0);
        SRVs[4] = pPhaseLUT_->getSRV();
        dxCtx->CSSetShaderResources(0, 8, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_CALCULATE;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(LIGHT_LUT_DEPTH_RESOLUTION / 32, LIGHT_LUT_WDOTV_RESOLUTION / 8, 1);

        UAVs[0] = pLightLUT_P_[1]->getUAV();
        UAVs[1] = pLightLUT_S1_[1]->getUAV();
        UAVs[2] = pLightLUT_S2_[1]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 3, UAVs, 0);
        SRVs[5] = pLightLUT_P_[0]->getSRV();
        SRVs[6] = pLightLUT_S1_[0]->getSRV();
        SRVs[7] = pLightLUT_S2_[0]->getSRV();
        dxCtx->CSSetShaderResources(0, 8, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_SUM;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(1, LIGHT_LUT_WDOTV_RESOLUTION / 4, 3);

        dxCtx->CSSetShader(nullptr, nullptr, 0);
        UAVs[0] = nullptr;
        UAVs[1] = nullptr;
        UAVs[2] = nullptr;
        dxCtx->CSSetUnorderedAccessViews(0, 3, UAVs, 0);
    }

    //--------------------------------------------------------------------------
    // Draw tessellated grid

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getInternalViewportWidth());
    viewport.Height = static_cast<float>(getInternalViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);
    dxCtx->RSSetState(states.rs.cull_none);
    dxCtx->OMSetBlendState(states.bs.additive, nullptr, 0xFFFFFFFF);

    ID3D11RenderTargetView * RTVs[] = { pAccumulation_->getRTV() };
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getDSV());

    // Determine DS/HS permutation
    RenderVolume_HS::Desc hs_desc;
    hs_desc.flags.SHADOWMAPTYPE = RenderVolume_HS::SHADOWMAPTYPE_ATLAS;
    hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_1;
    hs_desc.flags.VOLUMETYPE = RenderVolume_HS::VOLUMETYPE_FRUSTUM;
    hs_desc.flags.MAXTESSFACTOR = (RenderVolume_HS::eMAXTESSFACTOR) pVolumeDesc->eTessQuality;

    RenderVolume_DS::Desc ds_desc;
    ds_desc.flags.SHADOWMAPTYPE = RenderVolume_DS::SHADOWMAPTYPE_ATLAS;
    ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_1;
    ds_desc.flags.VOLUMETYPE = RenderVolume_DS::VOLUMETYPE_FRUSTUM;

    // Determine PS permutation
    RenderVolume_PS::Desc ps_desc;
    ps_desc.flags.SAMPLEMODE = isInternalMSAA() ? RenderVolume_PS::SAMPLEMODE_MSAA : RenderVolume_PS::SAMPLEMODE_SINGLE;
    ps_desc.flags.LIGHTMODE = RenderVolume_PS::LIGHTMODE_SPOTLIGHT;
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_GEOMETRY;
    ps_desc.flags.ATTENUATIONMODE = (RenderVolume_PS::eATTENUATIONMODE) pLightDesc->Spotlight.eAttenuationMode;
    ps_desc.flags.FALLOFFMODE = (RenderVolume_PS::eFALLOFFMODE) pLightDesc->Spotlight.eFalloffMode;

    dxCtx->HSSetShader(shaders.RenderVolume_HS[hs_desc], nullptr, 0);
    dxCtx->DSSetShader(shaders.RenderVolume_DS[ds_desc], nullptr, 0);
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);

    SRVs[1] = pShadowMap_SRV;
    SRVs[4] = pPhaseLUT_->getSRV();
    SRVs[5] = pLightLUT_P_[1]->getSRV();
    SRVs[6] = pLightLUT_S1_[1]->getSRV();
    SRVs[7] = pLightLUT_S2_[1]->getSRV();
    dxCtx->VSSetShaderResources(0, 8, SRVs);
    dxCtx->HSSetShaderResources(0, 8, SRVs);
    dxCtx->DSSetShaderResources(0, 8, SRVs);
    dxCtx->PSSetShaderResources(0, 8, SRVs);

    dxCtx->OMSetDepthStencilState(states.ds.render_volume, STENCIL_REF);
    DrawFrustumGrid(dxCtx, mesh_resolution);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);

    //--------------------------------------------------------------------------
    // Render the bounds of the spotlight volume
    dxCtx->RSSetState(states.rs.cull_front);
    DrawFrustumCap(dxCtx, mesh_resolution);

    //--------------------------------------------------------------------------
    // Finish the rendering by filling in stenciled gaps
    dxCtx->OMSetDepthStencilState(states.ds.finish_volume, STENCIL_REF);
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getReadOnlyDSV());
    SRVs[2] = pDepth_->getSRV();
    dxCtx->PSSetShaderResources(2, 1, &SRVs[2]);
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_FINAL;
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);
    DrawFullscreen(dxCtx);

	return Status::OK;
}

Status ContextImp_D3D11::RenderVolume_DoVolume_Omni(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    ID3D11ShaderResourceView * pShadowMap_SRV = shadowMap.d3d11;

    NV_PERFEVENT(dxCtx, "Omni");

    uint32_t mesh_resolution = getCoarseResolution(pVolumeDesc);

    ID3D11ShaderResourceView * SRVs[] = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    ID3D11UnorderedAccessView * UAVs[] = {
        nullptr
    };

    //--------------------------------------------------------------------------
    // Create look-up table
    {
        NV_PERFEVENT(dxCtx, "Generate Light LUT");
        ComputeLightLUT_CS::Desc cs_desc;
        cs_desc.flags.LIGHTMODE = ComputeLightLUT_CS::LIGHTMODE_OMNI;
        cs_desc.flags.ATTENUATIONMODE = (ComputeLightLUT_CS::eATTENUATIONMODE) pLightDesc->Omni.eAttenuationMode;

        UAVs[0] = pLightLUT_P_[0]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
        SRVs[4] = pPhaseLUT_->getSRV();
        dxCtx->CSSetShaderResources(0, 6, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_CALCULATE;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(LIGHT_LUT_DEPTH_RESOLUTION / 32, LIGHT_LUT_WDOTV_RESOLUTION / 8, 1);

        UAVs[0] = pLightLUT_P_[1]->getUAV();
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
        SRVs[5] = pLightLUT_P_[0]->getSRV();
        dxCtx->CSSetShaderResources(0, 6, SRVs);
        cs_desc.flags.COMPUTEPASS = ComputeLightLUT_CS::COMPUTEPASS_SUM;
        dxCtx->CSSetShader(shaders.ComputeLightLUT_CS[cs_desc], nullptr, 0);
        dxCtx->Dispatch(1, LIGHT_LUT_WDOTV_RESOLUTION / 4, 1);

        dxCtx->CSSetShader(nullptr, nullptr, 0);
        UAVs[0] = nullptr;
        dxCtx->CSSetUnorderedAccessViews(0, 1, UAVs, 0);
    }

    //--------------------------------------------------------------------------
    // Draw tessellated grid
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getInternalViewportWidth());
    viewport.Height = static_cast<float>(getInternalViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);
    dxCtx->RSSetState(states.rs.cull_none);
    dxCtx->OMSetBlendState(states.bs.additive, nullptr, 0xFFFFFFFF);

    ID3D11RenderTargetView * RTVs[] = { pAccumulation_->getRTV() };
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getDSV());

    // Determine DS/HS permutation
    RenderVolume_HS::Desc hs_desc;
    hs_desc.flags.SHADOWMAPTYPE = RenderVolume_HS::SHADOWMAPTYPE_ARRAY;
    hs_desc.flags.CASCADECOUNT = RenderVolume_HS::CASCADECOUNT_1;
    hs_desc.flags.VOLUMETYPE = RenderVolume_HS::VOLUMETYPE_PARABOLOID;
    hs_desc.flags.MAXTESSFACTOR = (RenderVolume_HS::eMAXTESSFACTOR) pVolumeDesc->eTessQuality;

    RenderVolume_DS::Desc ds_desc;
    ds_desc.flags.SHADOWMAPTYPE = RenderVolume_DS::SHADOWMAPTYPE_ARRAY;
    ds_desc.flags.CASCADECOUNT = RenderVolume_DS::CASCADECOUNT_1;
    ds_desc.flags.VOLUMETYPE = RenderVolume_DS::VOLUMETYPE_PARABOLOID;

    // Determine PS permutation
    RenderVolume_PS::Desc ps_desc;
    ps_desc.flags.SAMPLEMODE = isInternalMSAA() ? RenderVolume_PS::SAMPLEMODE_MSAA : RenderVolume_PS::SAMPLEMODE_SINGLE;
    ps_desc.flags.LIGHTMODE = RenderVolume_PS::LIGHTMODE_OMNI;
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_GEOMETRY;
    ps_desc.flags.ATTENUATIONMODE = (RenderVolume_PS::eATTENUATIONMODE) pLightDesc->Omni.eAttenuationMode;

    dxCtx->HSSetShader(shaders.RenderVolume_HS[hs_desc], nullptr, 0);
    dxCtx->DSSetShader(shaders.RenderVolume_DS[ds_desc], nullptr, 0);
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);

    SRVs[1] = pShadowMap_SRV;
    SRVs[5] = pLightLUT_P_[1]->getSRV();
    dxCtx->VSSetShaderResources(0, 6, SRVs);
    dxCtx->HSSetShaderResources(0, 6, SRVs);
    dxCtx->DSSetShaderResources(0, 6, SRVs);
    dxCtx->PSSetShaderResources(0, 6, SRVs);

    dxCtx->OMSetDepthStencilState(states.ds.render_volume, STENCIL_REF);
    DrawOmniVolume(dxCtx, mesh_resolution);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);

    //--------------------------------------------------------------------------
    // Finish the rendering by filling in stenciled gaps
    dxCtx->OMSetDepthStencilState(states.ds.finish_volume, STENCIL_REF);
    dxCtx->OMSetRenderTargets(1, RTVs, pDepth_->getReadOnlyDSV());
    SRVs[2] = pDepth_->getSRV();
    dxCtx->PSSetShaderResources(2, 1, &SRVs[2]);
    ps_desc.flags.PASSMODE = RenderVolume_PS::PASSMODE_FINAL;
    dxCtx->PSSetShader(shaders.RenderVolume_PS[ps_desc], nullptr, 0);
    DrawFullscreen(dxCtx);

    return Status::OK;
}

Status ContextImp_D3D11::RenderVolume_End(PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
	ID3D11RenderTargetView * NULL_RTV = nullptr;
	dxCtx->OMSetRenderTargets(1, &NULL_RTV, nullptr);
    NV_PERFEVENT_END(dxCtx);
	return Status::OK;
}

/*==============================================================================
   EndAccumulation
==============================================================================*/
Status ContextImp_D3D11::EndAccumulation_Imp(PlatformRenderCtx renderCtx)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    dxCtx;
	return Status::OK;
}

/*==============================================================================
   ApplyLighting
==============================================================================*/
Status ContextImp_D3D11::ApplyLighting_Start(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;

    NV_PERFEVENT_BEGIN(dxCtx, "NvVl::ApplyLighting");

    // Setup the constant buffer
    SetupCB_PerApply(pPostprocessDesc, pPerApplyCB->Map(dxCtx));
    pPerApplyCB->Unmap(dxCtx);

    ID3D11Buffer* pCBs[] = {
        pPerContextCB->getCB(),
        pPerFrameCB->getCB(),
        nullptr, // pPerVolumeCB - Invalid
        pPerApplyCB->getCB(),
    };
    dxCtx->VSSetConstantBuffers(0, 4, pCBs);
    dxCtx->PSSetConstantBuffers(0, 4, pCBs);

    ID3D11SamplerState * pSamplers[] = {
        states.ss.point,
        states.ss.linear
    };
    dxCtx->PSSetSamplers(0, 2, pSamplers);
    dxCtx->OMSetDepthStencilState(states.ds.no_depth, 0xFF);
    dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getInternalViewportWidth());
    viewport.Height = static_cast<float>(getInternalViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);

    pAccumulatedOutput_ = pAccumulation_;

	return Status::OK;
}

Status ContextImp_D3D11::ApplyLighting_Resolve(PlatformRenderCtx renderCtx, PostprocessDesc const* pPostprocessDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;

    NV_PERFEVENT(dxCtx, "Resolve");

    Resolve_PS::Desc ps_desc;
    ps_desc.flags.SAMPLEMODE = isInternalMSAA() ? Resolve_PS::SAMPLEMODE_MSAA : Resolve_PS::SAMPLEMODE_SINGLE;
	dxCtx->PSSetShader(shaders.Resolve_PS[ps_desc], nullptr, 0);
    ID3D11ShaderResourceView * SRVs[] = {
        pAccumulation_->getSRV(),
        pDepth_->getSRV()
    };
	dxCtx->PSSetShaderResources(0, 2, SRVs);
	ID3D11RenderTargetView * RTVs[] = {
		pResolvedAccumulation_->getRTV(),
        pResolvedDepth_->getRTV()
	};
	dxCtx->OMSetRenderTargets(2, RTVs, nullptr);
	DrawFullscreen(dxCtx);
    pAccumulatedOutput_ = pResolvedAccumulation_;
    ID3D11RenderTargetView * NULL_RTVs[] = { nullptr, nullptr };
	dxCtx->OMSetRenderTargets(2, NULL_RTVs, nullptr);
	return Status::OK;
}

Status ContextImp_D3D11::ApplyLighting_TemporalFilter(PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;

    NV_PERFEVENT(dxCtx, "TemporalFilter");

	dxCtx->PSSetShader(shaders.TemporalFilter_PS[TemporalFilter_PS::SINGLE], nullptr, 0);
    ID3D11ShaderResourceView * SRVs[] = {
        pResolvedAccumulation_->getSRV(),
        pFilteredAccumulation_[lastFrameIndex_]->getSRV(),
        pResolvedDepth_->getSRV(),
        nullptr,
        pFilteredDepth_[lastFrameIndex_]->getSRV()
    };
	dxCtx->PSSetShaderResources(0, 4, SRVs);
	ID3D11RenderTargetView * RTVs[] = {
		pFilteredAccumulation_[nextFrameIndex_]->getRTV(),
		pFilteredDepth_[nextFrameIndex_]->getRTV()
	};
	dxCtx->OMSetRenderTargets(2, RTVs, nullptr);
	DrawFullscreen(dxCtx);
    pAccumulatedOutput_ = pFilteredAccumulation_[nextFrameIndex_];
    ID3D11RenderTargetView * NULL_RTVs[] = { nullptr, nullptr };
	dxCtx->OMSetRenderTargets(2, NULL_RTVs, nullptr);
	return Status::OK;
}

Status ContextImp_D3D11::ApplyLighting_Composite(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    ID3D11RenderTargetView * pScene_RTV = sceneTarget.d3d11;
    ID3D11ShaderResourceView * pSceneDepth_SRV = sceneDepth.d3d11;
    
    NV_PERFEVENT(dxCtx, "Composite");

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(getOutputViewportWidth());
    viewport.Height = static_cast<float>(getOutputViewportHeight());
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    dxCtx->RSSetViewports(1, &viewport);

    if (debugFlags_ & (uint32_t)DebugFlags::NO_BLENDING)
    {
        dxCtx->OMSetBlendState(states.bs.debug_blend, nullptr, 0xFFFFFFFF);
    }
    else
    {
        FLOAT blend_factor[] = { pPostprocessDesc->fBlendfactor, pPostprocessDesc->fBlendfactor, pPostprocessDesc->fBlendfactor, pPostprocessDesc->fBlendfactor };
        dxCtx->OMSetBlendState(states.bs.additive_modulate, blend_factor, 0xFFFFFFFF);
    }
    dxCtx->OMSetRenderTargets(1, &pScene_RTV, nullptr);

    Apply_PS::Desc ps_desc;
    ps_desc.flags.SAMPLEMODE = isOutputMSAA() ? Apply_PS::SAMPLEMODE_MSAA : Apply_PS::SAMPLEMODE_SINGLE;
    switch (pPostprocessDesc->eUpsampleQuality)
    {
    default:
    case UpsampleQuality::POINT:
        ps_desc.flags.UPSAMPLEMODE = Apply_PS::UPSAMPLEMODE_POINT;
        break;

    case UpsampleQuality::BILINEAR:
        ps_desc.flags.UPSAMPLEMODE = Apply_PS::UPSAMPLEMODE_BILINEAR;
        break;

    case UpsampleQuality::BILATERAL:
        ps_desc.flags.UPSAMPLEMODE = Apply_PS::UPSAMPLEMODE_BILATERAL;
        break;
    }
    if (pPostprocessDesc->bDoFog == false)
        ps_desc.flags.FOGMODE = Apply_PS::FOGMODE_NONE;
    else if (pPostprocessDesc->bIgnoreSkyFog == true)
        ps_desc.flags.FOGMODE = Apply_PS::FOGMODE_NOSKY;
    else
        ps_desc.flags.FOGMODE = Apply_PS::FOGMODE_FULL;
    dxCtx->PSSetShader(shaders.Apply_PS[ps_desc], nullptr, 0);
    ID3D11ShaderResourceView * SRVs[] = {
	    pAccumulatedOutput_->getSRV(),
	    pSceneDepth_SRV,
	    nullptr,
	    nullptr,
        pPhaseLUT_->getSRV()
    };
    if (pFilteredDepth_[nextFrameIndex_])
        SRVs[2] = pFilteredDepth_[nextFrameIndex_]->getSRV();
    dxCtx->PSSetShaderResources(0, 5, SRVs);
    DrawFullscreen(dxCtx);

	ID3D11RenderTargetView * NULL_RTV = nullptr;
	dxCtx->OMSetRenderTargets(1, &NULL_RTV, nullptr);
	return Status::OK;
}

Status ContextImp_D3D11::ApplyLighting_End(PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    ID3D11DeviceContext * dxCtx = renderCtx;
    dxCtx;
    NV_PERFEVENT_END(dxCtx);
	return Status::OK;
}

/*==============================================================================
   Utility functions
==============================================================================*/

Status ContextImp_D3D11::DrawFullscreen(ID3D11DeviceContext * dxCtx)
{
    NV_PERFEVENT(dxCtx, "DrawFullscreen");
    dxCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dxCtx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
    dxCtx->IASetInputLayout(nullptr);
    dxCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    dxCtx->RSSetState(states.rs.cull_none);
    dxCtx->VSSetShader(shaders.Quad_VS[Quad_VS::SINGLE], nullptr, 0);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);
    dxCtx->Draw(3, 0);
    return Status::OK;
}

Status ContextImp_D3D11::DrawFrustumGrid(ID3D11DeviceContext * dxCtx, uint32_t resolution)
{
    NV_PERFEVENT(dxCtx, "DrawFrustumGrid");
    dxCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    dxCtx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
    dxCtx->IASetInputLayout(nullptr);
    dxCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    RenderVolume_VS::Desc vs_desc;
    vs_desc.flags.MESHMODE = RenderVolume_VS::MESHMODE_FRUSTUM_GRID;
    dxCtx->VSSetShader(shaders.RenderVolume_VS[vs_desc], nullptr, 0);

    if (debugFlags_ & (uint32_t)DebugFlags::WIREFRAME)
    {
        dxCtx->RSSetState(states.rs.wireframe);
        dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);
        dxCtx->PSSetShader(shaders.Debug_PS[Debug_PS::SINGLE], nullptr, 0);
    }

    uint32_t vtx_count = 4 * resolution * resolution;
    dxCtx->Draw(vtx_count, 0);
    return Status::OK;
}

Status ContextImp_D3D11::DrawFrustumBase(ID3D11DeviceContext * dxCtx, uint32_t resolution)
{
    NV_PERFEVENT(dxCtx, "DrawFrustumBase");
    dxCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dxCtx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
    dxCtx->IASetInputLayout(nullptr);
    dxCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    RenderVolume_VS::Desc vs_desc;
    vs_desc.flags.MESHMODE = RenderVolume_VS::MESHMODE_FRUSTUM_BASE;
    dxCtx->VSSetShader(shaders.RenderVolume_VS[vs_desc], nullptr, 0);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);

    if (debugFlags_ & (uint32_t)DebugFlags::WIREFRAME)
    {
        dxCtx->RSSetState(states.rs.wireframe);
        dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);
        dxCtx->PSSetShader(shaders.Debug_PS[Debug_PS::SINGLE], nullptr, 0);
    }

    dxCtx->Draw(6, 0);
    return Status::OK;
}

Status ContextImp_D3D11::DrawFrustumCap(ID3D11DeviceContext * dxCtx, uint32_t resolution)
{
    NV_PERFEVENT(dxCtx, "DrawFrustumCap");
    dxCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dxCtx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
    dxCtx->IASetInputLayout(nullptr);
    dxCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    RenderVolume_VS::Desc vs_desc;
    vs_desc.flags.MESHMODE = RenderVolume_VS::MESHMODE_FRUSTUM_CAP;
    dxCtx->VSSetShader(shaders.RenderVolume_VS[vs_desc], nullptr, 0);
    dxCtx->HSSetShader(nullptr, nullptr, 0);
    dxCtx->DSSetShader(nullptr, nullptr, 0);

    if (debugFlags_ & (uint32_t)DebugFlags::WIREFRAME)
    {
        dxCtx->RSSetState(states.rs.wireframe);
        dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);
        dxCtx->PSSetShader(shaders.Debug_PS[Debug_PS::SINGLE], nullptr, 0);
    }

    uint32_t vtx_count = 4*3*(resolution+1) + 6;
    dxCtx->Draw(vtx_count, 0);
    return Status::OK;
}

Status ContextImp_D3D11::DrawOmniVolume(ID3D11DeviceContext * dxCtx, uint32_t resolution)
{
    NV_PERFEVENT(dxCtx, "DrawOmniVolume");
    dxCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    dxCtx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
    dxCtx->IASetInputLayout(nullptr);
    dxCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    RenderVolume_VS::Desc vs_desc;
    vs_desc.flags.MESHMODE = RenderVolume_VS::MESHMODE_OMNI_VOLUME;
    dxCtx->VSSetShader(shaders.RenderVolume_VS[vs_desc], nullptr, 0);

    if (debugFlags_ & (uint32_t)DebugFlags::WIREFRAME)
    {
        dxCtx->RSSetState(states.rs.wireframe);
        dxCtx->OMSetBlendState(states.bs.no_blending, nullptr, 0xFFFFFFFF);
        dxCtx->PSSetShader(shaders.Debug_PS[Debug_PS::SINGLE], nullptr, 0);
    }

    uint32_t vtx_count = 6 * 4 * resolution * resolution;
    dxCtx->Draw(vtx_count, 0);
    return Status::OK;
}
////////////////////////////////////////////////////////////////////////////////
}; /*namespace Nv*/ }; /*namespace VolumetricLighting*/
////////////////////////////////////////////////////////////////////////////////
