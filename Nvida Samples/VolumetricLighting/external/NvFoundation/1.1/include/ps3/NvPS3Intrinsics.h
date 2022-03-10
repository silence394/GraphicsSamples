// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
#ifndef NV_PS3_NVPS3INTRINSICS_H
#define NV_PS3_NVPS3INTRINSICS_H

#include "Nv.h"
#include "NvAssert.h"

#if !NV_PS3
#error "This file should only be included by ps3 builds!!"
#endif

#include <math.h>
#ifdef __SPU__
#include "spu_intrinsics.h"
#else
#include "ppu_intrinsics.h"
#endif

namespace nvidia
{
namespace intrinsics
{
//! \brief platform-specific absolute value
NV_FORCE_INLINE float abs(float a)
{
	return ::fabsf(a);
}

//! \brief platform-specific select float
#ifdef __SPU__
NV_FORCE_INLINE float fsel(float a, float b, float c)
{
	return (a >= 0 ? b : c);
}
#else
NV_FORCE_INLINE float fsel(float a, float b, float c)
{
	return __fsels(a, b, c);
}
#endif

//! \brief platform-specific sign
#ifdef __SPU__
NV_FORCE_INLINE float sign(float a)
{
	return (a >= 0 ? 1.0f : -1.0f);
}
#else
NV_FORCE_INLINE float sign(float a)
{
	return __fsels(a, 1.0f, -1.0f);
}
#endif

//! \brief platform-specific reciprocal
NV_FORCE_INLINE float recip(float a)
{
	return 1.0f / a;
}

//! \brief platform-specific reciprocal estimate
#if defined(__SPU__) || !defined(_PPU_INTRINSICS_GCC_H)
NV_FORCE_INLINE float recipFast(float a)
{
	return 1.0f / a;
}
#else
NV_FORCE_INLINE float recipFast(float a)
{
	return __fres(a);
}
#endif

//! \brief platform-specific square root
NV_FORCE_INLINE float sqrt(float a)
{
	return ::sqrtf(a);
}

//! \brief platform-specific reciprocal square root
NV_FORCE_INLINE float recipSqrt(float a)
{
	return 1.0f / ::sqrtf(a);
}

//! \brief platform-specific reciprocal square root estimate
#ifdef __SPU__
NV_FORCE_INLINE float recipSqrtFast(float a)
{
	return 1.0f / ::sqrtf(a);
}
#else
NV_FORCE_INLINE float recipSqrtFast(float a)
{
	return float(__frsqrte(a));
}
#endif

//! \brief platform-specific sine
NV_FORCE_INLINE float sin(float a)
{
	return ::sinf(a);
}

//! \brief platform-specific cosine
NV_FORCE_INLINE float cos(float a)
{
	return ::cosf(a);
}

//! \brief platform-specific minimum
#ifdef __SPU__
NV_FORCE_INLINE float selectMin(float a, float b)
{
	return (a >= b ? b : a);
}
#else
NV_FORCE_INLINE float selectMin(float a, float b)
{
	return __fsels(a - b, b, a);
}
#endif

//! \brief platform-specific maximum
#ifdef __SPU__
NV_FORCE_INLINE float selectMax(float a, float b)
{
	return (a >= b ? a : b);
}
#else
NV_FORCE_INLINE float selectMax(float a, float b)
{
	return __fsels(a - b, a, b);
}
#endif
//! \brief platform-specific finiteness check (not INF or NAN)
NV_FORCE_INLINE bool isFinite(float a)
{
	return !isnan(a) && !isinf(a);
}

//! \brief platform-specific finiteness check (not INF or NAN)
NV_FORCE_INLINE bool isFinite(double a)
{
	return !isnan(a) && !isinf(a);
}

/*!
Sets \c count bytes starting at \c dst to zero.
*/
NV_FORCE_INLINE void* memZero(void* NV_RESTRICT dest, uint32_t count)
{
	return memset(dest, 0, count);
}

/*!
Sets \c count bytes starting at \c dst to \c c.
*/
NV_FORCE_INLINE void* memSet(void* NV_RESTRICT dest, int32_t c, uint32_t count)
{
	return memset(dest, c, count);
}

/*!
Copies \c count bytes from \c src to \c dst. User memMove if regions overlap.
*/
NV_FORCE_INLINE void* memCopy(void* NV_RESTRICT dest, const void* NV_RESTRICT src, uint32_t count)
{
	return memcpy(dest, src, count);
}

/*!
Copies \c count bytes from \c src to \c dst. Supports overlapping regions.
*/
NV_FORCE_INLINE void* memMove(void* NV_RESTRICT dest, const void* NV_RESTRICT src, uint32_t count)
{
	return memmove(dest, src, count);
}

/*!
Set 128B to zero starting at \c dst+offset. Must be aligned.
*/
NV_FORCE_INLINE void memZero128(void* NV_RESTRICT dest, uint32_t offset = 0)
{
	NV_ASSERT(((size_t(dest) + offset) & 0x7f) == 0);
	memSet((char*)dest + offset, 0, 128);
}

} // namespace intrinsics
} // namespace nvidia

#endif // #ifndef NV_PS3_NVPS3INTRINSICS_H
