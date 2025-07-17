//
//  nixtla.c
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//
//  This entire notice must be retained in this source code.
//  This source code is under MIT Licence.
//
//  This software is provided "as is", with absolutely no warranty expressed
//  or implied. Any use is at your own risk.
//
//  Latest fixes enhancements and documentation at https://github.com/marcosjom/lib-nixtla-audio
//

#include "nixtla-audio-private.h"
#include "nixtla-audio.h"
//
#include "nixtla-openal.h"
#include "nixtla-aaudio.h"
#include "nixtla-avfaudio.h"
//
#include <stdio.h>  //NULL
#include <string.h> //memcpy, memset

//-------------------------------
//-- IDENTIFY OS
//-------------------------------
#if defined(_WIN32) || defined(WIN32) //Windows
#   pragma message("NIXTLA-AUDIO, COMPILING FOR WINDOWS (OpenAL as default API)")
#   include <AL/al.h>
#   include <AL/alc.h>
#   define NIX_DEFAULT_API_OPENAL
#elif defined(__ANDROID__) //Android
//#   pragma message("NIXTLA-AUDIO, COMPILING FOR ANDROID (OpenSL)")
//#   include <SLES/OpenSLES.h>
//#   include <SLES/OpenSLES_Android.h>
//#   define NIX_OPENSL
#   pragma message("NIXTLA-AUDIO, COMPILING FOR ANDROID 8+ (API 26+, Oreo) (AAudio as default API)")
#   define NIX_DEFAULT_API_AAUDIO
#elif defined(__QNX__) //QNX 4, QNX Neutrino, or BlackBerry 10 OS
#   pragma message("NIXTLA-AUDIO, COMPILING FOR QNX / BB10 (OpenAL as default API)")
#   include <AL/al.h>
#   include <AL/alc.h>
#   define NIX_DEFAULT_API_OPENAL
#elif defined(__linux__) || defined(linux) //Linux
#   pragma message("NIXTLA-AUDIO, COMPILING FOR LINUX (OpenAL as default API)")
#   include <AL/al.h>	//remember to install "libopenal-dev" package
#   include <AL/alc.h> //remember to install "libopenal-dev" package
#   define NIX_DEFAULT_API_OPENAL
#elif defined(__MAC_OS_X_VERSION_MAX_ALLOWED) //OSX
#   pragma message("NIXTLA-AUDIO, COMPILING FOR MacOSX (AVFAudio as default API)")
#   define NIX_DEFAULT_API_AVFAUDIO
#else	//iOS?
#   pragma message("NIXTLA-AUDIO, COMPILING FOR iOS? (AVFAudio as default API)")
#   define NIX_DEFAULT_API_AVFAUDIO
#endif

//Tmp
#undef NIX_DEFAULT_API_AAUDIO
#undef NIX_DEFAULT_API_AVFAUDIO
#define NIX_DEFAULT_API_OPENAL
#pragma message("NIXTLA-AUDIO, FORCING OpenAL as default API")

//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++
//++ PRIVATE HEADER ++
//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++

//Audio description
   
NixBOOL STNix_audioDesc_IsEqual(const STNix_audioDesc* obj, const STNix_audioDesc* other){
    return (obj->samplesFormat == other->samplesFormat
            && obj->channels == other->channels
            && obj->bitsPerSample == other->bitsPerSample
            && obj->samplerate == other->samplerate
            && obj->blockAlign == other->blockAlign) ? NIX_TRUE : NIX_FALSE;
}

//-------------------------------
//-- AUDIO BUFFERS
//-------------------------------
typedef enum ENNixBufferState_ {
	ENNixBufferState_Free = 50				//Empty, data was played or waiting for data capture
	, ENNixBufferState_LoadedForPlay		//Filled with data but not attached to source queue
	, ENNixBufferState_AttachedForPlay		//Filled with data and attached to source queue
	//
	, ENNixBufferState_AttachedForCapture	//Filled with data and attached to capture queue
	, ENNixBufferState_LoadedWithCapture	//Filled with data but not attached to capture queue
#   ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
	, ENNixBufferState_WaitingForDeletion	//The buffer was unqueued and is waiting for deletion (OpenAL gives error when deleting a buffer inmediately after unqueue)
#   endif
} ENNixBufferState;

//Audio buffer description
typedef struct STNix_bufferDesc_ {
	ENNixBufferState	state;
	STNix_audioDesc		audioDesc;
	NixUI8*				dataPointer;
	NixUI32				dataBytesCount;
} STNix_bufferDesc;

typedef struct STNix_bufferAL_ {
	NixUI8				regInUse;
	NixUI32				retainCount;
    STNixApiBuffer      apiBuff;
    STNixApiBufferItf   api;
	STNix_bufferDesc	bufferDesc;
#   ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
	NixUI32				_msWaitingForDeletion;
#   endif
} STNix_bufferAL;

#define NIX_BUFFER_SECONDS(FLT_DEST, BUFFER) if(BUFFER.audioDesc.channels != 0 && BUFFER.audioDesc.bitsPerSample != 0 && BUFFER.audioDesc.samplerate != 0) FLT_DEST = ((float)BUFFER.dataBytesCount / (float)(BUFFER.audioDesc.channels * (BUFFER.audioDesc.bitsPerSample / 8)) / (float)BUFFER.audioDesc.samplerate); else FLT_DEST=0.0f;
#define NIX_BUFFER_SECONDS_P(FLT_DEST, BUFFER) if(BUFFER->audioDesc.channels != 0 && BUFFER->audioDesc.bitsPerSample != 0 && BUFFER->audioDesc.samplerate != 0) FLT_DEST = ((float)BUFFER->dataBytesCount / (float)(BUFFER->audioDesc.channels * (BUFFER->audioDesc.bitsPerSample / 8)) / (float)BUFFER->audioDesc.samplerate); else FLT_DEST=0.0f;

#define NIX_BUFFER_IS_COMPATIBLE(BUFFER, CHANNELS, BITPS, FREQ) (BUFFER.channels == CHANNELS && BUFFER.bitsPerSample == BITPS && BUFFER.samplerate == FREQ)
#define NIX_BUFFER_IS_COMPATIBLE_P(BUFFER, CHANNELS, BITPS, FREQ) (BUFFER->channels == CHANNELS && BUFFER->bitsPerSample == BITPS && BUFFER->samplerate == FREQ)

#define NIX_BUFFER_ARE_COMPATIBLE(BUFFER1, BUFFER2) (BUFFER1.channels == BUFFER2.channels && BUFFER1.bitsPerSample == BUFFER2.bitsPerSample && BUFFER1.samplerate == BUFFER2.samplerate)
#define NIX_BUFFER_ARE_COMPATIBLE_P(BUFFER1, BUFFER2) (BUFFER1->channels == BUFFER2->channels && BUFFER1->bitsPerSample == BUFFER2->bitsPerSample && BUFFER1->samplerate == BUFFER2->samplerate)

//-------------------------------
//-- AUDIO SOURCES
//-------------------------------
typedef enum ENNixSourceState_ {
	ENNixSourceState_Stopped = 0,		//Source is stopped or just-initialized, and ready to be assigned
	ENNixSourceState_Playing,			//Source is playing or paused
} ENNixSourceState;

#define NIX_STR_SOURCE_STATE(SRCSTATE)	(SRCSTATE == ENNixSourceState_Stopped ? "Stopped" : SRCSTATE == ENNixSourceState_Playing ? "Playing" : "State_???")

//Source type according to attached buffers
typedef enum ENNixSourceType_ {
	ENNixSourceType_Undefined = 90,
	ENNixSourceType_Static, //buffers remains attached
	ENNixSourceType_Stream  //buffers will be detached to be filled with new samples
} ENNixSourceType;

#define NIX_STR_SOURCE_TYPE(SRCTYPE) (SRCTYPE == ENNixSourceType_Undefined ? "Undefined" : SRCTYPE == ENNixSourceType_Static ? "Static" : SRCTYPE == ENNixSourceType_Stream ? "Stream" : "ENNixSourceType_notValid")

typedef struct STNix_source_ {
    STNixApiSource      apiSource;
    STNixApiSourceItf   api;
	NixUI8		regInUse;
	NixUI8		sourceType;			//ENNixSourceType
	NixUI8		sourceState;		//ENNixSourceState
	NixUI8		audioGroupIndex;	//Audio group index
	float		volume;				//Individual volume (this sound)
	//
	NixUI8		isReusable;
	NixUI32		retainCount;
	//Callbacks
	PTRNIX_SourceReleaseCallback releaseCallBack;
	void*		releaseCallBackUserData;
	PTRNIX_StreamBufferUnqueuedCallback bufferUnqueuedCallback;
	void*		bufferUnqueuedCallbackData;
	//Index of buffers attached to this source
	NixUI16*	queueBuffIndexes;
	NixUI16		queueBuffIndexesUse;
	NixUI16		queueBuffIndexesSize;
	NixUI16		buffStreamUnqueuedCount;	//For streams, how many buffers had been unqueued since last "nixTick()". For bufferUnqueuedCallback.
} STNix_source;

typedef struct STNix_EngineObjetcs_ {
	NixUI32				maskCapabilities;
    STNixApiEngine      apiEngine;
    STNixApiItf         api;
	//Audio sources
	STNix_source*		sourcesArr;
	NixUI16				sourcesArrUse;
	NixUI16				sourcesArrSize;
	//Audio buffers (complete short sounds and stream portions)
	STNix_bufferAL*		buffersArr;
	NixUI16				buffersArrUse;
	NixUI16				buffersArrSize;
	//Audio capture
    STNixApiRecorder    apiRecorder;
	NixUI8				captureInProgess;
	NixUI32				captureSamplesPerBuffer;
	STNix_audioDesc		captureFormat;
	//Capture buffers
	STNix_bufferDesc*	buffersCaptureArr;
	NixUI16				buffersCaptureArrFirst;
	NixUI16				buffersCaptureArrFilledCount;
	NixUI16				buffersCaptureArrSize;
	PTRNIX_CaptureBufferFilledCallback buffersCaptureCallback;
	void*				buffersCaptureCallbackUserData;
} STNix_EngineObjetcs;

//-------------------------------
//-- AUDIO GROUPS
//-------------------------------
typedef struct STNix_AudioGroup_ {
	NixBOOL				enabled;
	float				volume;
} STNix_AudioGroup;

STNix_AudioGroup __nixSrcGroups[NIX_AUDIO_GROUPS_SIZE];

void    __nixSourceBufferQueueCallback(void* pEng, NixUI32 sourceIndex, const NixUI32 ammBuffs); //AVFAUDIO specific

//Audio buffers
NixUI16	__nixBufferCreate(STNix_EngineObjetcs* eng);
void	__nixBufferDestroy(STNix_bufferAL* buffer, const NixUI16 bufferIndex);
void	__nixBufferRetain(STNix_bufferAL* buffer);
void	__nixBufferRelease(STNix_bufferAL* buffer, const NixUI16 bufferIndex);
NixBOOL __nixBufferSetData(STNix_EngineObjetcs* eng, STNix_bufferAL* buffer, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL __nixBufferFillZeroes(STNix_EngineObjetcs* eng, STNix_bufferAL* buffer);

//Audio sources
void	__nixSourceInit(STNix_source* src);
void	__nixSourceFinalize(STNix_Engine* engAbs, STNix_source* src, const NixUI16 sourceIndex, const NixBOOL forReuse);
void	__nixSourceRetain(STNix_source* source);
void	__nixSourceRelease(STNix_Engine* engAbs, STNix_source* source, const NixUI16 sourceIndex);
NixUI16	__nixSourceAdd(STNix_EngineObjetcs* eng, STNix_source* source);
NixUI16	__nixSourceAssign(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData, const NixUI16 queueSize, PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback, void* bufferUnqueueCallbackData);
//Audio sources queue
NixUI16	__nixSrcQueueFirstBuffer(STNix_source* src);
void	__nixSrcQueueSetUniqueBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex);
void	__nixSrcQueueAddStreamBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex);
void	__nixSrcQueueAddBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex);
void	__nixSrcQueueClear(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex);
void	__nixSrcQueueRemoveBuffersOldest(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex, NixUI32 buffersCountFromOldest);
void	__nixSrcQueueRemoveBuffersNewest(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex, NixUI32 buffersCountFromNewest);

//++++++++++++++++++
//++++++++++++++++++
//++++++++++++++++++
//++ PRIVATE CODE ++
//++++++++++++++++++
//++++++++++++++++++
//++++++++++++++++++

//
#define NIX_GET_SOURCE_START(ENG_OBJS, SRC_INDEX, SRC_DEST_POINTER) \
	NIX_ASSERT(SRC_INDEX != 0) \
	NIX_ASSERT(SRC_INDEX < ENG_OBJS->sourcesArrSize) \
	if(SRC_INDEX != 0 && SRC_INDEX < ENG_OBJS->sourcesArrSize){ \
		STNix_source* SRC_DEST_POINTER = &ENG_OBJS->sourcesArr[SRC_INDEX]; \
		NIX_ASSERT(SRC_DEST_POINTER->regInUse) \
		NIX_ASSERT(SRC_DEST_POINTER->retainCount != 0 /*|| SRC_DEST_POINTER->isReusable*/) \
		if(SRC_DEST_POINTER->regInUse && (SRC_DEST_POINTER->retainCount != 0 /*|| SRC_DEST_POINTER->isReusable*/))

#define NIX_GET_SOURCE_END \
	} \

