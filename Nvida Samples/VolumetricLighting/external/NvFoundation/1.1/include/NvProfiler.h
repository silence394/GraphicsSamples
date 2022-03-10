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

#ifndef NV_PROFILER_H
#define NV_PROFILER_H

#include <NvSimpleTypes.h>

namespace nvidia
{
	class NvProfilerCallback;
	namespace shdfnd
	{
		NV_FOUNDATION_API NvProfilerCallback *getProfilerCallback();
		NV_FOUNDATION_API void setProfilerCallback(NvProfilerCallback *profiler);
	}
}


namespace nvidia
{

struct NvProfileTypes
{
	enum Enum
	{
		eNORMAL = 0, //!< ordinary profile zone, starts and ends in same thread
		eSTALL = 1, //!< thread is busy but can't progress (example: spin-lock)
		eIDLE = 2, //!< thread is idle (example: waiting for event)
		eDETACHED = 3, //!< zone crosses thread boundary
		eLOCK = 4, //!< thread tries to acquire a lock, reports result on zoneEnd()
		eLOCK_SUCCESS = 5, //!< locking mutex succeded, to be passed to zoneEnd()
		eLOCK_FAILED = 6, //!< locking mutex failed, to be passed to zoneEnd()
		eLOCK_TIMEOUT = 7 //!< locking mutex timed out, to be passed to zoneEnd()
	};
};

struct NvProfileContext
{
	enum Enum
	{
		eNONE = 0 //!< value for no specific profile context. \see NvProfilerCallback::zoneAt
	};
};


/**
\brief The pure virtual callback interface for general purpose instrumentation and profiling of GameWorks modules as well as applications
*/
class NvProfilerCallback
{
public:

	/**************************************************************************************************************************
	Instrumented profiling events
	***************************************************************************************************************************/

	/**
	\brief Mark the beginning of a nested profile block
	\param eventName Event name.  Must be a persistent const char *
	\param type What type this zone is (i.e. normal, cross thread, lock, etc.). eLOCK_* should not be used here.
	\param contextId the context id of this zone. Zones with the same id belong to the same group. 0 is used for no specific group.
	\param filename The source code filename where this profile event begins
	\param lineno The source code line number where this profile event begins
	*/
	virtual void zoneStart(const char* eventName, NvProfileTypes::Enum type, uint64_t contextId, const char *filename, int lineno) = 0;

	/**
	\brief Mark the end of a nested profile block
	\param eventName The name of the zone ending, must match the corresponding name passed with 'zoneStart'.   Must be a persistent const char *
	\param type What type this zone is (i.e. normal, cross thread, lock, etc.). Should match the value passed to zoneStart, except if eLOCK was passed to zoneStart. In this case, type should be one of eLOCK_*.
	\param contextId The context of this zone. Should match the value passed to zoneStart.
	\note eventName plus contextId can be used to uniquely match up start and end of a zone.
	*/
	virtual void zoneEnd(const char *eventName, NvProfileTypes::Enum type, uint64_t contextId) = 0;

	/**************************************************************************************************************************
	TimeSpan events; profiling events not associated with any specific thread.  
	***************************************************************************************************************************/

	/**
	\brief Return current time
	This function is called to calibrate the start and end times passed to zoneAt().
	*/
	virtual uint64_t getTime() = 0;

	/**
	\brief Send a discrete zone of data with manually recorded time stamps
	\param start The timestamp and the start of the span
	\param end The timestamp at the end of the span
	\param name The name of this event.   Must be a persistent const char *
	\param type What type this zone is (i.e. normal, cross thread, lock, etc.). eLOCK or eLOCK_* should not be used here.
	\param contextId The context id for this event
	\param filename The source code filename where this zone is being sent from
	\param linenumber The source code line number where this zone is being sent from
	*/
	virtual void zoneAt(uint64_t start, uint64_t end, const char* name, NvProfileTypes::Enum type, uint64_t contextId, const char* filename, int linenumber) = 0;

