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

namespace Nv
{
    using namespace nvidia;
} // namespace Nv

/*==============================================================================
    GFSDK Conversion stubs
==============================================================================*/

NV_FORCE_INLINE NvcVec2 NVtoNVC(const Nv::NvVec2 & rhs)
{
    return *reinterpret_cast<const NvcVec2 *>(&rhs);
}

NV_FORCE_INLINE NvcVec3 NVtoNVC(const Nv::NvVec3 & rhs)
{
    return *reinterpret_cast<const NvcVec3 *>(&rhs);
}

NV_FORCE_INLINE NvcVec4 NVtoNVC(const Nv::NvVec4 & rhs)
{
    return *reinterpret_cast<const NvcVec4 *>(&rhs);
}

NV_FORCE_INLINE NvcMat44 NVtoNVC(const Nv::NvMat44 & rhs)
{
    return *reinterpret_cast<const NvcMat44 *>(&rhs);
}


NV_FORCE_INLINE Nv::NvMat44 PerspectiveProjLH(float fov, float aspect, float zn, float zf)
{
    Nv::NvMat44 p(Nv::NvZero);
    float cot_fov = Nv::intrinsics::cos(fov) / Nv::intrinsics::sin(fov);
    p(0, 0) = cot_fov / aspect;
    p(1, 1) = cot_fov;
    p(2, 2) = zf / (zf - zn);
    p(2, 3) = -zn * zf / (zf - zn);
    p(3, 2) = 1.0;
    return p;
}

NV_FORCE_INLINE Nv::NvMat44 OrthographicProjLH(float width, float height, float zn, float zf)
{
    Nv::NvMat44 p(Nv::NvZero);
    p(0, 0) = 2.0f / width;
    p(1, 1) = 2.0f / height;
    p(2, 2) = 1 / (zf - zn);
    p(2, 3) = zn / (zf - zn);
    p(3, 3) = 1.0f;
    return p;
}

NV_FORCE_INLINE Nv::NvMat44 LookAtTransform(const Nv::NvVec3 & eye, const Nv::NvVec3 & at, const Nv::NvVec3 & up)
{
    Nv::NvVec3 zaxis = at - eye;
    zaxis.normalize();
    Nv::NvVec3 xaxis = up.cross(zaxis);
    xaxis.normalize();
    Nv::NvVec3 yaxis = zaxis.cross(xaxis);
    return Nv::NvMat44(
        Nv::NvVec4(xaxis.x, yaxis.x, zaxis.x, 0),
        Nv::NvVec4(xaxis.y, yaxis.y, zaxis.y, 0),
        Nv::NvVec4(xaxis.z, yaxis.z, zaxis.z, 0),
        Nv::NvVec4(-xaxis.dot(eye), -yaxis.dot(eye), -zaxis.dot(eye), 1));
}

NV_FORCE_INLINE Nv::NvMat44 Inverse(const Nv::NvMat44 & in)
{
    const float * m = in.front();
    float inv[16];
    float det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
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
    Helper Macros
==============================================================================*/

#ifdef _DEBUG
#   include <stdio.h>
#   include <assert.h>
#   if defined(WIN31) || defined(WIN63)
#       define LOG(fmt, ...) { char debug_string[1024]; _snprintf_c(debug_string, 1024, fmt, ##__VA_ARGS__); OutputDebugStringA(debug_string); }
#       define ASSERT_LOG(test, msg, ...) if (!(test)) { LOG(msg "\n", ##__VA_ARGS__); DebugBreak(); }
#   else
#       define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#       define ASSERT_LOG(test, msg, ...) if (!(test)) { LOG(msg, ##__VA_ARGS__); abort(); }
#   endif
#else
#   define LOG(fmt, ...) 
#   define ASSERT_LOG(test, msg, ...)
#endif

#define VALIDATE(r, e) if (FAILED(r)) { LOG("Call Failure: %u\n", r); return e; };
#define SIZE_OF_ARRAY(a) (sizeof(a)/sizeof(a[0]))
#define SAFE_DELETE(x) if((x) != nullptr) {delete (x); (x)=nullptr;}
#define SAFE_RELEASE(x) if((x) != nullptr) {(x)->Release(); (x)=nullptr;}
#define SAFE_RELEASE_ARRAY(x) for (unsigned _sr_count=0; _sr_count<SIZE_OF_ARRAY(x); ++_sr_count) {if((x)[_sr_count] != nullptr) {((IUnknown *)(x)[_sr_count])->Release(); (x)[_sr_count]=nullptr;}}
#define SAFE_DELETE_ARRAY(x) if (x != nullptr) {SAFE_RELEASE_ARRAY(x); delete[] x; x=nullptr;}

#endif  // COMMON_H
