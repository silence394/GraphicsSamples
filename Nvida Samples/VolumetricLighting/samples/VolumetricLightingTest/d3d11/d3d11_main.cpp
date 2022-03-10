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

#include "compiled_shaders_d3d11.h"
#include "DeviceManager.h"
#include "d3d11_util.h"
#include "scene.h"

#include <Nv/VolumetricLighting/NvVolumetricLighting.h>
#include <d3d11.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Control rendering of the scene
#define LOAD_SHADERS(x) LoadShaders(device, ##x##::permutation_code, ##x##::permutation_length, shaders::##x##::PERMUTATION_ENTRY_COUNT, shaders.##x)

class SceneController : public IVisualController
{
private:
    Scene * pScene_;
    DeviceManager * pManager_;

    uint32_t framebufferWidth_;
    uint32_t framebufferHeight_;

    // Resources used by the scene

    ConstantBuffer <Scene::ViewCB> * pViewCB_;
    ConstantBuffer <Scene::ObjectCB> * pObjectCB_;
    ConstantBuffer <Scene::LightCB> * pLightCB_;

    RenderTarget * pSceneRT_;
    DepthTarget * pSceneDepth_;
    DepthTarget * pShadowMap_;
    DepthTarget * pParaboloidShadowMap_;

    ID3D11SamplerState * ss_shadowmap_;
    ID3D11RasterizerState * rs_render_;
    ID3D11BlendState * bs_no_color_;
    ID3D11DepthStencilState * ds_shadowmap_;

    struct
    {
        ID3D11PixelShader ** post_PS;
        ID3D11VertexShader ** quad_VS;
        ID3D11VertexShader ** scene_VS;
        ID3D11GeometryShader ** scene_GS;
        ID3D11PixelShader ** scene_PS;
    } shaders;

    // GWVL Platform-specific info
    Nv::Vl::PlatformDesc platformDesc_;

public:

    SceneController(Scene * pScene, DeviceManager * pManager)
    {
        pScene_ = pScene;
        pManager_ = pManager;
    }

    virtual HRESULT DeviceCreated(ID3D11Device* device)
    {
        Nv::Vl::OpenLibrary();

        // Shaders
        LOAD_SHADERS(post_PS);
        LOAD_SHADERS(quad_VS);
        LOAD_SHADERS(scene_VS);
        LOAD_SHADERS(scene_GS);
        LOAD_SHADERS(scene_PS);

        // Constant Buffers
        pViewCB_ = ConstantBuffer<Scene::ViewCB>::Create(device);
        pObjectCB_ = ConstantBuffer<Scene::ObjectCB>::Create(device);
        pLightCB_ = ConstantBuffer<Scene::LightCB>::Create(device);

        // Textures
        pSceneRT_ = nullptr;
        pSceneDepth_ = nullptr;
        pShadowMap_ = DepthTarget::Create(device, Scene::SHADOWMAP_RESOLUTION, Scene::SHADOWMAP_RESOLUTION, 1, DXGI_FORMAT_D16_UNORM, 1, "Simple Shadow Map");
        pParaboloidShadowMap_ = DepthTarget::Create(device, Scene::SHADOWMAP_RESOLUTION, Scene::SHADOWMAP_RESOLUTION, 1, DXGI_FORMAT_D16_UNORM, 2, "Dual-Paraboloid Shadow Map");

        // Shadowmap Sampler
        {
            CD3D11_SAMPLER_DESC desc((CD3D11_DEFAULT()));
            desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
            desc.ComparisonFunc = D3D11_COMPARISON_LESS;
            device->CreateSamplerState(&desc, &ss_shadowmap_);
        }
        // No color output blend-state
        {
            CD3D11_BLEND_DESC desc((CD3D11_DEFAULT()));
            desc.RenderTarget[0].RenderTargetWriteMask = 0x00000000;
            device->CreateBlendState(&desc, &bs_no_color_);
        }
        {
            CD3D11_RASTERIZER_DESC rsDesc((CD3D11_DEFAULT()));
            rsDesc.CullMode = D3D11_CULL_NONE;
            device->CreateRasterizerState(&rsDesc, &rs_render_);
        }
        {
            CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
            device->CreateDepthStencilState(&dsDesc, &ds_shadowmap_);
        }
        // Initialize GWVL settings info
        {
            platformDesc_.platform = Nv::Vl::PlatformName::D3D11;
            platformDesc_.d3d11.pDevice = device;
        }

        return S_OK;
    }