#define NIX_GET_BUFFER_START(ENG_OBJS, BUFF_INDEX, BUFF_DEST_POINTER) \
	NIX_ASSERT(BUFF_INDEX != 0) \
	NIX_ASSERT(BUFF_INDEX < ENG_OBJS->buffersArrUse) \
	if(BUFF_INDEX != 0 && BUFF_INDEX < ENG_OBJS->buffersArrUse){ \
		STNix_bufferAL* BUFF_DEST_POINTER = &ENG_OBJS->buffersArr[BUFF_INDEX]; \
		NIX_ASSERT(BUFF_DEST_POINTER->regInUse) \
		if(BUFF_DEST_POINTER->regInUse)

#define NIX_GET_BUFFER_END \
	} \

//----------------------------------------------------------
//--- Audio buffers
//----------------------------------------------------------
NixUI16 __nixBufferCreate(STNix_EngineObjetcs* eng){
	STNix_bufferAL audioBuffer;
    memset(&audioBuffer, 0, sizeof(audioBuffer));
	NixUI16 i; const NixUI16 useCount	= eng->buffersArrUse;
	//Format struct
	audioBuffer.regInUse				= NIX_TRUE;
	audioBuffer.retainCount				= 1; //retained by creator
	audioBuffer.bufferDesc.state		= ENNixBufferState_Free;
	//audioBuffer.bufferDesc.audioDesc
	audioBuffer.bufferDesc.dataBytesCount = 0;
	audioBuffer.bufferDesc.dataPointer	= NULL;
	//Look for a available register
	for(i=1; i < useCount; i++){ //Source index zero is reserved
		STNix_bufferAL* source = &eng->buffersArr[i];
		if(!source->regInUse){
			eng->buffersArr[i]		    = audioBuffer;
			NIX_PRINTF_INFO("Buffer created reusing(%d).\n", i);
			return i;
		}
	}
	//Add new register
	if(eng->buffersArrUse < NIX_BUFFERS_MAX){
		if(eng->buffersArrSize >= eng->buffersArrUse){
			STNix_bufferAL* buffersArr;
			eng->buffersArrSize	+= NIX_BUFFERS_GROWTH;
            NIX_MALLOC(buffersArr, STNix_bufferAL, sizeof(STNix_bufferAL) * eng->buffersArrSize, "buffersArr");
			if(eng->sourcesArr != NULL){
				if(eng->sourcesArrUse != 0) memcpy(buffersArr, eng->buffersArr, sizeof(STNix_bufferAL) * eng->buffersArrUse);
				NIX_FREE(eng->buffersArr);
			}
			eng->buffersArr		= buffersArr;
		}
		eng->buffersArr[eng->buffersArrUse++] = audioBuffer;
		NIX_PRINTF_INFO("Buffer created allocating(%d).\n", (eng->buffersArrUse-1));
		return (eng->buffersArrUse - 1);
	}
	//Cleanup, if something went wrong
	__nixBufferDestroy(&audioBuffer, 0);
	return 0;
}

void __nixBufferDestroy(STNix_bufferAL* buffer, const NixUI16 bufferIndex){
	NIX_ASSERT(buffer->regInUse)
    if(buffer->apiBuff.opq != NULL && buffer->api.destroy != NULL){
        (*buffer->api.destroy)(buffer->apiBuff);
        buffer->apiBuff = (STNixApiBuffer)STNixApiBuffer_Zero;
    }
	if(buffer->bufferDesc.dataPointer != NULL){
        NIX_FREE(buffer->bufferDesc.dataPointer);
		buffer->bufferDesc.dataPointer = NULL;
	}
	buffer->regInUse = NIX_FALSE;
}

void __nixBufferRetain(STNix_bufferAL* buffer){
	NIX_ASSERT(buffer->regInUse)
	buffer->retainCount++;
}

void __nixBufferRelease(STNix_bufferAL* buffer, const NixUI16 bufferIndex){
	NIX_ASSERT(buffer->regInUse)
	NIX_ASSERT(buffer->retainCount != 0)
	buffer->retainCount--;
	if(buffer->retainCount == 0){
#        ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
		NIX_PRINTF_INFO("Buffer Programming destruction(%d).\n", bufferIndex);
		buffer->_msWaitingForDeletion	= 0;
		buffer->bufferDesc.state		= ENNixBufferState_WaitingForDeletion;
#        else
		__nixBufferDestroy(buffer, bufferIndex);
#        endif
	}
}

