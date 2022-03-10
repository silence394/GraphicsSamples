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
// SCE CONFIDENTIAL
// Copyright (C) Sony Computer Entertainment Inc.
// All Rights Reserved.

#ifndef NV_PS3_NVPS3ASSERT_H
#define NV_PS3_NVPS3ASSERT_H

#include <NvFoundation/NvPreprocessor.h>

#ifdef NV_SPU
#include "spu_printf.h"

namespace nvidia
{
NV_INLINE void NvPs3Assert(const char* exp, const char* file, int line)
{
	spu_printf("SPU: Assertion failed! exp %s \n, line %d \n, file %s \n ", exp, line, file);
	__builtin_snpause();
}
}

#define NV_ASSERT(exp) ((void)(!!(exp) || (nvidia::NvPs3Assert(#exp, __FILE__, __LINE__), false)))
#define NV_ALWAYS_ASSERT_MESSAGE(exp) nvidia::NvPs3Assert(exp, __FILE__, __LINE__)
#define NV_ASSERT_WITH_MESSAGE(exp, message)                                                                           \
	((void)(!!(exp) || (nvidia::NvPs3Assert(message, __FILE__, __LINE__), false)))
#endif // NV_SPU
#endif // #ifndef NV_PS3_NVPS3ASSERT_H