    virtual void DeviceDestroyed()
    {
        // Release resources
        SAFE_RELEASE(ss_shadowmap_);
        SAFE_RELEASE(bs_no_color_);
        SAFE_RELEASE(rs_render_);
        SAFE_RELEASE(ds_shadowmap_);
        SAFE_DELETE(pViewCB_);
        SAFE_DELETE(pObjectCB_);
        SAFE_DELETE(pLightCB_);
        SAFE_DELETE(pSceneRT_);
        SAFE_DELETE(pSceneDepth_);
        SAFE_DELETE(pShadowMap_);
        SAFE_DELETE(pParaboloidShadowMap_);
        SAFE_DELETE_ARRAY(shaders.post_PS);
        SAFE_DELETE_ARRAY(shaders.quad_VS);
        SAFE_DELETE_ARRAY(shaders.scene_VS);
        SAFE_DELETE_ARRAY(shaders.scene_GS);
        SAFE_DELETE_ARRAY(shaders.scene_PS);

        pScene_->Release();
        Nv::Vl::CloseLibrary();
    }

    virtual void BackBufferResized(ID3D11Device* device, const DXGI_SURFACE_DESC* surface_desc)
    {
        framebufferWidth_ = surface_desc->Width;
        framebufferHeight_ = surface_desc->Height;

        // Create back-buffer sized resources
        SAFE_DELETE(pSceneRT_);
        SAFE_DELETE(pSceneDepth_);
        pSceneRT_ = RenderTarget::Create(device, framebufferWidth_, framebufferHeight_, surface_desc->SampleDesc.Count, DXGI_FORMAT_R16G16B16A16_FLOAT, "Scene Render Target");
        pSceneDepth_ = DepthTarget::Create(device, framebufferWidth_, framebufferHeight_, surface_desc->SampleDesc.Count, DXGI_FORMAT_D24_UNORM_S8_UINT, 1, "Scene Depth");
        
        // Update the context desc and (re-)create the context
        pScene_->updateFramebuffer(framebufferWidth_, framebufferHeight_, surface_desc->SampleDesc.Count);
    }