	/**************************************************************************************************************************
	Bundled CUDA warp profile events
	***************************************************************************************************************************/

	/**
	\brief Bundled profiling data relating to CUDA warps
	*/
	struct WarpProfileEvent
	{
		uint16_t block;
		uint8_t  warp;
		uint8_t  mpId;
		uint8_t  hwWarpId;
		uint8_t  userDataCfg;
		uint16_t eventId;
		uint32_t startTime;
		uint32_t endTime;
	};

	/**
	\brief Converts a string into a unique 16 bit integer value; used when sending CUDA kernel warp data.
	\param str The source string
	\return Returns the unique id associated with this string which can later be passed via the WarpProfileEvent structure
	*/
	virtual uint16_t getStringID(const char *str) = 0;

	/**
	\brief Defines the format version number for the kernel warp data struct
	*/
	static const uint32_t CurrentCUDABufferFormat = 1;

	/**
	\brief Send a CUDA profile buffer. We assume the submit time is almost exactly the end time of the batch.
	 We then work backwards, using the batchRuntimeInMilliseconds in order to get the original time
	 of the batch.  The buffer format is described in GPUProfile.h.
	 
	 \param batchRuntimeInMilliseconds The batch runtime in milliseconds, see cuEventElapsedTime.
	 \param cudaData An opaque pointer to the buffer of cuda data.
	 \param eventCount number of events
	 \param bufferVersion Version of the format of the cuda data.
	 */
	virtual void CUDAProfileBuffer( float batchRuntimeInMilliseconds, const WarpProfileEvent* cudaData, uint32_t eventCount, uint32_t bufferVersion = CurrentCUDABufferFormat ) = 0;

	protected:
		virtual ~NvProfilerCallback(void) {}
};

class NvProfileScoped
{
public:
	NV_FORCE_INLINE NvProfileScoped(const char *eventName, NvProfileTypes::Enum type, uint64_t contextId, const char *fileName, int lineno)
		: mCallback(nvidia::shdfnd::getProfilerCallback())
	{
		if ( mCallback )
		{
			mEventName = eventName;
			mType = type;
			mContextId = contextId;
			mCallback->zoneStart(mEventName, mType, mContextId, fileName, lineno);
		}
	}
	~NvProfileScoped(void)
	{
		if ( mCallback )
		{
			mCallback->zoneEnd(mEventName, mType, mContextId);
		}
	}
	nvidia::NvProfilerCallback *mCallback;
	const char					*mEventName;
	NvProfileTypes::Enum		mType;
	uint64_t					mContextId;
};



} // end of NVIDIA namespace



#if NV_DEBUG || NV_CHECKED || NV_PROFILE

#define NV_PROFILE_ZONE(x,y) nvidia::NvProfileScoped NV_CONCAT(_scoped,__LINE__)(x,nvidia::NvProfileTypes::eNORMAL,y,__FILE__,__LINE__)
#define NV_PROFILE_START_CROSSTHREAD(x,y) if ( nvidia::shdfnd::getProfilerCallback() ) nvidia::shdfnd::getProfilerCallback()->zoneStart(x,nvidia::NvProfileTypes::eDETACHED,y,__FILE__,__LINE__)
#define NV_PROFILE_STOP_CROSSTHREAD(x,y) if ( nvidia::shdfnd::getProfilerCallback() ) nvidia::shdfnd::getProfilerCallback()->zoneEnd(x,nvidia::NvProfileTypes::eDETACHED,y)

#else

#define NV_PROFILE_ZONE(x,y) 
#define NV_PROFILE_START_CROSSTHREAD(x,y)
#define NV_PROFILE_STOP_CROSSTHREAD(x,y)

#endif

#define NV_PROFILE_POINTER_TO_U64( pointer ) static_cast<uint64_t>(reinterpret_cast<size_t>(pointer))

#endif
