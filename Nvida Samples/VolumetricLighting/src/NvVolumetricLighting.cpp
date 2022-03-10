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
#include <NvAllocatorCallback.h>

#include "common.h"
#include "context_common.h"

#if defined( NV_PLATFORM_D3D11 )
#	include "d3d11/context_d3d11.h"
#endif

// [Add other platforms here]

////////////////////////////////////////////////////////////////////////////////

namespace
{
    bool g_isLoaded = false;
}

/*==============================================================================
   Override memory operators
==============================================================================*/
namespace
{
    // Default allocator
    class DefaultAllocator : public Nv::NvAllocatorCallback
    {
        virtual void* allocate(size_t size, const char* typeName, const char* filename, int line)
        {
            LOG("GWVL Allocation: %u bytes (%s:%d)", size, filename, line);
            return malloc(size);
        }

        virtual void deallocate(void* ptr)
        {
            free(ptr);
        }
    } g_defaultAllocator;

    // Actual allocator to use
    Nv::NvAllocatorCallback * g_allocator;
}

#pragma push_macro("new")
#undef new
void * operator new(size_t size, const char * filename, int line)
{
    return g_allocator->allocate(size, "Gameworks Volumetric Lighting", filename, line);
}

void operator delete(void * ptr, const char * filename, int line)
{
    filename; line;
    g_allocator->deallocate(ptr);
}

void operator delete (void * ptr)
{
    g_allocator->deallocate(ptr);
}

void * operator new[](size_t size, const char * filename, int line)
{
    return g_allocator->allocate(size, "Gameworks Volumetric Lighting", filename, line);
}

void operator delete[](void * ptr, const char * filename, int line)
{
    filename; line;
    g_allocator->deallocate(ptr);
}

void operator delete[](void * ptr)
{
    g_allocator->deallocate(ptr);
}
#pragma pop_macro("new")

/*==============================================================================
   Assert handler
==============================================================================*/

namespace
{
    Nv::NvAssertHandler * g_assert;

    class DefaultAssertHandler : public Nv::NvAssertHandler
    {
        virtual void operator()(const char* exp, const char* file, int line, bool& ignore)
        {
            LOG("%s(%d): Assertion Failed (%s)", file, line, exp);
            BREAK();
        }
    } g_defaultAssertHandler;
};

namespace nvidia
{
    NvAssertHandler& NvGetAssertHandler()
    {
        return *g_assert;
    }
}

////////////////////////////////////////////////////////////////////////////////
namespace Nv { namespace VolumetricLighting { 
////////////////////////////////////////////////////////////////////////////////

/*==============================================================================
   API Functions
==============================================================================*/

//------------------------------------------------------------------------------
//! Load the library and initialize global state
NV_VOLUMETRICLIGHTING_API(Status) OpenLibrary(NvAllocatorCallback * allocator, NvAssertHandler * assert_handler, const VersionDesc & link_version)
{
	if ( link_version.Major != LIBRARY_VERSION.Major || link_version.Minor != LIBRARY_VERSION.Minor)
		return Status::INVALID_VERSION;

    g_allocator = (allocator) ? allocator : &g_defaultAllocator;
    g_assert = (assert_handler) ? assert_handler : &g_defaultAssertHandler;

    g_isLoaded = true;
    return Status::OK;
}

//------------------------------------------------------------------------------
//! Release the library and resources, and un-initialize all global state
NV_VOLUMETRICLIGHTING_API(Status) CloseLibrary()
{
    if (g_isLoaded == false)
        return Status::UNINITIALIZED;

    g_isLoaded = false;
    return Status::OK;
}

//------------------------------------------------------------------------------
//! Create a new context for rendering to a specified framebuffer
NV_VOLUMETRICLIGHTING_API(Status) CreateContext(Context & out_ctx, const PlatformDesc * pPlatformDesc, const ContextDesc * pContextDesc)
{
    if (g_isLoaded == false)
        return Status::UNINITIALIZED;

	if (pPlatformDesc == nullptr || pContextDesc == nullptr)
		return Status::INVALID_PARAMETER;
	
	out_ctx = nullptr;
	switch (pPlatformDesc->platform)
	{

#   if defined(NV_PLATFORM_D3D11)
		case PlatformName::D3D11:
		{
            ContextImp_D3D11 * ctx_imp = nullptr;
            Status create_status = ContextImp_D3D11::Create(&ctx_imp, pPlatformDesc, pContextDesc);
			if (create_status != Status::OK)
			{
				return create_status;
			}
			else
			{
                out_ctx = static_cast<ContextImp_Common *>(ctx_imp);
				return Status::OK;
			}
		}
#   endif
        // [Add other platforms here]

		default:
			return Status::INVALID_PARAMETER;
	}	
}

//------------------------------------------------------------------------------
//! Release the context and any associated resources
NV_VOLUMETRICLIGHTING_API(Status) ReleaseContext(Context & ctx)
{
    if (ctx)
    {
        ContextImp_Common * vl_ctx = reinterpret_cast<ContextImp_Common *>(ctx);
        delete vl_ctx;
        ctx = nullptr;
        return Status::OK;
    }
    else
    {
        return Status::INVALID_PARAMETER;
    }
}

//------------------------------------------------------------------------------
//! Begin accumulation of lighting volumes for a view
NV_VOLUMETRICLIGHTING_API(Status) BeginAccumulation(Context ctx, PlatformRenderCtx renderCtx, PlatformShaderResource sceneDepth, ViewerDesc const* pViewerDesc, MediumDesc const* pMediumDesc, DebugFlags debugFlags)
{
    return reinterpret_cast<ContextImp_Common *>(ctx)->BeginAccumulation(renderCtx, sceneDepth, pViewerDesc, pMediumDesc, debugFlags);
}

//------------------------------------------------------------------------------
//! Add a lighting volume to the accumulated results
NV_VOLUMETRICLIGHTING_API(Status) RenderVolume(Context ctx, PlatformRenderCtx renderCtx, PlatformShaderResource shadowMap, ShadowMapDesc const* pShadowMapDesc, LightDesc const* pLightDesc, VolumeDesc const* pVolumeDesc)
{
    return reinterpret_cast<ContextImp_Common *>(ctx)->RenderVolume(renderCtx, shadowMap, pShadowMapDesc, pLightDesc, pVolumeDesc);
}

//------------------------------------------------------------------------------
//! Finish accumulation of lighting volumes
NV_VOLUMETRICLIGHTING_API(Status) EndAccumulation(Context ctx, PlatformRenderCtx renderCtx)
{
    return reinterpret_cast<ContextImp_Common *>(ctx)->EndAccumulation(renderCtx);
}

//------------------------------------------------------------------------------
//! Resolve the results and composite into the provided scene
NV_VOLUMETRICLIGHTING_API(Status) ApplyLighting(Context ctx, PlatformRenderCtx renderCtx, PlatformRenderTarget sceneTarget, PlatformShaderResource sceneDepth, PostprocessDesc const* pPostprocessDesc)
{
    return reinterpret_cast<ContextImp_Common *>(ctx)->ApplyLighting(renderCtx, sceneTarget, sceneDepth, pPostprocessDesc);
}

////////////////////////////////////////////////////////////////////////////////
}; /*namespace VolumetricLighting*/ }; /*namespace Nv*/
////////////////////////////////////////////////////////////////////////////////