NixBOOL __nixBufferSetData(STNix_EngineObjetcs* eng, STNix_bufferAL* buffer, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(buffer->apiBuff.opq == NULL){
        if(eng->api.buffer.create != NULL){
            buffer->apiBuff = (*eng->api.buffer.create)(audioDesc, audioDataPCM, audioDataPCMBytes);
            if(buffer->apiBuff.opq == NULL){
                NIX_PRINTF_ERROR("buffer->apiBuff.opq failed.\n");
            } else {
                buffer->api = eng->api.buffer;
                r = NIX_TRUE;
            }
        }
    } else {
        if(buffer->api.setData != NULL){
            if(!(*buffer->api.setData)(buffer->apiBuff, audioDesc, audioDataPCM, audioDataPCMBytes)){
                NIX_PRINTF_ERROR("buffer->api.setData failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    if(r){
        NIX_ASSERT(buffer->regInUse)
        NIX_ASSERT(buffer->retainCount != 0)
        buffer->bufferDesc.audioDesc = *audioDesc;
        if(buffer->bufferDesc.dataPointer != NULL){
            NIX_FREE(buffer->bufferDesc.dataPointer);
        }
        NIX_MALLOC(buffer->bufferDesc.dataPointer, NixUI8, sizeof(NixUI8) * audioDataPCMBytes, "bufferDesc.dataPointer");
        if(audioDataPCM != NULL){
            memcpy(buffer->bufferDesc.dataPointer, audioDataPCM, sizeof(NixUI8) * audioDataPCMBytes);
        }
        buffer->bufferDesc.dataBytesCount = audioDataPCMBytes;
        buffer->bufferDesc.state          = ENNixBufferState_LoadedForPlay;
        return NIX_TRUE;
    }
	return NIX_FALSE;
}

NixBOOL __nixBufferFillZeroes(STNix_EngineObjetcs* eng, STNix_bufferAL* buffer){
    NixBOOL r = NIX_FALSE;
    if(buffer->apiBuff.opq != NULL){
        if(buffer->api.fillWithZeroes != NULL){
            if(!(*buffer->api.fillWithZeroes)(buffer->apiBuff)){
                NIX_PRINTF_ERROR("buffer->api.fillWithZeroes failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

//----------------------------------------------------------
//--- Audio sources
//----------------------------------------------------------
void __nixSourceInit(STNix_source* src){
    memset(src, 0, sizeof(*src));
	src->regInUse					= NIX_TRUE;
	src->sourceType					= ENNixSourceType_Undefined;
	src->sourceState				= ENNixSourceState_Stopped;
	src->audioGroupIndex			= 0;
	src->volume						= 1.0f;
	//
	src->isReusable					= NIX_FALSE;
	src->retainCount				= 1; //retained by creator
	src->releaseCallBack			= NULL;
	src->releaseCallBackUserData	= NULL;
	//
	src->queueBuffIndexes			= NULL;
	src->queueBuffIndexesUse		= 0;
	src->queueBuffIndexesSize		= 0;
	//
	src->buffStreamUnqueuedCount	= 0;
}

NixUI16 __nixSourceAdd(STNix_EngineObjetcs* eng, STNix_source* source){
	//Look for a available register
	NixUI16 i; const NixUI16 useCount = eng->sourcesArrUse;
	for(i=1; i < useCount; i++){ //Source index zero is reserved
		STNix_source* src = &eng->sourcesArr[i];
		if(!src->regInUse){
			(*src) = (*source);
			NIX_PRINTF_INFO("Source created(%d) using free register.\n", i);
			return i;
		}
	}
	//Add new register
	if(eng->sourcesArrUse < NIX_SOURCES_MAX){
		if(eng->sourcesArrSize >= eng->sourcesArrUse){
			STNix_source* sourcesArr;
			eng->sourcesArrSize	+= NIX_SOURCES_GROWTH;
            NIX_MALLOC(sourcesArr, STNix_source, sizeof(STNix_source) * eng->sourcesArrSize, "sourcesArr");
			if(eng->sourcesArr != NULL){
				if(eng->sourcesArrUse != 0) memcpy(sourcesArr, eng->sourcesArr, sizeof(STNix_source) * eng->sourcesArrUse);
				NIX_FREE(eng->sourcesArr);
			}
			eng->sourcesArr		= sourcesArr;
		}
		eng->sourcesArr[eng->sourcesArrUse++] = (*source);
		NIX_PRINTF_INFO("Source created(%d) growing array.\n", (eng->sourcesArrUse - 1));
		return (eng->sourcesArrUse - 1);
	}
	return 0;
}

void __nixSourceFinalize(STNix_Engine* engAbs, STNix_source* source, const NixUI16 sourceIndex, const NixBOOL forReuse){
	STNix_EngineObjetcs* eng	= (STNix_EngineObjetcs*)engAbs->o;
	NIX_ASSERT(source->regInUse == NIX_TRUE)
	//Stop if necesary
	if(source->sourceState != ENNixSourceState_Stopped){
        if(source->apiSource.opq != NULL){
            if(source->api.stop != NULL){
                (*source->api.stop)(source->apiSource);
            }
        }
        source->sourceState = ENNixSourceState_Stopped;
	}
	//Release buffer queue
	if(source->queueBuffIndexes != NULL){
		__nixSrcQueueClear(eng, source, sourceIndex);
		NIX_FREE(source->queueBuffIndexes);
		source->queueBuffIndexes = NULL;
		source->queueBuffIndexesUse = 0;
		source->queueBuffIndexesSize = 0;
	}
	//Notify callback
	if(source->releaseCallBack != NULL){
		(*source->releaseCallBack)(engAbs, source->releaseCallBackUserData, sourceIndex);
		source->releaseCallBack = NULL;
		source->releaseCallBackUserData = NULL;
	}
    source->sourceType = ENNixSourceType_Undefined;
	//Delete source
	if(!forReuse){
        if(source->apiSource.opq != NULL){
            if(source->api.destroy != NULL){
                (*source->api.destroy)(source->apiSource);
            }
            source->apiSource = (STNixApiSource)STNixApiSource_Zero;
        }
		//PENDIENTE: liberar bufferes asociados a la fuente
		source->regInUse = NIX_FALSE;
		NIX_PRINTF_INFO("Source destroyed(%d).\n", sourceIndex);
	} else {
		NIX_PRINTF_INFO("Source destroyed for reuse(%d).\n", sourceIndex);
	}
}

void __nixSourceRetain(STNix_source* source){
	NIX_ASSERT(source->regInUse)
	source->retainCount++;
}

void __nixSourceRelease(STNix_Engine* engAbs, STNix_source* source, const NixUI16 sourceIndex){
	NIX_ASSERT(source->regInUse)
	NIX_ASSERT(source->retainCount != 0)
    if(source->retainCount == 1){
        __nixSourceFinalize(engAbs, source, sourceIndex, source->isReusable);
    }
	source->retainCount--;
}

/*float __nixSourceSecondsInBuffers(STNix_source* src){
	float total = 0.0f, secsBuffer;
	NixUI32 i; const NixUI32 useCount = src->buffersArrUse;
	STNix_bufferDesc* buffDesc;
	for(i=0; i < useCount; i++){
		buffDesc = &src->buffersArr[i]._buffer.bufferDesc;
		NIX_BUFFER_SECONDS_P(secsBuffer, buffDesc);
		total += secsBuffer;
	}
	return total;
}*/

//----------------------------------------------------------
//--- Sources queues
//----------------------------------------------------------

void __nixSrcQueueSetUniqueBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex){
	NIX_ASSERT(src->queueBuffIndexesUse == 0 || (src->sourceType == ENNixSourceType_Static && src->queueBuffIndexesUse == 1)); //the buffer should be empty or be a non-stream buffer
	__nixSrcQueueAddBuffer(src, buffer, buffIndex);
	src->sourceType = ENNixSourceType_Static;
}

void __nixSrcQueueAddStreamBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex){
	NIX_ASSERT(src->sourceType == ENNixSourceType_Stream || src->sourceType == ENNixSourceType_Undefined) //Should not be used out of a stream or undefined sources
	__nixSrcQueueAddBuffer(src, buffer, buffIndex);
	src->sourceType = ENNixSourceType_Stream;
}

void __nixSrcQueueAddBuffer(STNix_source* src, STNix_bufferAL* buffer, const NixUI16 buffIndex){
	if(src->queueBuffIndexesSize <= src->queueBuffIndexesUse){
		NixUI16* buffArr;
		src->queueBuffIndexesSize += 4;
        NIX_MALLOC(buffArr, NixUI16, sizeof(NixUI16) * src->queueBuffIndexesSize, "queueBuffIndexes");
		if(src->queueBuffIndexes != NULL){
			if(src->queueBuffIndexesUse != 0) memcpy(buffArr, src->queueBuffIndexes, sizeof(NixUI16) * src->queueBuffIndexesUse);
			NIX_FREE(src->queueBuffIndexes);
		}
		src->queueBuffIndexes = buffArr;
	}
	src->queueBuffIndexes[src->queueBuffIndexesUse++] = buffIndex;
	__nixBufferRetain(buffer);
	buffer->bufferDesc.state = ENNixBufferState_AttachedForPlay;
}

void __nixSrcQueueClear(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex){
    __nixSrcQueueRemoveBuffersOldest(eng, src, sourceIndex, src->queueBuffIndexesUse);
}

void __nixSrcQueueRemoveBuffersOldest(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex, NixUI32 buffersCountFromOldest){
	if(buffersCountFromOldest > 0){
		NixUI32 i;
		NIX_ASSERT(src->sourceType == ENNixSourceType_Stream) //Must be a stream source
		NIX_ASSERT(buffersCountFromOldest <= src->queueBuffIndexesUse)
		NIX_PRINTF_INFO("Source(%d, %s), unqueueing %d of %d buffers.\n", sourceIndex, NIX_STR_SOURCE_TYPE(src->sourceType), buffersCountFromOldest, src->queueBuffIndexesUse);
		if(buffersCountFromOldest > src->queueBuffIndexesUse) buffersCountFromOldest = src->queueBuffIndexesUse;
		//Release buffers
		for(i=0; i < buffersCountFromOldest; i++){
			const NixUI16 buffIndex	= src->queueBuffIndexes[i];
			NIX_GET_BUFFER_START(eng, buffIndex, buffer){
				NIX_PRINTF_INFO("Unqueueing source(%d, %s) buffer(%d, retainCount=%d) from queue.\n", sourceIndex, NIX_STR_SOURCE_TYPE(src->sourceType), buffIndex, buffer->retainCount);
				NIX_ASSERT(buffer->bufferDesc.state == ENNixBufferState_AttachedForPlay)
				buffer->bufferDesc.state = ENNixBufferState_LoadedForPlay;
				__nixBufferRelease(buffer, buffIndex);
			} NIX_GET_BUFFER_END
		}
		//Rearrange array elements
		src->queueBuffIndexesUse -= buffersCountFromOldest;
		for(i=0; i < src->queueBuffIndexesUse; i++){
			src->queueBuffIndexes[i] = src->queueBuffIndexes[i + buffersCountFromOldest];
		}
		if(src->queueBuffIndexesUse == 0){
			NIX_PRINTF_INFO("Source(%d) empty queue, changed from type('%s') to (%d 'Undefined').\n", sourceIndex, NIX_STR_SOURCE_TYPE(src->sourceType), ENNixSourceType_Undefined);
			src->sourceType = ENNixSourceType_Undefined;
		}
	}
}

void __nixSrcQueueRemoveBuffersNewest(STNix_EngineObjetcs* eng, STNix_source* src, const NixUI16 sourceIndex, NixUI32 buffersCountFromNewest){
	NixUI32 i;
	NIX_ASSERT(buffersCountFromNewest <= src->queueBuffIndexesUse)
	NIX_PRINTF_INFO("Source(%d), unqueueing %d of %d buffers.\n", sourceIndex, buffersCountFromNewest, src->queueBuffIndexesUse);
	if(buffersCountFromNewest > src->queueBuffIndexesUse) buffersCountFromNewest = src->queueBuffIndexesUse;
	//Release buffers
	for(i=(src->queueBuffIndexesUse - buffersCountFromNewest); i < src->queueBuffIndexesUse; i++){
		const NixUI16 buffIndex	= src->queueBuffIndexes[i];
		NIX_GET_BUFFER_START(eng, buffIndex, buffer){
			NIX_PRINTF_INFO("Unqueueing source(%d) buffer(%d, retainCount=%d) from queue.\n", sourceIndex, buffIndex, buffer->retainCount);
			NIX_ASSERT(buffer->bufferDesc.state == ENNixBufferState_AttachedForPlay)
			buffer->bufferDesc.state = ENNixBufferState_LoadedForPlay;
			__nixBufferRelease(buffer, buffIndex);
		} NIX_GET_BUFFER_END
	}
	src->queueBuffIndexesUse -= buffersCountFromNewest;
	if(src->queueBuffIndexesUse == 0){
		NIX_PRINTF_INFO("Source(%d) empty queue, changed from type(%d) to (%d 'Undefined')", sourceIndex, src->sourceType, ENNixSourceType_Undefined);
		src->sourceType = ENNixSourceType_Undefined;
	}
}

NixUI16 __nixSrcQueueFirstBuffer(STNix_source* src){
	if(src->queueBuffIndexesUse > 0) return src->queueBuffIndexes[0];
	return 0;
}

//-------------------------------
//-- ENGINES
//-------------------------------

NixBOOL nixInitBase_(STNix_EngineObjetcs* eng, const NixUI16 pregeneratedSources){
    NixBOOL r = NIX_FALSE;
    eng->maskCapabilities            = 0;
    //
    eng->sourcesArrUse                = 1; //Source index zero is reserved
    eng->sourcesArrSize                = 1 + (pregeneratedSources != 0 ? pregeneratedSources : NIX_SOURCES_GROWTH);
    NIX_MALLOC(eng->sourcesArr, STNix_source, sizeof(STNix_source) * eng->sourcesArrSize, "sourcesArr");
    eng->sourcesArr[0].regInUse        = 0;
    //
    eng->buffersArrUse                = 1; //Buffer index zero is reserved
    eng->buffersArrSize                = 1 + NIX_BUFFERS_GROWTH;
    NIX_MALLOC(eng->buffersArr, STNix_bufferAL, sizeof(STNix_bufferAL) * eng->buffersArrSize, "buffersArr");
    eng->buffersArr[0].regInUse        = 0;
    //
    eng->captureInProgess            = 0;
    eng->captureSamplesPerBuffer    = 0;
    //eng->captureFormat
    //
    eng->buffersCaptureArr            = NULL;
    eng->buffersCaptureArrFirst        = 0;
    eng->buffersCaptureArrFilledCount = 0;
    eng->buffersCaptureArrSize        = 0;
    //Audio groups
    {
        NixUI16 i;
        for(i=0; i < NIX_AUDIO_GROUPS_SIZE; i++){
            __nixSrcGroups[i].enabled    = NIX_TRUE;
            __nixSrcGroups[i].volume    = 1.0f;
        }
    }
    r = NIX_TRUE;
    return r;
}
    

NixBOOL nixInit(STNix_Engine* engAbs, const NixUI16 pregeneratedSources){
    NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng; NIX_MALLOC(eng, STNix_EngineObjetcs, sizeof(STNix_EngineObjetcs), "STNix_EngineObjetcs");
    memset(eng, 0, sizeof(*eng));
    engAbs->o = eng;
    if(!nixInitBase_(eng, pregeneratedSources)){
        NIX_PRINTF_ERROR("nixInitBase_ failed.\n");
    } else {
        //API
#       ifdef NIX_DEFAULT_API_AVFAUDIO
        if(!nixAVAudioEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixAVAudioEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
#       elif defined(NIX_DEFAULT_API_AAUDIO)
        if(!nixAAudioEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixAVAudioEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
#       elif defined(NIX_DEFAULT_API_OPENAL)
        if(!nixOpenALEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixOpenALEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
#       endif
    }
    //ToDo: pregenerate sources
    //Pregenerate reusable sources
    /*{
        NixUI16 sourcesCreated = 0;
        while(sourcesCreated < pregeneratedSources){
            ALuint idFuenteAL; alGenSources(1, &idFuenteAL); //el ID=0 es valido en OpenAL (no usar '0' para validar los IDs)
            if(alGetError() != AL_NONE){
                break;
            } else {
                STNix_source audioSource;
                __nixSourceInit(&audioSource);
                audioSource.idSourceAL        = idFuenteAL;
                audioSource.isReusable        = NIX_TRUE;
                audioSource.retainCount        = 0; //Not retained by creator
                if(__nixSourceAdd(eng, &audioSource) == 0){
                    alDeleteSources(1, &idFuenteAL); NIX_OPENAL_ERR_VERIFY("alDeleteSources");
                    break;
                } else {
                    sourcesCreated++;
                }
            }
        }
    }*/
	return r;
}

/*
NixBOOL nixInitWithOpenAL(STNix_Engine* engAbs, const NixUI16 pregeneratedSources){
    NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng; NIX_MALLOC(eng, STNix_EngineObjetcs, sizeof(STNix_EngineObjetcs), "STNix_EngineObjetcs");
    memset(eng, 0, sizeof(*eng));
    engAbs->o = eng;
    if(!nixInitBase_(eng, pregeneratedSources)){
        NIX_PRINTF_ERROR("nixInitBase_ failed.\n");
    } else {
        //API
        if(!nixOpenALEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixOpenALEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixOpenALEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
    }
    return r;
}
*/

/*
NixBOOL nixInitWithAVFAudio(STNix_Engine* engAbs, const NixUI16 pregeneratedSources){
    NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng; NIX_MALLOC(eng, STNix_EngineObjetcs, sizeof(STNix_EngineObjetcs), "STNix_EngineObjetcs");
    memset(eng, 0, sizeof(*eng));
    engAbs->o = eng;
    if(!nixInitBase_(eng, pregeneratedSources)){
        NIX_PRINTF_ERROR("nixInitBase_ failed.\n");
    } else {
        //API
        if(!nixAVAudioEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixAVAudioEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
    }
    return r;
}
*/

/*
NixBOOL nixInitWithAAudio(STNix_Engine* engAbs, const NixUI16 pregeneratedSources){
    NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng; NIX_MALLOC(eng, STNix_EngineObjetcs, sizeof(STNix_EngineObjetcs), "STNix_EngineObjetcs");
    memset(eng, 0, sizeof(*eng));
    engAbs->o = eng;
    if(!nixInitBase_(eng, pregeneratedSources)){
        NIX_PRINTF_ERROR("nixInitBase_ failed.\n");
    } else {
        //API
        if(!nixAAudioEngine_getApiItf(&eng->api)){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf failed.\n");
        } else if(eng->api.engine.create == NULL){
            NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf provided NULL for eng->api.engine.create.\n");
        } else {
            eng->apiEngine = (*eng->api.engine.create)();
            if(eng->apiEngine.opq == NULL){
                NIX_PRINTF_ERROR("nixAVAudioEngine_getApiItf::eng->api.engine.create provided NULL engine.\n");
            } else {
                NIX_PRINTF_INFO("nixAVAudioEngine_getApiItf::eng->api.engine.create OK.\n");
                r = NIX_TRUE;
            }
        }
    }
    return r;
}
*/

void nixGetStatusDesc(STNix_Engine* engAbs, STNix_StatusDesc* dest){
	STNix_EngineObjetcs* eng	= (STNix_EngineObjetcs*)engAbs->o;
	NixUI16 i;
	const NixUI16 useSources	= eng->sourcesArrUse;
	const NixUI16 usePlayBuffs	= eng->buffersArrUse;
	const NixUI16 useRecBuffs	= eng->buffersCaptureArrSize;
	//Format destination
	dest->countSources			= 0;
	dest->countSourcesReusable	= 0;
	dest->countSourcesAssigned	= 0;
	dest->countSourcesStatic	= 0;
	dest->countSourcesStream	= 0;
	//
	dest->countPlayBuffers		= 0;
	dest->sizePlayBuffers		= 0;
	dest->sizePlayBuffersAtSW	= 0;
	//
	dest->countRecBuffers		= 0;
	dest->sizeRecBuffers		= 0;
	dest->sizeRecBuffersAtSW	= 0;
	//Read sources
	for(i=1; i < useSources; i++){
		STNix_source* source	= &eng->sourcesArr[i];
		if(source->regInUse){
			dest->countSources++;
			if(source->isReusable) dest->countSourcesReusable++;
			if(source->retainCount != 0) dest->countSourcesAssigned++;
			if(source->sourceType == ENNixSourceType_Static) dest->countSourcesStatic++;
			if(source->sourceType == ENNixSourceType_Stream) dest->countSourcesStream++;
		}
	}
	//Read play buffers
	for(i=1; i < usePlayBuffs; i++){
		STNix_bufferAL* buffer	= &eng->buffersArr[i];
		if(buffer->regInUse){
			dest->countPlayBuffers++;
			dest->sizePlayBuffers += buffer->bufferDesc.dataBytesCount;
			if(buffer->bufferDesc.dataPointer != NULL) dest->sizePlayBuffersAtSW += buffer->bufferDesc.dataBytesCount;
		}
	}
	//Read capture buffers
    //ToDo: implement
    //dest->sizeRecBuffers = ...
	for(i=0; i < useRecBuffs; i++){
		STNix_bufferDesc* buffer = &eng->buffersCaptureArr[i];
		dest->countRecBuffers++;
		dest->sizeRecBuffers	+= buffer->dataBytesCount;
		if(buffer->dataPointer != NULL) dest->sizeRecBuffersAtSW	+= buffer->dataBytesCount;
	}
}

NixUI32	nixCapabilities(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	return eng->maskCapabilities;
}

void nixPrintCaps(STNix_Engine* engAbs){
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiEngine.opq != NULL && eng->api.engine.printCaps != NULL){
        (*eng->api.engine.printCaps)(eng->apiEngine);
    }
}

void nixFinalize(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NixUI16 buffersInUse = 0, sourcesInUse = 0;
	//Destroy capture
	nixCaptureFinalize(engAbs);
	//Destroy sources
	if(eng->sourcesArr != NULL){
		NixUI16 i; const NixUI16 useCount = eng->sourcesArrUse;
		for(i=1; i < useCount; i++){ //Source index zero is reserved
			STNix_source* source = &eng->sourcesArr[i];
			if(source->regInUse){
				__nixSourceFinalize(engAbs, source, i, NIX_FALSE/*not for reuse*/);
				sourcesInUse++;
			}
		}
		NIX_FREE(eng->sourcesArr); eng->sourcesArr = NULL;
	}
	//Destroy buffers
	if(eng->buffersArr != NULL){
		NixUI16 i; const NixUI16 useCount = eng->buffersArrUse;
		for(i=1; i < useCount; i++){
			STNix_bufferAL* buffer = &eng->buffersArr[i];
			if(buffer->regInUse){
				__nixBufferDestroy(buffer, i);
				buffersInUse++;
			}
		}
		NIX_FREE(eng->buffersArr); eng->buffersArr = NULL;
	}
    if(sourcesInUse > 0 || buffersInUse > 0){
        NIX_PRINTF_WARNING("nixFinalize, forced elimination of %d sources and %d buffers.\n", sourcesInUse, buffersInUse);
    }
	//Destroy context and device
    if(eng->apiEngine.opq != NULL){
        if(eng->api.engine.destroy != NULL){
            (*eng->api.engine.destroy)(eng->apiEngine);
            eng->apiEngine = (STNixApiEngine)STNixApiEngine_Zero;
        }
    }
	//
    NIX_FREE(eng);
	engAbs->o = NULL;
}

//

NixBOOL nixContextIsActive(STNix_Engine* engAbs){
	NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiEngine.opq != NULL && eng->api.engine.ctxIsActive != NULL){
        r = (*eng->api.engine.ctxIsActive)(eng->apiEngine);
    }
	return r;
}

NixBOOL nixctxActivate(STNix_Engine* engAbs){
	NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiEngine.opq != NULL && eng->api.engine.ctxActivate != NULL){
        r = (*eng->api.engine.ctxActivate)(eng->apiEngine);
    }
	return r;
}

NixBOOL nixctxDeactivate(STNix_Engine* engAbs){
	NixBOOL r = NIX_FALSE;
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiEngine.opq != NULL && eng->api.engine.ctxDeactivate != NULL){
        r = (*eng->api.engine.ctxDeactivate)(eng->apiEngine);
    }
	return r;
}

//Deprecated and removed: 2025-07-16
/*void nixGetContext(STNix_Engine* engAbs, void* dest){
	//
}*/

//

void nixTick(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	//Delete buffers
#	ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
	{
		NixUI16 i; const NixUI16 use = eng->buffersArrUse;
		for(i=1; i < use; i++){
			STNix_bufferAL* buffer = &eng->buffersArr[i];
			if(buffer->regInUse && buffer->bufferDesc.state == ENNixBufferState_WaitingForDeletion){
				buffer->_msWaitingForDeletion += (1000 / 30);
				if(buffer->_msWaitingForDeletion >= NIX_MSWAIT_BEFORE_DELETING_BUFFERS){
					__nixBufferDestroy(buffer, i);
				}
			}
		}
	}
#	endif
	//
    if(eng->api.engine.tick != NULL){
        (*eng->api.engine.tick)(eng->apiEngine);
    }
	//STREAMS: notifify recently played & unattached buffers.
	{
		NixUI16 iSrc; const NixUI16 useSrc = eng->sourcesArrUse;
		for(iSrc=1; iSrc < useSrc; iSrc++){ //Source index zero is reserved
			STNix_source* source = &eng->sourcesArr[iSrc];
			if(source->regInUse && source->retainCount != 0){
				//if(source->sourceType == ENNixSourceType_Stream){ //Do not validate 'sourceType' because when all buffers are removed from source, it is restored to 'typeUndefined', (ex: when a 'tick' takes too long)
				const NixUI16 unqueuedCount = source->buffStreamUnqueuedCount; //Create and evaluate a copy variable, in case another thread modify the value
				if(unqueuedCount != 0){
					NIX_PRINTF_INFO("Queue, removing and invoquing callback for %d unqueued buffers.\n", unqueuedCount);
					//
                    __nixSrcQueueRemoveBuffersOldest(eng, source, iSrc, unqueuedCount);
					//Callback
					if(source->bufferUnqueuedCallback != NULL){ (*source->bufferUnqueuedCallback)(engAbs, source->bufferUnqueuedCallbackData, iSrc, unqueuedCount); }
					//
					NIX_ASSERT(unqueuedCount <= source->buffStreamUnqueuedCount)
					source->buffStreamUnqueuedCount -= unqueuedCount;
				}
				//}
			}
		}
	}
}

//------------------------------------------

NixUI16 nixSourceAssignStatic(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData){
	return __nixSourceAssign(engAbs, lookIntoReusable, audioGroupIndex, releaseCallBack, releaseCallBackUserData, 1/*queueSize*/, NULL, NULL);
}

NixUI16 nixSourceAssignStream(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData, const NixUI16 queueSize, PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback, void* bufferUnqueueCallbackData){
	return __nixSourceAssign(engAbs, lookIntoReusable, audioGroupIndex, releaseCallBack, releaseCallBackUserData, queueSize, bufferUnqueueCallback, bufferUnqueueCallbackData);
}

NixUI16 __nixSourceAssign(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData, const NixUI16 queueSize, PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback, void* bufferUnqueueCallbackData){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	//Search into reusable sources
	if(lookIntoReusable && eng->sourcesArr != NULL){
		NixUI16 i; const NixUI16 useCount = eng->sourcesArrUse;
		for(i=1; i < useCount; i++){ //Source index zero is reserved
			STNix_source* source = &eng->sourcesArr[i];
			if(source->regInUse && source->retainCount == 0 && source->isReusable){
				STNix_AudioGroup* grp		= &__nixSrcGroups[audioGroupIndex];
				NIX_ASSERT(source->sourceState == ENNixSourceState_Stopped)
				NIX_ASSERT(source->queueBuffIndexesUse == 0)
				if(source->releaseCallBack != NULL) (*source->releaseCallBack)(engAbs, source->releaseCallBackUserData, i);
				source->audioGroupIndex			= audioGroupIndex;
				source->releaseCallBack			= releaseCallBack;
				source->releaseCallBackUserData	= releaseCallBackUserData;
				source->bufferUnqueuedCallback		= bufferUnqueueCallback;
				source->bufferUnqueuedCallbackData	= bufferUnqueueCallbackData;
				__nixSourceRetain(source);
                if(source->apiSource.opq != NULL){
                    if(source->api.setVolume != NULL){
                        if(!(*source->api.setVolume)(source->apiSource, source->volume * (grp->enabled ? grp->volume : 0.0f))){
                            NIX_PRINTF_WARNING("source->api.setVolume failed for source reutilization.\n");
                        }
                    }
                }
				return i;
			}
		}
	}
	//Try to Generate new source
    if(eng->api.source.create != NULL){
        STNix_AudioGroup* grp = &__nixSrcGroups[audioGroupIndex];
        STNixApiSource src = (*eng->api.source.create)(eng->apiEngine);
        if(src.opq == NULL){
            NIX_PRINTF_ERROR("eng->api.source.create failed.\n");
        } else {
            NixUI16 newSourceIndex;
            STNix_source audioSource;
            __nixSourceInit(&audioSource);
            audioSource.apiSource                   = src;
            audioSource.api                         = eng->api.source;
            audioSource.audioGroupIndex             = audioGroupIndex;
            audioSource.releaseCallBack             = releaseCallBack;
            audioSource.releaseCallBackUserData     = releaseCallBackUserData;
            audioSource.bufferUnqueuedCallback      = bufferUnqueueCallback;
            audioSource.bufferUnqueuedCallbackData  = bufferUnqueueCallbackData;
            NIX_ASSERT(audioSource.regInUse)
            newSourceIndex = __nixSourceAdd(eng, &audioSource);
            if(newSourceIndex == 0){
                if(eng->api.source.destroy != NULL){
                    (*eng->api.source.destroy)(src);
                    src = (STNixApiSource)STNixApiSource_Zero;
                }
            } else {
                if(audioSource.api.setCallback != NULL){
                    (*audioSource.api.setCallback)(audioSource.apiSource, __nixSourceBufferQueueCallback, eng, newSourceIndex);
                }
                if(audioSource.api.setVolume != NULL){
                    if(!(*audioSource.api.setVolume)(audioSource.apiSource, audioSource.volume * (grp->enabled ? grp->volume : 0.0f))){
                        NIX_PRINTF_WARNING("nixAVAudioSource_setVolume failed for source creation.\n");
                    }
                }
                return newSourceIndex;
            }
        }
    }
	return 0;
}

NixUI32	nixSourceRetainCount(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		return source->retainCount;
	} NIX_GET_SOURCE_END
	return 0;
}

void nixSourceRetain(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		__nixSourceRetain(source);
	} NIX_GET_SOURCE_END
}

void nixSourceRelease(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source){
		__nixSourceRelease(engAbs, source, sourceIndex);
	} NIX_GET_SOURCE_END
}

void nixSourceSetRepeat(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixBOOL repeat){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source){
        if(source->api.setRepeat != NULL){
            (*source->api.setRepeat)(source->apiSource, repeat);
        }
	} NIX_GET_SOURCE_END
}

NixUI32 nixSourceGetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	NixUI32 r = 0;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		NixUI16 i; const NixUI16 use = source->queueBuffIndexesUse;
		for(i=0; i < use; i++){
			const NixUI16 iBuff = source->queueBuffIndexes[i];
			NIX_GET_BUFFER_START(eng, iBuff, buffer) {
				NIX_ASSERT(buffer->bufferDesc.audioDesc.blockAlign != 0)
				NIX_ASSERT(buffer->bufferDesc.audioDesc.samplerate != 0)
				if(buffer->bufferDesc.audioDesc.blockAlign != 0){
					r += (buffer->bufferDesc.dataBytesCount / buffer->bufferDesc.audioDesc.blockAlign);
				}
			} NIX_GET_BUFFER_END
		}
	} NIX_GET_SOURCE_END
	return r;
}