    virtual void Render(ID3D11Device*, ID3D11DeviceContext* ctx, ID3D11RenderTargetView* pFramebuffer_RTV, ID3D11DepthStencilView*)
    {
        NV_PERFEVENT(ctx, "Render Frame");
        if (!pScene_->isCtxValid())
        {
            pScene_->createCtx(&platformDesc_);
        }

        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->ClearRenderTargetView( pSceneRT_->getRTV(), clear_color);
        ctx->ClearDepthStencilView(pSceneDepth_->getDSV(), D3D11_CLEAR_DEPTH, 1.0, 0);

        ID3D11Buffer * CBs[] = {
            pViewCB_->getCB(),
            pObjectCB_->getCB(),
            pLightCB_->getCB()
        };

        auto RenderScene = [=]()
        {
            NV_PERFEVENT(ctx, "Draw Scene Geometry");
            const float scene_range[] = { -6, -3, 3, 6 };
            for (auto x : scene_range)
            {
                for (auto y : scene_range)
                {
                    for (auto z : scene_range)
                    {
                        Scene::ObjectCB * pObject = this->pObjectCB_->Map(ctx);
                        pScene_->setupObjectCB(pObject, Nv::NvVec3(x, y, z));
                        this->pObjectCB_->Unmap(ctx);

                        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        ctx->IASetVertexBuffers(0,0,nullptr, nullptr,nullptr);
                        ctx->IASetInputLayout(nullptr);
                        ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
                        ctx->Draw(36, 0);
                    }
                }
            }
        };

        //----------------------------------------------------------------------
        // Render the light's shadow map
        {
            NV_PERFEVENT(ctx, "Render Shadow Map");
            {
                Scene::ViewCB * pView = pViewCB_->Map(ctx);
                pScene_->setupLightViewCB(pView);
                pViewCB_->Unmap(ctx);
            }

            ctx->ClearState();
            CD3D11_VIEWPORT shadowmap_viewport;
            shadowmap_viewport.TopLeftX = 0;
            shadowmap_viewport.TopLeftY = 0;
            shadowmap_viewport.Width = static_cast<float>(Scene::SHADOWMAP_RESOLUTION);
            shadowmap_viewport.Height = static_cast<float>(Scene::SHADOWMAP_RESOLUTION);
            shadowmap_viewport.MinDepth = 0.0f;
            shadowmap_viewport.MaxDepth = 1.0f;
            ctx->RSSetState(rs_render_);
            ctx->VSSetShader(shaders.scene_VS[scene_VS::SINGLE], nullptr, 0);
            ctx->VSSetConstantBuffers(0, 3, CBs);
            if (pScene_->getLightMode() == Scene::eLightMode::OMNI)
            {
                ctx->ClearDepthStencilView(pParaboloidShadowMap_->getDSV(), D3D11_CLEAR_DEPTH, 1.0, 0);
                ctx->OMSetRenderTargets(0, nullptr, pParaboloidShadowMap_->getDSV());
                CD3D11_VIEWPORT dual_viewports[] = { shadowmap_viewport, shadowmap_viewport };
                ctx->RSSetViewports(2, dual_viewports);
                ctx->GSSetShader(shaders.scene_GS[scene_GS::SINGLE], nullptr, 0);
                ctx->GSSetConstantBuffers(0, 3, CBs);
            }
            else
            {
                ctx->ClearDepthStencilView(pShadowMap_->getDSV(), D3D11_CLEAR_DEPTH, 1.0, 0);
                ctx->OMSetRenderTargets(0, nullptr, pShadowMap_->getDSV());
                ctx->RSSetViewports(1, &shadowmap_viewport);
            }
            ctx->PSSetShader(nullptr, nullptr, 0);
            ctx->OMSetBlendState(bs_no_color_, nullptr, 0xFFFFFFFF);
            ctx->OMSetDepthStencilState(ds_shadowmap_, 0xFF);
            RenderScene();
        }
        ID3D11ShaderResourceView * shadowmap_srv = (pScene_->getLightMode() == Scene::eLightMode::OMNI) ? pParaboloidShadowMap_->getSRV() : pShadowMap_->getSRV();

        //----------------------------------------------------------------------
        // Render the scene
        {
            NV_PERFEVENT(ctx, "Render Main Scene");
            {
                Scene::ViewCB * pView = pViewCB_->Map(ctx);
                Scene::LightCB * pLight = pLightCB_->Map(ctx);
                pScene_->setupSceneCBs(pView, pLight);
                pViewCB_->Unmap(ctx);
                pLightCB_->Unmap(ctx);
            }

            ctx->ClearState();
            CD3D11_VIEWPORT scene_viewport;
            scene_viewport.TopLeftX = 0.0f;
            scene_viewport.TopLeftY = 0.f;
            scene_viewport.Width = static_cast<float>(framebufferWidth_);
            scene_viewport.Height = static_cast<float>(framebufferHeight_);
            scene_viewport.MinDepth = 0.0f;
            scene_viewport.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &scene_viewport);
            ctx->RSSetState(rs_render_);
            ctx->VSSetShader(shaders.scene_VS[scene_VS::SINGLE], nullptr, 0);
            ctx->VSSetConstantBuffers(0, 3, CBs);
            ctx->GSSetShader(nullptr, nullptr, 0);
            scene_PS::Desc ps_desc;
            switch (pScene_->getLightMode())
            {
            default:
            case Scene::eLightMode::DIRECTIONAL:
                ps_desc.flags.LIGHTMODE = scene_PS::LIGHTMODE_DIRECTIONAL;
                break;

            case Scene::eLightMode::SPOTLIGHT:
                ps_desc.flags.LIGHTMODE = scene_PS::LIGHTMODE_SPOTLIGHT;
                break;

            case Scene::eLightMode::OMNI:
                ps_desc.flags.LIGHTMODE = scene_PS::LIGHTMODE_OMNI;
                break;
            }
            ctx->PSSetShader(shaders.scene_PS[ps_desc], nullptr, 0);
            ctx->PSSetConstantBuffers(0, 3, CBs);
            ID3D11ShaderResourceView * SRVs[] = { shadowmap_srv };
            ctx->PSSetShaderResources(0, 1, SRVs);
            ctx->PSSetSamplers(0, 1, &ss_shadowmap_);
            ID3D11RenderTargetView * RTVs[] = { pSceneRT_->getRTV() };
            ctx->OMSetRenderTargets(1, RTVs, pSceneDepth_->getDSV());
            RenderScene();
        }

        //----------------------------------------------------------------------
        // Render the volumetric lighting
        {
            NV_PERFEVENT(ctx, "Volumetric Lighting");
            ctx->ClearState();
            pScene_->beginAccumulation(ctx, pSceneDepth_->getSRV());
            pScene_->renderVolume(ctx, shadowmap_srv);
            pScene_->endAccumulation(ctx);
            pScene_->applyLighting(ctx, pSceneRT_->getRTV(), pSceneDepth_->getSRV());
        }

