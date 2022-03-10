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

#ifndef COMMON_H
#define COMMON_H
////////////////////////////////////////////////////////////////////////////////

#include <NvPreprocessor.h>
#include <NvAssert.h>
#include <NvIntrinsics.h>
#include <NvMath.h>
#include <NvFoundationMath.h>
#include <NvCTypes.h>

#if (NV_WIN32 || NV_WIN64)
#include <Windows.h>
#endif

namespace Nv
{
    using namespace nvidia;
} // namespace Nv

////////////////////////////////////////////////////////////////////////////////

/*==============================================================================
    GFSDK Conversion stubs
==============================================================================*/

NV_FORCE_INLINE Nv::NvVec2 NVCtoNV(const NvcVec2 & rhs)
{
    return *reinterpret_cast<const Nv::NvVec2 *>(&rhs);
}

NV_FORCE_INLINE Nv::NvVec3 NVCtoNV(const NvcVec3 & rhs)
{
    return *reinterpret_cast<const Nv::NvVec3 *>(&rhs);
}

NV_FORCE_INLINE Nv::NvVec4 NVCtoNV(const NvcVec4 & rhs)
{
    return *reinterpret_cast<const Nv::NvVec4 *>(&rhs);
}

NV_FORCE_INLINE Nv::NvMat44 NVCtoNV(const NvcMat44 & rhs)
{
    return *reinterpret_cast<const Nv::NvMat44 *>(&rhs);
}

NV_FORCE_INLINE Nv::NvMat44 Inverse(const Nv::NvMat44 & in)
{
    const float * m = in.front();
    float inv[16];
    float det;
    int i;

    inv[0] = m[5] * m[10] * m[15] -
        m[5] * m[11] * m[14] -
        m[9] * m[6] * m[15] +
        m[9] * m[7] * m[14] +
        m[13] * m[6] * m[11] -
        m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] +
        m[4] * m[11] * m[14] +
        m[8] * m[6] * m[15] -
        m[8] * m[7] * m[14] -
        m[12] * m[6] * m[11] +
        m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] -
        m[4] * m[11] * m[13] -
        m[8] * m[5] * m[15] +
        m[8] * m[7] * m[13] +
        m[12] * m[5] * m[11] -
        m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] +
        m[4] * m[10] * m[13] +
        m[8] * m[5] * m[14] -
        m[8] * m[6] * m[13] -
        m[12] * m[5] * m[10] +
        m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] +
        m[1] * m[11] * m[14] +
        m[9] * m[2] * m[15] -
        m[9] * m[3] * m[14] -
        m[13] * m[2] * m[11] +
        m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] -
        m[0] * m[11] * m[14] -
        m[8] * m[2] * m[15] +
        m[8] * m[3] * m[14] +
        m[12] * m[2] * m[11] -
        m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] +
        m[0] * m[11] * m[13] +
        m[8] * m[1] * m[15] -
        m[8] * m[3] * m[13] -
        m[12] * m[1] * m[11] +
        m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] -
        m[0] * m[10] * m[13] -
        m[8] * m[1] * m[14] +
        m[8] * m[2] * m[13] +
        m[12] * m[1] * m[10] -
        m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] -
        m[1] * m[7] * m[14] -
        m[5] * m[2] * m[15] +
        m[5] * m[3] * m[14] +
        m[13] * m[2] * m[7] -
        m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] +
        m[0] * m[7] * m[14] +
        m[4] * m[2] * m[15] -
        m[4] * m[3] * m[14] -
        m[12] * m[2] * m[7] +
        m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] -
        m[0] * m[7] * m[13] -
        m[4] * m[1] * m[15] +
        m[4] * m[3] * m[13] +
        m[12] * m[1] * m[7] -
        m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] +
        m[0] * m[6] * m[13] +
        m[4] * m[1] * m[14] -
        m[4] * m[2] * m[13] -
        m[12] * m[1] * m[6] +
        m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
        m[1] * m[7] * m[10] +
        m[5] * m[2] * m[11] -
        m[5] * m[3] * m[10] -
        m[9] * m[2] * m[7] +
        m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
        m[0] * m[7] * m[10] -
        m[4] * m[2] * m[11] +
        m[4] * m[3] * m[10] +
        m[8] * m[2] * m[7] -
        m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
        m[0] * m[7] * m[9] +
        m[4] * m[1] * m[11] -
        m[4] * m[3] * m[9] -
        m[8] * m[1] * m[7] +
        m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
        m[0] * m[6] * m[9] -
        m[4] * m[1] * m[10] +
        m[4] * m[2] * m[9] +
        m[8] * m[1] * m[6] -
        m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return Nv::NvMat44(Nv::NvZero);

    det = 1.0f / det;

    for (i = 0; i < 16; i++)
        inv[i] = inv[i] * det;

    return Nv::NvMat44(inv);
}
/*==============================================================================
    Helper macros
==============================================================================*/

#if (NV_DEBUG)
#	include <stdio.h>
#	include <assert.h>
#	if (NV_WIN32 || NV_WIN64)
#		define LOG(fmt, ...) { char debug_string[1024]; _snprintf_c(debug_string, 1024, fmt "\n", ##__VA_ARGS__); OutputDebugStringA(debug_string); }
#		define BREAK() DebugBreak();
#	else
#		define LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#		define BREAK() abort();
#	endif
#		define ASSERT_LOG(test, msg, ...) if (!(test)) { LOG(msg, ##__VA_ARGS__); NV_ALWAYS_ASSERT(); }
#else
#	define LOG(fmt, ...) 
#   define ASSERT_LOG(test, msg, ...)
#   define BREAK() 
#endif

// Validate internal call
#define V_(f) { Status v_s = f; if (v_s != Status::OK) return v_s; }

// Validate D3D call
#define VD3D_(f) { HRESULT v_hr = f; if (FAILED(v_hr)) {LOG("Call Failure: %u", v_hr); return Status::API_ERROR;} }

// Validate resource creation
#define V_CREATE(x, f) x = f; if (x == nullptr) return Status::RESOURCE_FAILURE;

#define SIZE_OF_ARRAY(a) (sizeof(a)/sizeof(a[0]))
#define SAFE_DELETE(x) if((x) != nullptr) {delete (x); (x)=nullptr;}
#define SAFE_RELEASE(x) if((x) != nullptr) {((IUnknown *)(x))->Release(); (x)=nullptr;}
#define SAFE_RELEASE_ARRAY(x) for (unsigned _sr_count=0; _sr_count<SIZE_OF_ARRAY(x); ++_sr_count) {if((x)[_sr_count] != nullptr) {((IUnknown *)(x)[_sr_count])->Release(); (x)[_sr_count]=nullptr;}}
#define SAFE_DELETE_ARRAY(x) if (x != nullptr) {SAFE_RELEASE_ARRAY(x); delete[] x; x=nullptr;}

/*==============================================================================
    Memory Management
==============================================================================*/

void * operator new(size_t size, const char * filename, int line);
void operator delete(void * ptr, const char * filename, int line);
void operator delete (void * ptr);
void * operator new[](size_t size, const char * filename, int line);
void operator delete[](void * ptr, const char * filename, int line);
void operator delete[](void * ptr);

#if (NV_CHECKED)
#   define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#   define new new(__FILENAME__, __LINE__)
#else
#   define new new("NvVolumetricLighting.dll", 0)
#endif

////////////////////////////////////////////////////////////////////////////////
#endif  // COMMON_H