NixUI32 nixSourceGetBytes(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	NixUI32 r = 0;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		NixUI16 i; const NixUI16 use = source->queueBuffIndexesUse;
		for(i=0; i < use; i++){
			const NixUI16 iBuff = source->queueBuffIndexes[i];
			NIX_GET_BUFFER_START(eng, iBuff, buffer) {
				NIX_ASSERT(buffer->bufferDesc.audioDesc.blockAlign != 0)
				NIX_ASSERT(buffer->bufferDesc.audioDesc.samplerate != 0)
				if(buffer->bufferDesc.audioDesc.blockAlign != 0){
					r += buffer->bufferDesc.dataBytesCount;
				}
			} NIX_GET_BUFFER_END
		}
	} NIX_GET_SOURCE_END
	return r;
}

NixFLOAT nixSourceGetSeconds(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	NixFLOAT r = 0.0f;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		NixUI16 i; const NixUI16 use = source->queueBuffIndexesUse;
		for(i=0; i < use; i++){
			const NixUI16 iBuff = source->queueBuffIndexes[i];
			NIX_GET_BUFFER_START(eng, iBuff, buffer) {
				NIX_ASSERT(buffer->bufferDesc.audioDesc.blockAlign != 0)
				NIX_ASSERT(buffer->bufferDesc.audioDesc.samplerate != 0)
				if(buffer->regInUse && buffer->bufferDesc.audioDesc.blockAlign != 0 && buffer->bufferDesc.audioDesc.samplerate != 0){
					r += ((float)buffer->bufferDesc.dataBytesCount / (float)buffer->bufferDesc.audioDesc.blockAlign) / (float)buffer->bufferDesc.audioDesc.samplerate;
				}
			} NIX_GET_BUFFER_END
		}
	} NIX_GET_SOURCE_END
	return r;
}

NixFLOAT nixSourceGetVoume(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		return source->volume;
	} NIX_GET_SOURCE_END
	return 0.0f;
}

void nixSourceSetVolume(STNix_Engine* engAbs, const NixUI16 sourceIndex, const float volume){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		STNix_AudioGroup* grp = &__nixSrcGroups[source->audioGroupIndex];
		source->volume = volume;
        if(source->api.setVolume != NULL){
            (*source->api.setVolume)(source->apiSource, volume * (grp->enabled ? grp->volume : 0.0f));
        }
	} NIX_GET_SOURCE_END
}

NixUI16 nixSourceGetBuffersCount(STNix_Engine* engAbs, const NixUI16 sourceIndex){
    NixUI16 r = 0;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		r = source->queueBuffIndexesUse;
	} NIX_GET_SOURCE_END
	return r;
}

NixUI32 nixSourceGetOffsetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	NixUI32 r = 0;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        //ToDo: implement
#       if defined(NIX_DEFAULT_API_OPENAL)
		/*if(source->queueBuffIndexesUse != 0){
			ALint offset; alGetSourcei(source->idSourceAL, AL_SAMPLE_OFFSET, &offset); NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SAMPLE_OFFSET)");
			r = offset;
		}*/
#       endif
	} NIX_GET_SOURCE_END
	return r;
}

NixUI32 nixSourceGetOffsetBytes(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	NixUI32 r = 0;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        //ToDo: implement
#       if defined(NIX_DEFAULT_API_OPENAL)
		/*if(source->queueBuffIndexesUse != 0){
			ALint offset; alGetSourcei(source->idSourceAL, AL_BYTE_OFFSET, &offset); NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_BYTE_OFFSET)");
			r = offset;
		}*/
#       endif
	} NIX_GET_SOURCE_END
	return r;
}

void nixSourceSetOffsetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI32 offsetSamples){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        //ToDo: implement
#       if defined(NIX_DEFAULT_API_OPENAL)
		/*if(source->queueBuffIndexesUse != 0){
			ALint offset = offsetSamples; alSourcei(source->idSourceAL, AL_SAMPLE_OFFSET, offset); NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SAMPLE_OFFSET)");
		}*/
#       endif
	} NIX_GET_SOURCE_END
}

void nixSourcePlay(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        if(source->api.play != NULL){
            (*source->api.play)(source->apiSource);
        }
		source->sourceState = ENNixSourceState_Playing;
	} NIX_GET_SOURCE_END
}

NixBOOL nixSourceIsPlaying(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        if(source->api.isPlaying != NULL){
            return (*source->api.isPlaying)(source->apiSource) ? NIX_TRUE : NIX_FALSE;
        }
	} NIX_GET_SOURCE_END
	return NIX_FALSE;
}

void nixSourcePause(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        if(source->api.pause != NULL){
            (*source->api.pause)(source->apiSource);
        }
		source->sourceState = ENNixSourceState_Playing;
	} NIX_GET_SOURCE_END
}

void nixSourceStop(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        if(source->api.stop != NULL){
            (*source->api.stop)(source->apiSource);
        }
		source->sourceState = ENNixSourceState_Stopped;
	} NIX_GET_SOURCE_END
}

void nixSourceRewind(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
        //ToDo: imeplement
/*#     if defined(NIX_DEFAULT_API_OPENAL)
		alSourceRewind(source->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceRewind");
#       endif*/
	} NIX_GET_SOURCE_END
}

NixBOOL nixSourceSetBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 bufferIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_PRINTF_INFO("nixSourceSetBuffer::start source(%d) buffer(%d).\n", sourceIndex, bufferIndex);
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		__nixSrcQueueClear(eng, source, sourceIndex);
		if(bufferIndex == 0){
			//Set buffer to NONE (default if the above failed)
			return NIX_TRUE;
		} else {
			NIX_GET_BUFFER_START(eng, bufferIndex, buffer) {
                {
                    if(source->apiSource.opq == NULL){
                        NIX_PRINTF_ERROR("nixSourceSetBuffer::source->apiSource.opq is NULL.\n");
                    } else if(source->api.setBuffer == NULL){
                        NIX_PRINTF_ERROR("nixSourceSetBuffer::source->api.setBuffer is NULL.\n");
                    } else {
                        __nixSrcQueueSetUniqueBuffer(source, buffer, bufferIndex); NIX_PRINTF_INFO("Queued STATIC source(%d) buffer(%d) (%d in queue).\n", sourceIndex, bufferIndex, source->queueBuffIndexesUse);
                        if(!(*source->api.setBuffer)(source->apiSource, buffer->apiBuff)){
                            NIX_PRINTF_ERROR("nixSourceSetBuffer::source->api.setBuffer(buffer #%d) failed.\n", (source->queueBuffIndexesUse + 1));
                            __nixSrcQueueRemoveBuffersNewest(eng, source, sourceIndex, 1);
                        } else {
                            //Play if its playing
                            /*if(source->sourceState == ENNixSourceState_Playing){
                                if(!nixAVAudioSource_isPlaying(source->avfSource)){
                                    nixAVAudioSource_play(source->avfSource);
                                }
                            }*/
                            return NIX_TRUE;
                        }
                    }
                }
			} NIX_GET_BUFFER_END
		} //if(bufferIndex == 0){
	} NIX_GET_SOURCE_END
	return NIX_FALSE;
}

NixBOOL nixSourceStreamAppendBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 streamBufferIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    NIX_PRINTF_INFO("nixSourceStreamAppendBuffer::start source(%d) streamBufferIndex(%d).\n", sourceIndex, streamBufferIndex);
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		NIX_GET_BUFFER_START(eng, streamBufferIndex, buffer) {
			NIX_ASSERT(buffer->bufferDesc.state == ENNixBufferState_LoadedForPlay)
			if(buffer->bufferDesc.state == ENNixBufferState_LoadedForPlay){
                if(source->apiSource.opq == NULL){
                    NIX_PRINTF_ERROR("nixSourceStreamAppendBuffer::source->apiSource.opq is NULL.\n"); NIX_ASSERT(0)
                } else if(buffer->apiBuff.opq == NULL){
                    NIX_PRINTF_ERROR("nixSourceStreamAppendBuffer::buffer->apiBuff.opq is NULL.\n"); NIX_ASSERT(0)
                } else if(source->api.queueBuffer == NULL){
                    NIX_PRINTF_ERROR("nixSourceStreamAppendBuffer::source->api.queueBuffer is NULL.\n"); NIX_ASSERT(0)
                } else if(!(*source->api.queueBuffer)(source->apiSource, buffer->apiBuff)){
                    NIX_PRINTF_ERROR("nixSourceStreamAppendBuffer::source->api.queueBuffer failed.\n"); NIX_ASSERT(0)
                } else {
                    __nixSrcQueueAddStreamBuffer(source, buffer, streamBufferIndex); NIX_PRINTF_INFO("Queued STREAM source(%d) buffer(%d) (%d in queue).\n", sourceIndex, streamBufferIndex, source->queueBuffIndexesUse);
                    /*if(source->sourceState == ENNixSourceState_Playing){
                        //Play if its playing
                        if(!nixAVAudioSource_isPlaying(source->avfSource)){
                            nixAVAudioSource_play(source->avfSource);
                        }
                    }*/
                    return NIX_TRUE;
                }
			}
		} NIX_GET_BUFFER_END
	} NIX_GET_SOURCE_END
	return NIX_FALSE;
}

NixBOOL nixSourceEmptyQueue(STNix_Engine* engAbs, const NixUI16 sourceIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		__nixSrcQueueClear(eng, source, sourceIndex);
		return NIX_TRUE;
	} NIX_GET_SOURCE_END
	return NIX_FALSE;
}

void nixDbgPrintSourcesStatus(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	//Sources status
	/*{
		int i, found = 0; const int count = eng->sourcesArrUse;
		NIX_PRINTF_ALLWAYS("--- NIXTLA SOURCES STATUS.\n");
		for(i = 0; i < count; i++){
			const STNix_source* src = &eng->sourcesArr[i];
			if(src->regInUse){
#			ifdef NIX_DEFAULT_API_OPENAL
				ALint sourceState;
				alGetSourcei(src->idSourceAL, AL_SOURCE_STATE, &sourceState);	NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SOURCE_STATE)");
				switch (sourceState) {
					case AL_INITIAL:
						NIX_PRINTF_ALLWAYS("Src #%d (al %d), state 'INITIAL' (%d retentions).\n", (i + 1), src->idSourceAL, src->retainCount);
						break;
					case AL_PLAYING:
						NIX_PRINTF_ALLWAYS("Src #%d (al %d), state 'PLAYING' (%d retentions).\n", (i + 1), src->idSourceAL, src->retainCount);
						break;
					case AL_PAUSED:
						NIX_PRINTF_ALLWAYS("Src #%d (al %d), state 'PAUSED' (%d retentions).\n", (i + 1), src->idSourceAL, src->retainCount);
						break;
					case AL_STOPPED:
						NIX_PRINTF_ALLWAYS("Src #%d (al %d), state 'STOPPED' (%d retentions).\n", (i + 1), src->idSourceAL, src->retainCount);
						break;
					default:
						NIX_PRINTF_ALLWAYS("Src #%d (al %d), state UNKNOWN (%d retentions).\n", (i + 1), src->idSourceAL, src->retainCount);
						break;
				}
				//Source buffers
				{
					int i; const int count = src->queueBuffIndexesUse;
					for(i = 0; i < count; i++){
						const NixUI16 buffIdx = src->queueBuffIndexes[i];
						NIX_ASSERT(buffIdx < eng->buffersArrUse)
						const STNix_bufferAL* buff = &eng->buffersArr[buffIdx];
						NIX_ASSERT(buff->regInUse)
						const STNix_bufferDesc* buffDesc = &buff->bufferDesc;
						switch (buffDesc->state) {
							case ENNixBufferState_Free:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state FREE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
							case ENNixBufferState_LoadedForPlay:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state LOADED_FOR_PLAY (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
							case ENNixBufferState_AttachedForPlay:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state ATACHED_FOR_PLAY (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
							case ENNixBufferState_AttachedForCapture:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state ATACHED_FOR_CAPTURE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
							case ENNixBufferState_LoadedWithCapture:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state LOADED_WITH_CAPTURE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
#						ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
							case ENNixBufferState_WaitingForDeletion:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state WAITING_FOR_DELETION (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
#						endif
							default:
								NIX_PRINTF_ALLWAYS("   + Buff #%d (al %d), state UNKNOWN (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
								break;
						}
					}
				}
#			endif
				found++;
			}
		}
		NIX_PRINTF_ALLWAYS("--- %d NIXTLA SOURCES FOUND.\n", found);
	}
	//Buffers status
	{
		int i, found = 0; const int count = eng->buffersArrUse;
		NIX_PRINTF_ALLWAYS("--- NIXTLA BUFFERS STATUS.\n");
		for(i = 0; i < count; i++){
			const STNix_bufferAL* buff = &eng->buffersArr[i];
			if(buff->regInUse){
#			ifdef NIX_DEFAULT_API_OPENAL
				const STNix_bufferDesc* buffDesc = &buff->bufferDesc;
				switch (buffDesc->state) {
					case ENNixBufferState_Free:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state FREE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
					case ENNixBufferState_LoadedForPlay:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state LOADED_FOR_PLAY (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
					case ENNixBufferState_AttachedForPlay:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state ATACHED_FOR_PLAY (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
					case ENNixBufferState_AttachedForCapture:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state ATACHED_FOR_CAPTURE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
					case ENNixBufferState_LoadedWithCapture:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state LOADED_WITH_CAPTURE (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
#						ifdef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
					case ENNixBufferState_WaitingForDeletion:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state WAITING_FOR_DELETION (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
#						endif
					default:
						NIX_PRINTF_ALLWAYS("Buff #%d (al %d), state UNKNOWN (%d retentions).\n", (i + 1), buff->idBufferAL, buff->retainCount);
						break;
				}
#			endif
				found++;
			}
		}
		NIX_PRINTF_ALLWAYS("--- %d NIXTLA BUFFERS FOUND.\n", found);
	}*/
	//CAPTURE
	/*{
		NIX_PRINTF_ALLWAYS("--- CAPTURE .\n");
		NIX_PRINTF_ALLWAYS("   In progress: %d.\n", eng->captureInProgess);
		NIX_PRINTF_ALLWAYS("   Filled buffers: %d.\n", eng->buffersCaptureArrFilledCount);
		NIX_PRINTF_ALLWAYS("--- CAPTURE .\n");
	}*/
}

NixBOOL nixSourceHaveBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 bufferIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_ASSERT(bufferIndex > 0)
	NIX_ASSERT(bufferIndex < eng->buffersArrUse)
	NIX_GET_SOURCE_START(eng, sourceIndex, source) {
		NixUI16 i; const NixUI16 use = source->queueBuffIndexesUse;
		for(i=0; i < use; i++){
			NIX_ASSERT(source->queueBuffIndexes[i] > 0)
			NIX_ASSERT(source->queueBuffIndexes[i] < eng->buffersArrUse)
			if(source->queueBuffIndexes[i] == bufferIndex) return NIX_TRUE;
		}
	} NIX_GET_SOURCE_END
	return NIX_FALSE;
}

//--------------------
//--  Audio groups  --
//--------------------

NixBOOL	nixSrcGroupIsEnabled(STNix_Engine* engAbs, const NixUI8 groupIndex){
	NIX_ASSERT(groupIndex < NIX_AUDIO_GROUPS_SIZE)
	if(groupIndex < NIX_AUDIO_GROUPS_SIZE){
		return __nixSrcGroups[groupIndex].enabled;
	}
	return NIX_FALSE;
}

NixFLOAT nixSrcGroupGetVolume(STNix_Engine* engAbs, const NixUI8 groupIndex){
	NIX_ASSERT(groupIndex < NIX_AUDIO_GROUPS_SIZE)
	if(groupIndex < NIX_AUDIO_GROUPS_SIZE){
		return __nixSrcGroups[groupIndex].volume;
	}
	return 0.0f;
}

void nixSrcGroupSetEnabled(STNix_Engine* engAbs, const NixUI8 groupIndex, const NixBOOL enabled){
	NIX_ASSERT(groupIndex < NIX_AUDIO_GROUPS_SIZE)
	if(groupIndex < NIX_AUDIO_GROUPS_SIZE){
		STNix_AudioGroup* grp		= &__nixSrcGroups[groupIndex];
		STNix_EngineObjetcs* eng	= (STNix_EngineObjetcs*)engAbs->o;
		NixUI16 i; const NixUI16 use = eng->sourcesArrUse;
		for(i=0; i < use; i++){
			STNix_source* source = &eng->sourcesArr[i];
            if(source->regInUse && source->audioGroupIndex == groupIndex){
                if(source->apiSource.opq != NULL && source->api.setVolume != NULL){
                    if(!(*source->api.setVolume)(source->apiSource, source->volume * (enabled ? grp->volume : 0.0f))){
                        //error
                    }
                }
			}
		}
		grp->enabled = enabled;
	}
}

