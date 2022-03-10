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

#ifndef D3D11_UTIL_H
#define D3D11_UTIL_H
////////////////////////////////////////////////////////////////////////////////
#include "common.h"

#include <d3d11.h>
#include <d3d11_1.h>

#pragma warning( disable: 4127 )

////////////////////////////////////////////////////////////////////////////////
/*==============================================================================
    Overloaded functions to make shader compilation simpler
==============================================================================*/
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11VertexShader ** out_shader);
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11HullShader ** out_shader);
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11DomainShader ** out_shader);
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11GeometryShader ** out_shader);
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11PixelShader ** out_shader);
HRESULT CompileShader(ID3D11Device * device, const void * data, UINT length, ID3D11ComputeShader ** out_shader);


template <class T>
NV_FORCE_INLINE HRESULT LoadShaders(ID3D11Device * device, const void ** shader_codes, const uint32_t * shader_lengths, UINT count, T ** &out_shaders)
{
    HRESULT hr = S_OK;
    out_shaders = new T*[count];
    for (UINT i = 0; i < count; ++i)
    {
        const void * code = shader_codes[i];
        uint32_t length = shader_lengths[i];
        if (code == nullptr)
        {
            out_shaders[i] = nullptr;
            continue;
        }
        hr = CompileShader(device, code, length, &out_shaders[i]);
        ASSERT_LOG(hr == S_OK, "Failure loading shader");
        if (FAILED(hr))
            break;
    }
    return hr;
}

/*==============================================================================
    Perf Events
==============================================================================*/

class ScopedPerfEvent
{
public:
    ScopedPerfEvent(ID3D11DeviceContext * ctx, const wchar_t * msg)
    {
        ctx->QueryInterface(IID_ID3DUserDefinedAnnotation, reinterpret_cast<void **>(&pPerf));
        if (pPerf)
        {
            pPerf->BeginEvent(msg);
        }
    }

    ~ScopedPerfEvent()
    {
        if (pPerf)
        {
            pPerf->EndEvent();
            SAFE_RELEASE(pPerf);
        }
    };

private:
    ScopedPerfEvent() : pPerf(nullptr) {};
    ID3DUserDefinedAnnotation * pPerf;
};

#if (NV_PROFILE)
#   define NV_PERFEVENT(c, t) ScopedPerfEvent perfevent_##__COUNTER__##(c, L##t)
#   define NV_PERFEVENT_BEGIN(c, t) { \
        ID3DUserDefinedAnnotation * pPerf_##__COUNTER__ ; \
        c->QueryInterface(IID_ID3DUserDefinedAnnotation, reinterpret_cast<void **>(&pPerf_##__COUNTER__##)); \
        if (pPerf_##__COUNTER__##) { pPerf_##__COUNTER__##->BeginEvent(L##t); pPerf_##__COUNTER__##->Release(); } }
#   define NV_PERFEVENT_END(c) { \
        ID3DUserDefinedAnnotation * pPerf_##__COUNTER__ ; \
        c->QueryInterface(IID_ID3DUserDefinedAnnotation, reinterpret_cast<void **>(&pPerf_##__COUNTER__##)); \
        if (pPerf_##__COUNTER__##) { pPerf_##__COUNTER__##->EndEvent(); pPerf_##__COUNTER__##->Release(); } }
#else
#   define NV_PERFEVENT(c, t)
#   define NV_PERFEVENT_BEGIN(c, t)
#   define NV_PERFEVENT_END(c)
#endif

/*==============================================================================
    Constant buffer management
==============================================================================*/

template <class T>
class ConstantBuffer
{
public:
    static ConstantBuffer<T> * Create(ID3D11Device * device)
    {
        HRESULT hr;
        ID3D11Buffer * cb = nullptr;

        static_assert((sizeof(T) % 16) == 0, "Constant buffer size must be 16-byte aligned");

        CD3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(T);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 1;
        hr = device->CreateBuffer(&cbDesc, nullptr, &cb);
        if (cb == nullptr)
            return nullptr;
        return new ConstantBuffer<T>(cb);
    }

    ConstantBuffer(ID3D11Buffer * b)
    {
        buffer_ = b;
    }

    ~ConstantBuffer()
    {
        SAFE_RELEASE(buffer_);
    }

    T * Map(ID3D11DeviceContext * ctx)
    {
        HRESULT hr;
        D3D11_MAPPED_SUBRESOURCE data;
        hr = ctx->Map(buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &data);
        return static_cast<T*>(data.pData);
    }

    void Unmap(ID3D11DeviceContext * ctx)
    {
        ctx->Unmap(buffer_, 0);
    }

    ID3D11Buffer * getCB() { return buffer_; }

private:
    ConstantBuffer() {};

    ID3D11Buffer *buffer_;
};

/*==============================================================================
    Texture resource management helpers
==============================================================================*/

//------------------------------------------------------------------------------
class ShaderResource
{
public:
    ShaderResource(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11UnorderedAccessView * uav);
    ~ShaderResource();

    ID3D11Resource * getResource() { return resource_; }
    ID3D11ShaderResourceView * getSRV() { return srv_; }
    ID3D11UnorderedAccessView * getUAV() { return uav_; }

protected:
    ShaderResource() {};

    ID3D11Resource * resource_;
    ID3D11ShaderResourceView * srv_;
    ID3D11UnorderedAccessView * uav_;
};

//------------------------------------------------------------------------------
class RenderTarget : public ShaderResource
{
public:
    static RenderTarget * Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, const char * debug_name=nullptr);
    RenderTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView * rtv, ID3D11UnorderedAccessView * uav);
    ~RenderTarget();

    ID3D11RenderTargetView * getRTV() { return rtv_; }

protected:
    RenderTarget() {};
    ID3D11RenderTargetView * rtv_;
};

//------------------------------------------------------------------------------
class DepthTarget : public ShaderResource
{
public:
    static DepthTarget * Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, uint32_t slices=1, char * debug_name=nullptr);
    DepthTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11DepthStencilView * dsv, ID3D11DepthStencilView * readonly_dsv, ID3D11UnorderedAccessView * uav);
    ~DepthTarget();

    ID3D11DepthStencilView * getDSV() { return dsv_; }
    ID3D11DepthStencilView * getReadOnlyDSV() { return readonly_dsv_; }

protected:
    DepthTarget() {};
    ID3D11DepthStencilView * dsv_;
    ID3D11DepthStencilView * readonly_dsv_;
};

////////////////////////////////////////////////////////////////////////////////
#endif // D3D11_UTIL_H