        //----------------------------------------------------------------------
        // Tonemap to output
        {
            NV_PERFEVENT(ctx, "Postprocess");
            ctx->ClearState();
            CD3D11_VIEWPORT scene_viewport;
            scene_viewport.TopLeftX = 0.0f;
            scene_viewport.TopLeftY = 0.f;
            scene_viewport.Width = static_cast<float>(framebufferWidth_);
            scene_viewport.Height = static_cast<float>(framebufferHeight_);
            scene_viewport.MinDepth = 0.0f;
            scene_viewport.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &scene_viewport);
            ctx->RSSetState(rs_render_);
            ID3D11RenderTargetView * RTVs[] = { pFramebuffer_RTV };
            ctx->OMSetRenderTargets(1, RTVs, nullptr);
            ctx->VSSetShader(shaders.quad_VS[quad_VS::SINGLE], nullptr, 0);
            ctx->PSSetShader(shaders.post_PS[post_PS::SINGLE], nullptr, 0);
            ID3D11ShaderResourceView * SRVs[] = { pSceneRT_->getSRV() };
            ctx->PSSetShaderResources(0, 1, SRVs);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
            ctx->IASetInputLayout(nullptr);
            ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
            ctx->Draw(3, 0);
        }
    }

    virtual void Animate(double fElapsedTimeSeconds)
    {
        pScene_->animate((float)fElapsedTimeSeconds);
    }

    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
    {
		hWnd; uMsg; wParam; lParam;
        switch (uMsg)
        {
        case WM_KEYDOWN:
            switch (wParam)
            {
            case VK_ESCAPE:
                PostQuitMessage(0);
                break;

            case VK_F1:
                pScene_->setDebugMode(Nv::Vl::DebugFlags::NONE);
                break;

            case VK_F2:
                pScene_->setDebugMode(Nv::Vl::DebugFlags::NO_BLENDING);
                break;

            case VK_F3:
                pScene_->setDebugMode(Nv::Vl::DebugFlags::WIREFRAME);
                break;

            case VK_F4:
                pManager_->ToggleFullscreen();
                break;

            case VK_SPACE:
                pScene_->togglePause();
                break;

            case 0x44: // D key - toggle downsample mode
                pScene_->toggleDownsampleMode();
                break;

            case 0x46: // F key - toggle fog
                pScene_->toggleFog();
                break;

            case 0x4C: // L key - toggle light type
                pScene_->toggleLightMode();
                break;

            case 0x4D: // M key - toggle MSAA mode
                pScene_->toggleMsaaMode();
                break;

            case 0x4F: // O key - toggle light intensity
                pScene_->toggleMediumType();
                break;

            case 0x50: // P key - toggle light intensity
                pScene_->toggleIntensity();
                break;

            case 0x54: // T key - toggle temporal filtering
                pScene_->toggleFiltering();
                break;

            case 0x55: // U key - toggle upsample mode
                pScene_->toggleUpsampleMode();
                break;
            
            case 0x56: // V key - toggle view mode
            pScene_->toggleViewpoint();
                break;
            }
            return 0;
        }
        return 1;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
////////////////////////////////////////////////////////////////////////////////////////////////////

int main_D3D11(Scene * pScene)
{
    // Enable run-time memory check for debug builds.
#if (NV_CHECKED)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

    AllocConsole();
    freopen("CONIN$", "r",stdin);
    freopen("CONOUT$", "w",stdout);
    freopen("CONOUT$", "w",stderr);
#endif

    DeviceManager * device_manager = new DeviceManager();

    auto scene_controller = SceneController(pScene, device_manager);
    device_manager->AddControllerToFront(&scene_controller);
    
    DeviceCreationParameters deviceParams;
    deviceParams.swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    deviceParams.swapChainSampleCount = 4;
    deviceParams.startFullscreen = false;
    deviceParams.backBufferWidth = 1920;
    deviceParams.backBufferHeight = 1080;
#if (NV_CHECKED)
    deviceParams.createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG; 
#endif
    
    if(FAILED(device_manager->CreateWindowDeviceAndSwapChain(deviceParams, L"Gameworks Volumetric Lighting Unit Test")))
    {
        MessageBox(nullptr, L"Cannot initialize the D3D11 device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    // Loop
    device_manager->MessageLoop();
    // Shutdown and exit
    device_manager->Shutdown();
    delete device_manager;

    return 0;
}