void nixSrcGroupSetVolume(STNix_Engine* engAbs, const NixUI8 groupIndex, const NixFLOAT volume){
	NIX_ASSERT(groupIndex < NIX_AUDIO_GROUPS_SIZE)
	if(groupIndex < NIX_AUDIO_GROUPS_SIZE){
		STNix_AudioGroup* grp		= &__nixSrcGroups[groupIndex];
		if(grp->enabled){
			STNix_EngineObjetcs* eng	= (STNix_EngineObjetcs*)engAbs->o;
			NixUI16 i; const NixUI16 use = eng->sourcesArrUse;
			for(i=0; i < use; i++){
				STNix_source* source = &eng->sourcesArr[i];
				if(source->regInUse && source->audioGroupIndex == groupIndex){
                    if(source->apiSource.opq != NULL && source->api.setVolume != NULL){
                        if(!(*source->api.setVolume)(source->apiSource, source->volume * (grp->enabled ? grp->volume : 0.0f))){
                            //error
                        }
                    }
				}
			}
		}
		grp->volume = volume;
	}
}

void __nixSourceBufferQueueCallback(void* pEng, NixUI32 sourceIndex, const NixUI32 ammBuffs){ //AVFAUDIO specific
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)pEng; NIX_ASSERT(eng != NULL)
    NIX_GET_SOURCE_START(eng, sourceIndex, source){
        NIX_ASSERT((source->buffStreamUnqueuedCount + ammBuffs) <= source->queueBuffIndexesUse)
        if((source->buffStreamUnqueuedCount + ammBuffs) <= source->queueBuffIndexesUse){
            switch (source->sourceType) {
                case ENNixSourceType_Stream:
                    //STREAM, release oldest buffer
                    source->buffStreamUnqueuedCount += ammBuffs; //This number will be processed at 'nixTick'
                    break;
                case ENNixSourceType_Static:
                    //STATIC, source stopped
                    NIX_ASSERT(source->queueBuffIndexesUse == ammBuffs)
                    if(source->queueBuffIndexesUse > 0){
                        __nixSrcQueueRemoveBuffersNewest(eng, source, sourceIndex, source->queueBuffIndexesUse);
                    }
                    source->sourceState = ENNixSourceState_Stopped;
                    break;
                default:
                    NIX_ASSERT(NIX_FALSE)
                    break;
            }
        }
    } NIX_GET_SOURCE_END
}

//------------------------------------------

NixUI16 nixBufferWithData(STNix_Engine* engAbs, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NixUI16 i; const NixUI16 useCount = eng->buffersArrUse;
	NIX_ASSERT(audioDesc->blockAlign == ((audioDesc->bitsPerSample / 8) * audioDesc->channels))
	//Look for a available existent buffer
	{
		for(i=1; i < useCount; i++){ //Buffer index zero is reserved
			STNix_bufferAL* buffer = &eng->buffersArr[i];
			if(buffer->regInUse && buffer->bufferDesc.state == ENNixBufferState_Free){
				if(__nixBufferSetData(eng, buffer, audioDesc, audioDataPCM, audioDataPCMBytes)){
					__nixBufferRetain(buffer);
					return i;
				}
			}
		}
	}
	//Create a new buffer
	{
		const NixUI16 iBuff = __nixBufferCreate(eng);
		if(iBuff != 0){
			if(__nixBufferSetData(eng, &eng->buffersArr[iBuff], audioDesc, audioDataPCM, audioDataPCMBytes)){
				return iBuff;
			} else {
				__nixBufferRelease(&eng->buffersArr[iBuff], iBuff);
			}
		}
	}
	return 0;
}

NixBOOL nixBufferSetData(STNix_Engine* engAbs, const NixUI16 buffIndex, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_BUFFER_START(eng, buffIndex, buffer) {
		return __nixBufferSetData(eng, buffer, audioDesc, audioDataPCM, audioDataPCMBytes);
	} NIX_GET_BUFFER_END
	return NIX_FALSE;
}

NixBOOL nixBufferFillZeroes(STNix_Engine* engAbs, const NixUI16 buffIndex){ //the unused range of the bufer will be zeroed
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    NIX_GET_BUFFER_START(eng, buffIndex, buffer) {
        return __nixBufferFillZeroes(eng, buffer);
    } NIX_GET_BUFFER_END
    return NIX_FALSE;
}

NixUI32	nixBufferRetainCount(STNix_Engine* engAbs, const NixUI16 buffIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_BUFFER_START(eng, buffIndex, buffer) {
		return buffer->retainCount;
	} NIX_GET_BUFFER_END
	return 0;
}

void nixBufferRetain(STNix_Engine* engAbs, const NixUI16 buffIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_BUFFER_START(eng, buffIndex, buffer) {
		__nixBufferRetain(buffer);
	} NIX_GET_BUFFER_END
}

void nixBufferRelease(STNix_Engine* engAbs, const NixUI16 buffIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_GET_BUFFER_START(eng, buffIndex, buffer) {
		__nixBufferRelease(buffer, buffIndex);
	} NIX_GET_BUFFER_END
}

float nixBufferSeconds(STNix_Engine* engAbs, const NixUI16 buffIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	float r = 0.0f;
	NIX_ASSERT(buffIndex != 0)
	if(buffIndex != 0){
		NIX_ASSERT(buffIndex < eng->buffersArrUse)
		if(buffIndex < eng->buffersArrUse){
			NIX_ASSERT(eng->buffersArr[buffIndex].regInUse)
			if(eng->buffersArr[buffIndex].regInUse){
				STNix_bufferDesc* bufferDesc = &eng->buffersArr[buffIndex].bufferDesc;
				STNix_audioDesc* audioDesc = &bufferDesc->audioDesc;
				if(bufferDesc->dataBytesCount != 0 && audioDesc->bitsPerSample != 0 && audioDesc->channels != 0 && audioDesc->samplerate != 0){
					r = (float)bufferDesc->dataBytesCount / (float)(audioDesc->samplerate * audioDesc->channels * audioDesc->bitsPerSample / 8);
				}
			}
		}
	}
	return r;
}

STNix_audioDesc nixBufferAudioDesc(STNix_Engine* engAbs, const NixUI16 buffIndex){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	STNix_audioDesc r;
	r.channels = 0;
	r.bitsPerSample = 0;
	r.samplerate = 0;
	r.samplerate = 0;
	NIX_ASSERT(buffIndex != 0)
	if(buffIndex != 0){
		NIX_ASSERT(buffIndex < eng->buffersArrUse)
		if(buffIndex < eng->buffersArrUse){
			NIX_ASSERT(eng->buffersArr[buffIndex].regInUse)
			if(eng->buffersArr[buffIndex].regInUse){
				r = eng->buffersArr[buffIndex].bufferDesc.audioDesc;
			}
		}
	}
	return r;
}

void nixCaptureBufferFilledCallback_(STNixApiEngine apiEng, STNixApiRecorder rec, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples, void* userdata){
    STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)userdata;
    STNix_Engine enAbs = { eng };
    if(eng->buffersCaptureCallback != NULL){
        (*eng->buffersCaptureCallback)(&enAbs, eng->buffersCaptureCallbackUserData, audioDesc, audioData, audioDataBytes, audioDataSamples);
    }
}

NixBOOL nixCaptureInit(STNix_Engine* engAbs, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer, PTRNIX_CaptureBufferFilledCallback bufferCaptureCallback, void* bufferCaptureCallbackUserData){
    NixBOOL r = NIX_FALSE;
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	NIX_PRINTF_INFO("CaptureInit with channels(%d) bitsPerSample(%d) samplerate(%d) blockAlign(%d).\n", audioDesc->channels, audioDesc->bitsPerSample, audioDesc->samplerate, audioDesc->blockAlign);
    if(eng->apiEngine.opq != NULL && eng->apiRecorder.opq == NULL && eng->api.recorder.create != NULL && buffersCount != 0 && samplesPerBuffer != 0){
        STNixApiRecorder recorder = (*eng->api.recorder.create)(eng->apiEngine, audioDesc, buffersCount, samplesPerBuffer);
        if(recorder.opq != NULL){
            //set callback
            if(eng->api.recorder.setCallback != NULL){
                if(!(*eng->api.recorder.setCallback)(recorder, nixCaptureBufferFilledCallback_, eng)){
                    NIX_PRINTF_ERROR("eng->api.recorder.setCallback failed.\n");
                } else {
                    eng->apiRecorder = recorder; recorder = (STNixApiRecorder)STNixApiRecorder_Zero; //consume
                    eng->buffersCaptureCallback = bufferCaptureCallback;
                    eng->buffersCaptureCallbackUserData = bufferCaptureCallbackUserData;
                    r = NIX_TRUE;
                }
            }
        }
        //release (if not consumed)
        if(recorder.opq != NULL){
            if(eng->api.recorder.destroy != NULL){
                (*eng->api.recorder.destroy)(recorder);
            }
            recorder = (STNixApiRecorder)STNixApiRecorder_Zero;
        }
    }
#   if defined(NIX_DEFAULT_API_OPENAL)
	/*if(eng->deviceCaptureAL == NULL && buffersCount != 0 && samplesPerBuffer != 0){
		const ALenum dataFormat = NIX_OPENAL_AUDIO_FORMAT(audioDesc->channels, audioDesc->bitsPerSample);
		if(dataFormat != 0 && audioDesc->samplesFormat == ENNix_sampleFormat_int){
			eng->captureMainBufferBytesCount	= (audioDesc->bitsPerSample / 8) * audioDesc->channels * samplesPerBuffer * 2;
			eng->deviceCaptureAL		= alcCaptureOpenDevice(NULL/*nomDispositivo* /, audioDesc->samplerate, dataFormat, eng->captureMainBufferBytesCount);
			NIX_OPENAL_ERR_VERIFY("alcCaptureOpenDevice")
			if(eng->deviceCaptureAL == NULL){
				NIX_PRINTF_ERROR("alcCaptureOpenDevice failed\n");
				eng->captureMainBufferBytesCount = 0;
			} else {
				NixUI16 i; NixUI32 bytesPerBuffer;
				eng->captureFormat				= *audioDesc;
				eng->captureInProgess			= NIX_FALSE;
				eng->captureSamplesPerBuffer	= samplesPerBuffer;
				//Free last capture buffers
				if(eng->buffersCaptureArr != NULL){
					NixUI16 i; const NixUI16 useCount = eng->buffersCaptureArrSize;
					for(i=0; i < useCount; i++){ NIX_FREE(eng->buffersCaptureArr[i].dataPointer); }
					NIX_FREE(eng->buffersCaptureArr);
					eng->buffersCaptureArr = NULL;
					eng->buffersCaptureArrSize = 0;
				}
				//Create capture buffers
				bytesPerBuffer                  = (audioDesc->bitsPerSample / 8) * audioDesc->channels * samplesPerBuffer;
				eng->buffersCaptureArrFirst     = 0;
				eng->buffersCaptureArrFilledCount = 0;
				eng->buffersCaptureArrSize      = buffersCount;
                NIX_MALLOC(eng->buffersCaptureArr, STNix_bufferDesc, sizeof(STNix_bufferDesc) * buffersCount, "buffersCaptureArr");
				eng->buffersCaptureCallback	= bufferCaptureCallback;
				eng->buffersCaptureCallbackUserData = bufferCaptureCallbackUserData;
				for(i=0; i < buffersCount; i++){
					STNix_bufferDesc* buff      = &eng->buffersCaptureArr[i];
					buff->dataBytesCount		= bytesPerBuffer;
                    NIX_MALLOC(buff->dataPointer, NixUI8, sizeof(NixUI8) * bytesPerBuffer, "buffCap.dataPointer");
					buff->state		            = ENNixBufferState_Free;
					buff->audioDesc	            = *audioDesc;
				}
				return NIX_TRUE;
			}
		}
	}*/
#   endif
	return r;
}

void nixCaptureFinalize(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
	//Destroy capture device
    if(eng->apiRecorder.opq != NULL){
        if(eng->api.recorder.stop != NULL){
            (*eng->api.recorder.stop)(eng->apiRecorder);
        }
        if(eng->api.recorder.destroy != NULL){
            (*eng->api.recorder.destroy)(eng->apiRecorder);
        }
        eng->apiRecorder = (STNixApiRecorder)STNixApiRecorder_Zero;
    }
#   if defined(NIX_DEFAULT_API_OPENAL)
	/*if(eng->deviceCaptureAL != NULL){
		alcCaptureStop(eng->deviceCaptureAL); NIX_OPENAL_ERR_VERIFY("alcCaptureStop");
		if(alcCaptureCloseDevice(eng->deviceCaptureAL) == AL_FALSE){
			NIX_PRINTF_ERROR("alcCaptureCloseDevice failed\n");
		}
		eng->captureMainBufferBytesCount = 0;
		eng->deviceCaptureAL = NULL;
	}*/
#   endif
	//Destroy capture buffers
	if(eng->buffersCaptureArr != NULL){
		NixUI16 i; const NixUI16 useCount = eng->buffersCaptureArrSize;
		for(i=0; i < useCount; i++){
			NIX_FREE(eng->buffersCaptureArr[i].dataPointer);
		}
		NIX_FREE(eng->buffersCaptureArr);
		eng->buffersCaptureArr				= NULL;
		eng->buffersCaptureArrSize			= 0;
		eng->buffersCaptureArrFilledCount	= 0;
	}
}

