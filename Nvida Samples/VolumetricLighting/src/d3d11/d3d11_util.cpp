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
#include "d3d11_util.h"
#include <d3d11.h>

/*==============================================================================
    Overloaded functions to make shader compilation simpler
==============================================================================*/
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11VertexShader ** out_shader)
    { return device->CreateVertexShader(data, length, nullptr, out_shader); }
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11HullShader ** out_shader)
    { return device->CreateHullShader(data, length, nullptr, out_shader); }
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11DomainShader ** out_shader)
    { return device->CreateDomainShader(data, length, nullptr, out_shader); }
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11GeometryShader ** out_shader)
    { return device->CreateGeometryShader(data, length, nullptr, out_shader); }
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11PixelShader ** out_shader)
    { return device->CreatePixelShader(data, length, nullptr, out_shader); }
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11ComputeShader ** out_shader)
    { return device->CreateComputeShader(data, length, nullptr, out_shader); }
/*==============================================================================
    Texture resource management helpers
==============================================================================*/

//------------------------------------------------------------------------------
ShaderResource::ShaderResource(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11UnorderedAccessView * uav)
{
    resource_ = resource;
    srv_ = srv;
    uav_ = uav;
}

ShaderResource::~ShaderResource()
{
    SAFE_RELEASE(resource_);
    SAFE_RELEASE(srv_);
    SAFE_RELEASE(uav_);
}

//------------------------------------------------------------------------------
RenderTarget * RenderTarget::Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, const char * debug_name)
{
    ID3D11Texture2D * tex = nullptr;
    ID3D11ShaderResourceView * srv = nullptr;
    ID3D11RenderTargetView * rtv = nullptr;
    ID3D11UnorderedAccessView * uav = nullptr;

    CD3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    if (samples == 1)
        texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    texDesc.CPUAccessFlags = 0;
    texDesc.Format = format;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.MiscFlags = 0;
    if (samples > 1)
    {
        texDesc.SampleDesc.Count = samples;
        texDesc.SampleDesc.Quality = 0;//static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN);
    }
    else
    {
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
    }
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    device->CreateTexture2D(&texDesc, nullptr, &tex);
    if (tex == nullptr)
    {
        return nullptr;
    }

    CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = texDesc.Format;
    if (samples > 1)
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    }
    else
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
    }
    device->CreateShaderResourceView(tex, &srvDesc, &srv);
    if (srv == nullptr)
    {
        tex->Release();
        return nullptr;
    }

    CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = texDesc.Format;
    if (samples > 1)
    {
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
    }
    else
    {
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
    }
    device->CreateRenderTargetView(tex, &rtvDesc, &rtv);
    if (rtv == nullptr)
    {
        tex->Release();
        srv->Release();
        return nullptr;
    }

    if (samples == 1)
    {
        CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = texDesc.Format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        device->CreateUnorderedAccessView(tex, &uavDesc, &uav);
        if (uav == nullptr)
        {
            tex->Release();
            srv->Release();
            rtv->Release();
            return nullptr;
        }
    }
#if (NV_DEBUG || NV_PROFILE)
    if (debug_name)
    {
        tex->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(debug_name), debug_name);
    }
#else
    debug_name;
#endif
    return new RenderTarget(tex, srv, rtv, uav);
}

RenderTarget::RenderTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView * rtv, ID3D11UnorderedAccessView * uav)
    : ShaderResource(resource, srv, uav)
{
    rtv_ = rtv;
}

RenderTarget::~RenderTarget()
{
    SAFE_RELEASE(rtv_);
}

//------------------------------------------------------------------------------
DepthTarget * DepthTarget::Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, uint32_t slices, char * debug_name)
{
    ID3D11Texture2D * tex = nullptr;
    ID3D11ShaderResourceView * srv = nullptr;
    ID3D11DepthStencilView * dsv = nullptr;
    ID3D11DepthStencilView * dsv_ro = nullptr;

    DXGI_FORMAT tex_format;
    DXGI_FORMAT srv_format;
    DXGI_FORMAT dsv_format;
    switch (format)
    {
    case DXGI_FORMAT_D32_FLOAT:
        tex_format = DXGI_FORMAT_R32_TYPELESS;
        srv_format = DXGI_FORMAT_R32_FLOAT;
        dsv_format = DXGI_FORMAT_D32_FLOAT;
        break;

    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        tex_format = DXGI_FORMAT_R24G8_TYPELESS;
        srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        break;

    case DXGI_FORMAT_D16_UNORM:
        tex_format = DXGI_FORMAT_R16_TYPELESS;
        srv_format = DXGI_FORMAT_R16_UNORM;
        dsv_format = DXGI_FORMAT_D16_UNORM;
        break;

    default:
        return nullptr;
    }

    CD3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize = slices;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    texDesc.CPUAccessFlags = 0;
    texDesc.Format = tex_format;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.MiscFlags = 0;
    if (samples > 1)
    {
        texDesc.SampleDesc.Count = samples;
        texDesc.SampleDesc.Quality = 0;//static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN);
    }
    else
    {
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
    }
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    device->CreateTexture2D(&texDesc, nullptr, &tex);
    if (tex == nullptr)
    {
        return nullptr;
    }

    CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = srv_format;
    if (slices == 1)
    {
        if (samples > 1)
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        }
        else
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
        }
    }
    else
    {
        if (samples > 1)
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
            srvDesc.Texture2DMSArray.FirstArraySlice = 0;
            srvDesc.Texture2DMSArray.ArraySize = slices;
        }
        else
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = slices;
        }
    }
    device->CreateShaderResourceView(tex, &srvDesc, &srv);

    if (srv == nullptr)
    {
        tex->Release();
        return nullptr;
    }

    CD3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = 0;
    dsvDesc.Format = dsv_format;
    if (slices == 1)
    {
        if (samples > 1)
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        }
        else
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
    }
    else
    {
        if (samples > 1)
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
            dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
            dsvDesc.Texture2DMSArray.ArraySize = slices;
        }
        else
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = 0;
            dsvDesc.Texture2DArray.ArraySize = slices;
        }
    }

    device->CreateDepthStencilView(tex, &dsvDesc, &dsv);
    dsvDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
    if (dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT)
        dsvDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
    device->CreateDepthStencilView(tex, &dsvDesc, &dsv_ro);
    if (dsv == nullptr || dsv_ro == nullptr)
    {
        tex->Release();
        srv->Release();
        return nullptr;
    }

#if (NV_DEBUG || NV_PROFILE)
    if (debug_name)
    {
        tex->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(debug_name), debug_name);
    }
#else
    debug_name;
#endif

    return new DepthTarget(tex, srv, dsv, dsv_ro, nullptr);
}

DepthTarget::DepthTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11DepthStencilView * dsv, ID3D11DepthStencilView * readonly_dsv, ID3D11UnorderedAccessView * uav)
    : ShaderResource(resource, srv, uav)
{
    dsv_ = dsv;
    readonly_dsv_ = readonly_dsv;
}

DepthTarget::~DepthTarget()
{
    SAFE_RELEASE(dsv_);
    SAFE_RELEASE(readonly_dsv_);
}