NixBOOL nixCaptureIsOnProgress(STNix_Engine* engAbs){
	return ((STNix_EngineObjetcs*)engAbs->o)->captureInProgess;
}

NixBOOL nixCaptureStart(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiRecorder.opq != NULL){
        if(eng->api.recorder.start != NULL){
            if(!(*eng->api.recorder.start)(eng->apiRecorder)){
                NIX_PRINTF_ERROR("Could not start audio capture,eng->api.recorder.start failed.\n");
            } else {
                eng->captureInProgess = NIX_TRUE;
                return NIX_TRUE;
            }
        }
    }
#   if defined(NIX_DEFAULT_API_OPENAL)
	/*if(eng->deviceCaptureAL != NULL / *&& !eng->captureInProgess* /){
		ALenum error;
		alcCaptureStart(eng->deviceCaptureAL);
		error = alGetError();
		if(error != AL_NO_ERROR){
			NIX_PRINTF_ERROR("Could not start audio capture error#(%d)\n", (NixSI32)error);
		} else {
			eng->captureInProgess = NIX_TRUE;
			return NIX_TRUE;
		}
	}*/
#   endif
	return NIX_FALSE;
}

void nixCaptureStop(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(eng->apiRecorder.opq != NULL){
        if(eng->api.recorder.stop != NULL){
            if(!(*eng->api.recorder.stop)(eng->apiRecorder)){
                NIX_PRINTF_ERROR("Could not stop audio capture,eng->api.recorder.stop failed.\n");
            } else {
                eng->captureInProgess = NIX_FALSE;
            }
        }
    }
#   if defined(NIX_DEFAULT_API_OPENAL)
	/*if(eng->deviceCaptureAL != NULL){
		alcCaptureStop(eng->deviceCaptureAL);
		eng->captureInProgess = NIX_FALSE;
	}*/
#   endif
}

NixUI32 nixCaptureFilledBuffersCount(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    //ToDo: implement.
	return 0;
}

NixUI32 nixCaptureFilledBuffersSamples(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    //ToDo: implement.
	return 0;
}

float nixCaptureFilledBuffersSeconds(STNix_Engine* engAbs){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    //ToDo: implement.
	return 0.0f;
}

void nixCaptureFilledBuffersRelease(STNix_Engine* engAbs, NixUI32 quantBuffersToRelease){
	STNix_EngineObjetcs* eng = (STNix_EngineObjetcs*)engAbs->o;
    if(quantBuffersToRelease > eng->buffersCaptureArrFilledCount){
        quantBuffersToRelease = eng->buffersCaptureArrFilledCount;
    }
	eng->buffersCaptureArrFirst += quantBuffersToRelease;
    if(eng->buffersCaptureArrFirst >= eng->buffersCaptureArrSize){
        eng->buffersCaptureArrFirst -= eng->buffersCaptureArrSize;
    }
}

#define NIX_FMT_CONVERTER_FREQ_PRECISION    512 //fixed point-denominator
#define NIX_FMT_CONVERTER_CHANNELS_MAX      2

//PCMFormat converter

typedef struct STNix_FmtConverterChannel_ {
    void*       ptr;
    NixUI32     sampleAlign; //jumps to get the next sample
} STNix_FmtConverterChannel;

typedef struct STNix_FmtConverterSide_ {
    STNix_audioDesc             desc;
    STNix_FmtConverterChannel   channels[NIX_FMT_CONVERTER_CHANNELS_MAX];
} STNix_FmtConverterSide;

typedef struct STNix_FmtConverter_ {
    STNix_FmtConverterSide src;
    STNix_FmtConverterSide dst;
    //accum
    struct {
        NixUI32 fixed;
        NixUI32 count;
        //accumulated values
        union {
            NixFLOAT accumFloat[NIX_FMT_CONVERTER_CHANNELS_MAX];
            NixSI64  accumSI64[NIX_FMT_CONVERTER_CHANNELS_MAX];
            NixSI32  accumSI32[NIX_FMT_CONVERTER_CHANNELS_MAX];
        };
    } samplesAccum;
} STNix_FmtConverter;

void* nixFmtConverter_create(void){
    STNix_FmtConverter* r = NULL;
    NIX_MALLOC(r, STNix_FmtConverter, sizeof(STNix_FmtConverter), "STNix_FmtConverter");
    memset(r, 0, sizeof(STNix_FmtConverter));
    return r;
}

void nixFmtConverter_destroy(void* pObj){
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL){
        NIX_FREE(obj);
        obj = NULL;
    }
}

NixBOOL nixFmtConverter_prepare(void* pObj, const STNix_audioDesc* srcDesc, const STNix_audioDesc* dstDesc){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL && srcDesc != NULL && dstDesc != NULL && srcDesc->channels > 0 && srcDesc->channels <= NIX_FMT_CONVERTER_CHANNELS_MAX && dstDesc->channels > 0 && dstDesc->channels <= NIX_FMT_CONVERTER_CHANNELS_MAX){
        if(
           //src
           (srcDesc->samplesFormat == ENNix_sampleFormat_float && srcDesc->bitsPerSample == 32)
           || (srcDesc->samplesFormat == ENNix_sampleFormat_int && (srcDesc->bitsPerSample == 8 || srcDesc->bitsPerSample == 16 || srcDesc->bitsPerSample == 32))
           //dst
           || (dstDesc->samplesFormat == ENNix_sampleFormat_float && dstDesc->bitsPerSample == 32)
           || (dstDesc->samplesFormat == ENNix_sampleFormat_int && (dstDesc->bitsPerSample == 8 || dstDesc->bitsPerSample == 16 || dstDesc->bitsPerSample == 32))
           )
        {
            memset(&obj->src, 0, sizeof(obj->src));
            memset(&obj->dst, 0, sizeof(obj->dst));
            obj->src.desc   = *srcDesc;
            obj->dst.desc   = *dstDesc;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL nixFmtConverter_setPtrAtSrcChannel(void* pObj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL && ptr != NULL && sampleAlign > 0 && iChannel < obj->src.desc.channels && iChannel < (sizeof(obj->src.channels) / sizeof(obj->src.channels[0]))){
        STNix_FmtConverterChannel* ch = &obj->src.channels[iChannel];
        ch->ptr = ptr;
        ch->sampleAlign = sampleAlign;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixFmtConverter_setPtrAtDstChannel(void* pObj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL && ptr != NULL && sampleAlign > 0 && iChannel < obj->dst.desc.channels && iChannel < (sizeof(obj->dst.channels) / sizeof(obj->dst.channels[0]))){
        STNix_FmtConverterChannel* ch = &obj->dst.channels[iChannel];
        ch->ptr = ptr;
        ch->sampleAlign = sampleAlign;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixFmtConverter_setPtrAtSrcInterlaced(void* pObj, const STNix_audioDesc* desc, void* ptr, const NixUI32 iFirstSample){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL && desc != NULL){
        r = NIX_TRUE;
        NixUI32 iCh, chCountDst = desc->channels;
        for(iCh = 0; iCh < chCountDst && iCh < NIX_FMT_CONVERTER_CHANNELS_MAX; ++iCh){
            const NixUI32 samplePos = (iFirstSample * desc->blockAlign);
            const NixUI32 sampleChannelPos = samplePos + ((desc->bitsPerSample / 8) * iCh);
            if(!nixFmtConverter_setPtrAtSrcChannel(pObj, iCh, &(((NixBYTE*)ptr)[sampleChannelPos]), desc->blockAlign)){
                NIX_PRINTF_ERROR("nixFmtConverter_setPtrAtSrcInterlaced::nixFmtConverter_setPtrAtSrcChannel(iCh:%d, blockAlign:%d, ptr:%s) failed.\n", iCh, desc->blockAlign, (ptr != NULL ? "not-null" : "NULL"));
                r = NIX_FALSE;
                break;
            }
        }
    }
    return r;
}

NixBOOL nixFmtConverter_setPtrAtDstInterlaced(void* pObj, const STNix_audioDesc* desc, void* ptr, const NixUI32 iFirstSample){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL && desc != NULL){
        r = NIX_TRUE;
        NixUI32 iCh, chCountDst = desc->channels;
        for(iCh = 0; iCh < chCountDst && iCh < NIX_FMT_CONVERTER_CHANNELS_MAX; ++iCh){
            const NixUI32 samplePos = (iFirstSample * desc->blockAlign);
            const NixUI32 sampleChannelPos = samplePos + ((desc->bitsPerSample / 8) * iCh);
            if(!nixFmtConverter_setPtrAtDstChannel(pObj, iCh, &(((NixBYTE*)ptr)[sampleChannelPos]), desc->blockAlign)){
                r = NIX_FALSE;
                break;
            }
        }
    }
    return r;
}

NixBOOL nixFmtConverter_convertSameFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);
NixBOOL nixFmtConverter_convertIncFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);
NixBOOL nixFmtConverter_convertDecFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);

NixBOOL nixFmtConverter_convert(void* pObj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    STNix_FmtConverter* obj = (STNix_FmtConverter*)pObj;
    if(obj != NULL){
        if(obj->src.desc.samplerate == obj->dst.desc.samplerate){
            //same freq
            r = nixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        } else if(obj->src.desc.samplerate < obj->dst.desc.samplerate){
            //increasing freq
            r = nixFmtConverter_convertIncFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        } else {
            //decreasing freq
            r = nixFmtConverter_convertDecFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        }
    }
    return r;
}

#define FMT_CONVERTER_IS_FLOAT32(DESC)  ((DESC).samplesFormat == ENNix_sampleFormat_float && (DESC).bitsPerSample == 32)
#define FMT_CONVERTER_IS_SI32(DESC)     ((DESC).samplesFormat == ENNix_sampleFormat_int && (DESC).bitsPerSample == 32)
#define FMT_CONVERTER_IS_SI16(DESC)     ((DESC).samplesFormat == ENNix_sampleFormat_int && (DESC).bitsPerSample == 16)
#define FMT_CONVERTER_IS_UI8(DESC)      ((DESC).samplesFormat == ENNix_sampleFormat_int && (DESC).bitsPerSample == 8)

#define FMT_CONVERTER_SAME_FREQ_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        dst0 += dstAlign0; \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_SAME_FREQ_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        *(LEFT_TYPE*)dst1 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        dst0 += dstAlign0; \
        dst1 += dstAlign1; \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)dst1 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        dst0 += dstAlign0; \
        dst1 += dstAlign1; \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, DIV_2_WITH_SUFIX) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2) + (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2)) / DIV_2_WITH_SUFIX); \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        dst0 += dstAlign0; \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

NixBOOL nixFmtConverter_convertSameFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    NixUI32 i = 0;
    if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 1){
        //same 1-channel
        if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_CH(NixFLOAT, NixFLOAT, , ,); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
        } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixSI32, NixSI32, , , ); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_CH(NixSI16, NixFLOAT, , , * 32767.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixSI16, NixSI16, , , ); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_CH(NixUI8, NixUI8, , , ); }
        }
    } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 2){
        //same 2-channels
        if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_CH(NixFLOAT, NixFLOAT, , ,); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
        } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixSI32, NixSI32, , , ); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_CH(NixSI16, NixFLOAT, , , * 32767.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixSI16, NixSI16, , , ); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_CH(NixUI8, NixUI8, , , ); }
        }
    } else if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 2){
        //duplicate src channels
        if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixFLOAT, NixFLOAT, , ,); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
        } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI32, NixSI32, , , ); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI16, NixFLOAT, , , * 32767.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI16, NixSI16, , , ); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
        } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_1_TO_2_CH(NixUI8, NixUI8, , , ); }
        }
    } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 1){
        //merge src channels
        if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixFLOAT, NixFLOAT, , , , 2.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., 2); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, 2); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, 2); }
        } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., 2.); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI32, NixSI32, , , , 2); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), 2); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), 2); }
        } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI16, NixFLOAT, , , * 32767.f, 2.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), 2); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI16, NixSI16, (NixSI32), , , 2); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), 2); }
        } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
            if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, 2.f); }
            else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, 2); }
            else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, 2); }
            else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_SAME_FREQ_2_TO_1_CH(NixUI8, NixUI8, , , , 2); }
        }
    }
    return r;
}

#define FMT_CONVERTER_INC_FREQ_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        dst0 += dstAlign0; \
        obj->samplesAccum.fixed += repeatPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)(dst0 - dstAlign0); \
            dst0 += dstAlign0; \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_INC_FREQ_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        *(LEFT_TYPE*)dst1 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        dst0 += dstAlign0; \
        dst1 += dstAlign1; \
        obj->samplesAccum.fixed += repeatPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)(dst0 - dstAlign0); \
            *(LEFT_TYPE*)dst1 = *(LEFT_TYPE*)(dst1 - dstAlign1); \
            dst0 += dstAlign0; \
            dst1 += dstAlign1; \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_INC_FREQ_1_TO_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)dst1 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        dst0 += dstAlign0; \
        dst1 += dstAlign1; \
        obj->samplesAccum.fixed += repeatPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)dst1 = *(LEFT_TYPE*)(dst0 - dstAlign0); \
            dst0 += dstAlign0; \
            dst1 += dstAlign1; \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_INC_FREQ_2_TO_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, DIV_2_WITH_SUFIX) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        *(LEFT_TYPE*)dst0 = (LEFT_TYPE) (((((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2) + (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2)) / DIV_2_WITH_SUFIX); \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        dst0 += dstAlign0; \
        obj->samplesAccum.fixed += repeatPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)(dst0 - dstAlign0); \
            dst0 += dstAlign0; \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

NixBOOL nixFmtConverter_convertIncFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    //increasing freq
    const NixUI32 delta = (obj->dst.desc.samplerate - obj->src.desc.samplerate);
    const NixUI32 repeatPerOrgSample = delta * NIX_FMT_CONVERTER_FREQ_PRECISION / obj->src.desc.samplerate;
    if(repeatPerOrgSample == 0){
        //freq difference is below 'NIX_FMT_CONVERTER_FREQ_PRECISION', just ignore
        r = nixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
    } else {
        NixUI32 i = 0;
        if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 1){
            //same 1-channel
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_CH(NixFLOAT, NixFLOAT, , ,); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixSI32, NixSI32, , , ); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_CH(NixSI16, NixFLOAT, , , * 32767.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixSI16, NixSI16, , , ); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_CH(NixUI8, NixUI8, , , ); }
            }
        } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 2){
            //same 2-channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_CH(NixFLOAT, NixFLOAT, , ,); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixSI32, NixSI32, , , ); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_CH(NixSI16, NixFLOAT, , , * 32767.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixSI16, NixSI16, , , ); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_CH(NixUI8, NixUI8, , , ); }
            }
        } else if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 2){
            //duplicate src channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixFLOAT, NixFLOAT, , ,); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648.); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647.); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI32, NixSI32, , , ); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI16, NixFLOAT, , , * 32767.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF)); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI16, NixSI16, , , ); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80)); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_1_TO_2_CH(NixUI8, NixUI8, , , ); }
            }
        } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 1){
            //merge src channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixFLOAT, NixFLOAT, , , , 2.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., 2); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, 2); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, 2); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., 2.); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI32, NixSI32, , , , 2); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), 2); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), 2); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI16, NixFLOAT, , , * 32767.f, 2.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), 2); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI16, NixSI16, , , , 2); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), 2); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, 2.f); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, 2); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, 2); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_INC_FREQ_2_TO_1_CH(NixUI8, NixUI8, , , , 2); }
            }
        }
    }
    return r;
}

#define FMT_CONVERTER_DEC_FREQ_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, ACCUM_VAR_NAME) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        obj->samplesAccum.ACCUM_VAR_NAME[0] += (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        obj->samplesAccum.count++; \
        obj->samplesAccum.fixed += accumPerOrgSample; \
        src0 += srcAlign0; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = (LEFT_TYPE)(obj->samplesAccum.ACCUM_VAR_NAME[0] / obj->samplesAccum.count); \
            dst0 += dstAlign0; \
            if(obj->samplesAccum.fixed < NIX_FMT_CONVERTER_FREQ_PRECISION){ \
                obj->samplesAccum.ACCUM_VAR_NAME[0] = 0; \
                obj->samplesAccum.count = 0; \
            } \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_DEC_FREQ_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, ACCUM_VAR_NAME) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        obj->samplesAccum.ACCUM_VAR_NAME[0] += (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        obj->samplesAccum.ACCUM_VAR_NAME[1] += (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        obj->samplesAccum.count++; \
        obj->samplesAccum.fixed += accumPerOrgSample; \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = (LEFT_TYPE)(obj->samplesAccum.ACCUM_VAR_NAME[0] / obj->samplesAccum.count); \
            *(LEFT_TYPE*)dst1 = (LEFT_TYPE)(obj->samplesAccum.ACCUM_VAR_NAME[1] / obj->samplesAccum.count); \
            dst0 += dstAlign0; \
            dst1 += dstAlign1; \
            if(obj->samplesAccum.fixed < NIX_FMT_CONVERTER_FREQ_PRECISION){ \
                obj->samplesAccum.ACCUM_VAR_NAME[0] = obj->samplesAccum.ACCUM_VAR_NAME[1] = 0; \
                obj->samplesAccum.count = 0; \
            } \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, ACCUM_VAR_NAME) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    STNix_FmtConverterChannel* dstCh1 = &obj->dst.channels[1]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    NixBYTE *dst1 = (NixBYTE*)dstCh1->ptr; NixUI32 dstAlign1 = dstCh1->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        obj->samplesAccum.ACCUM_VAR_NAME[0] += *(LEFT_TYPE*)dst1 = (LEFT_TYPE) (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2); \
        src0 += srcAlign0; \
        obj->samplesAccum.count++; \
        obj->samplesAccum.fixed += accumPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = *(LEFT_TYPE*)dst1 = (LEFT_TYPE)(obj->samplesAccum.ACCUM_VAR_NAME[0] / obj->samplesAccum.count); \
            dst0 += dstAlign0; \
            dst1 += dstAlign1; \
            if(obj->samplesAccum.fixed < NIX_FMT_CONVERTER_FREQ_PRECISION){ \
                obj->samplesAccum.ACCUM_VAR_NAME[0] = obj->samplesAccum.ACCUM_VAR_NAME[1] = 0; \
                obj->samplesAccum.count = 0; \
            } \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;

#define FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2, DIV_2_WITH_SUFIX, ACCUM_VAR_NAME) \
    STNix_FmtConverterChannel* srcCh0 = &obj->src.channels[0]; \
    STNix_FmtConverterChannel* srcCh1 = &obj->src.channels[1]; \
    STNix_FmtConverterChannel* dstCh0 = &obj->dst.channels[0]; \
    NixBYTE *src0 = (NixBYTE*)srcCh0->ptr; NixUI32 srcAlign0 = srcCh0->sampleAlign; \
    NixBYTE *src1 = (NixBYTE*)srcCh1->ptr; NixUI32 srcAlign1 = srcCh1->sampleAlign; \
    NixBYTE *dst0 = (NixBYTE*)dstCh0->ptr; NixUI32 dstAlign0 = dstCh0->sampleAlign; \
    const NixBYTE *dst0AfterEnd = dst0 + (dstAlign0 * dstBlocks); \
    for(i = 0; i < srcBlocks && dst0 < dst0AfterEnd; ++i){ \
        obj->samplesAccum.ACCUM_VAR_NAME[0] += (LEFT_TYPE) (((((RIGHT_MATH_CAST *(RIGHT_TYPE*)src0) RIGHT_MATH_OP1) RIGHT_MATH_OP2) + (((RIGHT_MATH_CAST *(RIGHT_TYPE*)src1) RIGHT_MATH_OP1) RIGHT_MATH_OP2)) / DIV_2_WITH_SUFIX); \
        src0 += srcAlign0; \
        src1 += srcAlign1; \
        obj->samplesAccum.count++; \
        obj->samplesAccum.fixed += accumPerOrgSample; \
        while(obj->samplesAccum.fixed >= NIX_FMT_CONVERTER_FREQ_PRECISION && dst0 < dst0AfterEnd){ \
            obj->samplesAccum.fixed -= NIX_FMT_CONVERTER_FREQ_PRECISION; \
            *(LEFT_TYPE*)dst0 = (LEFT_TYPE)(obj->samplesAccum.ACCUM_VAR_NAME[0] / obj->samplesAccum.count); \
            dst0 += dstAlign0; \
        } \
        if(obj->samplesAccum.fixed < NIX_FMT_CONVERTER_FREQ_PRECISION){ \
            obj->samplesAccum.ACCUM_VAR_NAME[0] = obj->samplesAccum.ACCUM_VAR_NAME[1] = 0; \
            obj->samplesAccum.count = 0; \
        } \
    } \
    if(dstAmmBlocksRead != NULL) *dstAmmBlocksRead = i; \
    if(dstAmmBlocksWritten != NULL) *dstAmmBlocksWritten = (NixUI32)(dst0 - (NixBYTE*)dstCh0->ptr) / dstAlign0; \
    r = NIX_TRUE;


NixBOOL nixFmtConverter_convertDecFreq_(STNix_FmtConverter* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    //decreasing freq
    const NixUI32 accumPerOrgSample = obj->dst.desc.samplerate * NIX_FMT_CONVERTER_FREQ_PRECISION / obj->src.desc.samplerate;
    if(accumPerOrgSample <= 0){
        //freq difference is below 'NIX_FMT_CONVERTER_FREQ_PRECISION', just ignore
        r = nixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
    } else {
        NixUI32 i = 0;
        if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 1){
            //same 1-channel
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_CH(NixFLOAT, NixFLOAT, , , , accumFloat); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., accumFloat); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, accumFloat); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, accumFloat); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., accumSI64); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixSI32, NixSI32, , , , accumSI64); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), accumSI64); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), accumSI64); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_CH(NixSI16, NixFLOAT, , , * 32767.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixSI16, NixSI16, , , , accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), accumSI32); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_CH(NixUI8, NixUI8, , , , accumSI32); }
            }
        } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 2){
            //same 2-channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_CH(NixFLOAT, NixFLOAT, , , , accumFloat); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., accumFloat); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, accumFloat); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, accumFloat); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., accumSI64); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixSI32, NixSI32, , , , accumSI64); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), accumSI64); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), accumSI64); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_CH(NixSI16, NixFLOAT, , , * 32767.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixSI16, NixSI16, , , , accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), accumSI32); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_CH(NixUI8, NixUI8, , , , accumSI32); }
            }
        } else if(obj->src.desc.channels == 1 && obj->dst.desc.channels == 2){
            //duplicate src channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixFLOAT, NixFLOAT, , , , accumFloat); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., accumFloat); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, accumFloat); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, accumFloat); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., accumSI64); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI32, NixSI32, , , , accumSI64); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), accumSI64); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), accumSI64); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI16, NixFLOAT, , , * 32767.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI16, NixSI16, , , , accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), accumSI32); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_1_TO_2_CH(NixUI8, NixUI8, , , , accumSI32); }
            }
        } else if(obj->src.desc.channels == 2 && obj->dst.desc.channels == 1){
            //merge src channels
            if(FMT_CONVERTER_IS_FLOAT32(obj->dst.desc)){            //to FLOAT32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixFLOAT, NixFLOAT, , , , 2.f, accumFloat); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixFLOAT, NixSI32, (NixDOUBLE), , / 2147483648., 2, accumFloat); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixFLOAT, NixSI16, (NixFLOAT), , / 32768.f, 2, accumFloat); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixFLOAT, NixUI8, (NixFLOAT), -128.f , / 128.f, 2, accumFloat); }
            } else if(FMT_CONVERTER_IS_SI32(obj->dst.desc)){        //to SI32
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI32, NixFLOAT, (NixDOUBLE), , * 2147483647., 2., accumSI64); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI32, NixSI32, , , , 2, accumSI64); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI32, NixSI16, (NixSI32), + 1, * (0x7FFFFFFF / 0x7FFF), 2, accumSI64); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI32, NixUI8, (NixSI64), - 127, * (0x7FFFFFFF / 0x80), 2, accumSI64); }
            } else if(FMT_CONVERTER_IS_SI16(obj->dst.desc)){        //to SI16
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI16, NixFLOAT, , , * 32767.f, 2.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI16, NixSI32, , , / (0x7FFFFFFF / 0x7FFF), 2, accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI16, NixSI16, , , , 2, accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixSI16, NixUI8, (NixSI32), - 127, * (0x7FFF / 0x80), 2, accumSI32); }
            } else if(FMT_CONVERTER_IS_UI8(obj->dst.desc)){         //to UI8
                if(FMT_CONVERTER_IS_FLOAT32(obj->src.desc)){        FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixUI8, NixFLOAT, , + 1.f, * 127.f, 2.f, accumSI32); }
                else if(FMT_CONVERTER_IS_SI32(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixUI8, NixSI32, , / (0x7FFFFFFF / 0x7F), + 127, 2, accumSI32); }
                else if(FMT_CONVERTER_IS_SI16(obj->src.desc)){      FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixUI8, NixSI16, , / (0x7FFF / 0x7F), + 127, 2, accumSI32); }
                else if(FMT_CONVERTER_IS_UI8(obj->src.desc)){       FMT_CONVERTER_DEC_FREQ_2_TO_1_CH(NixUI8, NixUI8, , , , 2, accumSI32); }
            }
        }
    }
    return r;
}

//

NixUI32 nixFmtConverter_maxChannels(void){
    return NIX_FMT_CONVERTER_CHANNELS_MAX;
}

NixUI32 nixFmtConverter_samplesForNewFrequency(const NixUI32 ammSampesOrg, const NixUI32 freqOrg, const NixUI32 freqNew){   //ammount of output samples from one frequeny to another, +1 for safety
    NixUI32 r = ammSampesOrg;
    if(freqOrg < freqNew){
        //increasing freq
        const NixUI32 delta = (freqNew - freqOrg);
        const NixUI32 repeatPerOrgSample = delta * NIX_FMT_CONVERTER_FREQ_PRECISION / freqOrg;
        r = ammSampesOrg + ( ammSampesOrg * repeatPerOrgSample / NIX_FMT_CONVERTER_FREQ_PRECISION ) + 1;
    } else if(freqOrg > freqNew){
        //decreasing freq
        const NixUI32 accumPerOrgSample = freqNew * NIX_FMT_CONVERTER_FREQ_PRECISION / freqOrg;
        if(accumPerOrgSample > 0){
            r = ( ammSampesOrg * accumPerOrgSample / NIX_FMT_CONVERTER_FREQ_PRECISION) + 1;
        } else {
            r = ammSampesOrg + 1;
        }
    }
    return r;
}
