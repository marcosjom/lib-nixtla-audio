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
#include <stdlib.h> //malloc

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

//Default API

NixBOOL NixApiItf_getDefaultApiForCurrentOS(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        //API
#       ifdef NIX_DEFAULT_API_AVFAUDIO
        r = nixAVAudioEngine_getApiItf(dst);
#       elif defined(NIX_DEFAULT_API_AAUDIO)
        r = nixAAudioEngine_getApiItf(dst);
#       elif defined(NIX_DEFAULT_API_OPENAL)
        r = nixOpenALEngine_getApiItf(dst);
#       endif
    }
    return r;
}

//Audio description
   
NixBOOL STNixAudioDesc_isEqual(const STNixAudioDesc* obj, const STNixAudioDesc* other){
    return (obj->samplesFormat == other->samplesFormat
            && obj->channels == other->channels
            && obj->bitsPerSample == other->bitsPerSample
            && obj->samplerate == other->samplerate
            && obj->blockAlign == other->blockAlign) ? NIX_TRUE : NIX_FALSE;
}

// STNixSharedPtr (provides retain/release model)

typedef struct STNixSharedPtr_ {
    void*           opq;        //opaque object, must be first member to allow toll-free casting
    STNixMutexRef   mutex;
    NixSI32         retainCount;
    STNixMemoryItf  memItf;
} STNixSharedPtr;

struct STNixSharedPtr_* NixSharedPtr_alloc(STNixContextItf* itf, void* opq, const char* dbgHintStr){
    struct STNixSharedPtr_* obj = NULL;
    obj = (struct STNixSharedPtr_*)(*itf->mem.malloc)(sizeof(struct STNixSharedPtr_), dbgHintStr);
    if(obj != NULL){
        obj->mutex = (itf->mutex.alloc)(itf);
        obj->retainCount = 1; //retained by creator
        //
        obj->opq = opq;
        obj->memItf = itf->mem;
    }
    return obj;
}

void NixSharedPtr_free(struct STNixSharedPtr_* obj){
    NixMutex_free(&obj->mutex);
    (*obj->memItf.free)(obj);
}

void* NixSharedPtr_getOpq(struct STNixSharedPtr_* obj){
    //can also be obtained by casting 'obj' as '*(void**)obj'
    return (obj != NULL ? obj->opq : NULL);
}

void NixSharedPtr_retain(struct STNixSharedPtr_* obj){
    NixMutex_lock(obj->mutex);
    {
        NIX_ASSERT(obj->retainCount > 0) //if fails, the pointer was re-activated during cleanup (change your code to avoid this)
        ++obj->retainCount;
    }
    NixMutex_unlock(obj->mutex);
    /*
#   ifdef NIX_DEBUG
    if(obj->dbgItf.retainedBy != NULL){
        const STNixDbgThreadState* st = NixDbgThreadState_get();
        (*obj->dbgItf.retainedBy)(obj, (st != NULL && st->stack.use > 0 ? st->stack.arr[st->stack.use - 1].className.str : NULL));
    }
#   endif
    */
}

NixSI32 NixSharedPtr_release(struct STNixSharedPtr_* obj){
    NixSI32 r = 0;
    NixMutex_lock(obj->mutex);
    {
        NIX_ASSERT(obj->retainCount > 0)
        r = --obj->retainCount;
    }
    NixMutex_unlock(obj->mutex);
    /*
#   ifdef NIX_DEBUG
    if(obj->dbgItf.releasedBy != NULL){
        const STNixDbgThreadState* st = NixDbgThreadState_get();
        (*obj->dbgItf.releasedBy)(obj, (st != NULL && st->stack.use > 0 ? st->stack.arr[st->stack.use - 1].className.str : NULL));
    }
#   endif
     */
    return r;
}

#define NIX_REF_METHOD_DEFINITION_VOID(TYPE, METHOD, PARAMS_DEFS, PARAMS_NAMES)   \
void TYPE ## _ ## METHOD PARAMS_DEFS { \
        if(ref.itf != NULL && ref.itf->METHOD != NULL){ \
            (*ref.itf->METHOD)PARAMS_NAMES; \
        } \
    }

#define NIX_REF_METHOD_DEFINITION_BOOL(TYPE, METHOD, PARAMS_DEFS, PARAMS_NAMES)   \
    NixBOOL TYPE ## _ ## METHOD PARAMS_DEFS{ \
        return (ref.itf != NULL && ref.itf->METHOD != NULL ? (*ref.itf->METHOD)PARAMS_NAMES : NIX_FALSE); \
    }

#define NIX_REF_METHOD_DEFINITION_FLOAT(TYPE, METHOD, PARAMS_DEFS, PARAMS_NAMES)   \
    NixFLOAT TYPE ## _ ## METHOD PARAMS_DEFS{ \
        return (ref.itf != NULL && ref.itf->METHOD != NULL ? (*ref.itf->METHOD)PARAMS_NAMES : 0.f); \
    }


//STNixEngineRef (shared pointer)

#define STNixEngineRef_Zero     { NULL, NULL }

STNixEngineRef NixEngine_alloc(STNixContextRef ctx, struct STNixApiItf_* apiItf){
    return (ctx.ptr != NULL && apiItf != NULL && apiItf->engine.alloc != NULL ? (*apiItf->engine.alloc)(ctx) : (STNixEngineRef)STNixEngineRef_Zero);
}

void NixEngine_retain(STNixEngineRef ref){
    NixSharedPtr_retain(ref.ptr);
}

void NixEngine_release(STNixEngineRef* ref){
    if(ref != NULL && ref->ptr != NULL){
        if(0 == NixSharedPtr_release(ref->ptr)){
            if(ref->itf != NULL && ref->itf->free != NULL){
                (*ref->itf->free)(*ref);
            }
        }
    }
}

NIX_REF_METHOD_DEFINITION_VOID(NixEngine, printCaps, (STNixEngineRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixEngine, ctxIsActive, (STNixEngineRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixEngine, ctxActivate, (STNixEngineRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixEngine, ctxDeactivate, (STNixEngineRef ref), (ref))
NIX_REF_METHOD_DEFINITION_VOID(NixEngine, tick, (STNixEngineRef ref), (ref))

//Factory

STNixSourceRef NixEngine_allocSource(STNixEngineRef ref){
    return (ref.itf != NULL && ref.itf->allocSource != NULL ? (*ref.itf->allocSource)(ref) : (STNixSourceRef)STNixSourceRef_Zero);
}

STNixBufferRef NixEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    return (ref.itf != NULL && ref.itf->allocBuffer != NULL ? (*ref.itf->allocBuffer)(ref, audioDesc, audioDataPCM, audioDataPCMBytes) : (STNixBufferRef)STNixBufferRef_Zero);
}

STNixRecorderRef NixEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    return (ref.itf != NULL && ref.itf->allocRecorder != NULL ? (*ref.itf->allocRecorder)(ref, audioDesc, buffersCount, blocksPerBuffer) : (STNixRecorderRef)STNixRecorderRef_Zero);
}

//STNixBufferRef (shared pointer)

#define STNixBufferRef_Zero     { NULL, NULL }

void NixBuffer_retain(STNixBufferRef ref){
    NixSharedPtr_retain(ref.ptr);
}

void NixBuffer_release(STNixBufferRef* ref){
    if(ref != NULL && ref->ptr != NULL){
        if(0 == NixSharedPtr_release(ref->ptr)){
            if(ref->itf != NULL && ref->itf->free != NULL){
                (*ref->itf->free)(*ref);
            }
        }
    }
}

NIX_REF_METHOD_DEFINITION_BOOL(NixBuffer, setData, (STNixBufferRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes), (ref, audioDesc, audioDataPCM, audioDataPCMBytes) )
NIX_REF_METHOD_DEFINITION_BOOL(NixBuffer, fillWithZeroes, (STNixBufferRef ref), (ref))


//STNixSourceRef (shared pointer)

#define STNixSourceRef_Zero     { NULL, NULL }

void NixSource_retain(STNixSourceRef ref){
    NixSharedPtr_retain(ref.ptr);
}

void NixSource_release(STNixSourceRef* ref){
    if(ref != NULL && ref->ptr != NULL){
        if(0 == NixSharedPtr_release(ref->ptr)){
            NIX_ASSERT(ref->ptr->memItf.free != NULL) //testing
            if(ref->itf != NULL && ref->itf->free != NULL){
                (*ref->itf->free)(*ref);
            }
        }
    }
}

NIX_REF_METHOD_DEFINITION_VOID(NixSource, setCallback, (STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData), (ref, callback, callbackData))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, setVolume, (STNixSourceRef ref, const float vol), (ref, vol))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, setRepeat, (STNixSourceRef ref, const NixBOOL isRepeat), (ref, isRepeat))
NIX_REF_METHOD_DEFINITION_VOID(NixSource, play, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_VOID(NixSource, pause, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_VOID(NixSource, stop, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, isPlaying, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, isPaused, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, isRepeat, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_FLOAT(NixSource, getVolume, (STNixSourceRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, setBuffer, (STNixSourceRef ref, STNixBufferRef buff), (ref, buff))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, queueBuffer, (STNixSourceRef ref, STNixBufferRef buff), (ref, buff))
NIX_REF_METHOD_DEFINITION_BOOL(NixSource, setBufferOffset, (STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset), (ref,  type, offset))
NixUI32 NixSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){   //all buffer queue
    if(ref.itf != NULL && ref.itf->getBuffersCount != NULL){
        return (*ref.itf->getBuffersCount)(ref, optDstBytesCount, optDstBlocksCount, optDstMsecsCount);
    }
    if(optDstBytesCount != NULL) *optDstBytesCount = 0;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = 0;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = 0;
    return 0;
}

NixUI32 NixSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){  //relative to first buffer in queue
    if(ref.itf != NULL && ref.itf->getBlocksOffset != NULL){
        return (*ref.itf->getBlocksOffset)(ref, optDstBytesCount, optDstBlocksCount, optDstMsecsCount);
    }
    if(optDstBytesCount != NULL) *optDstBytesCount = 0;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = 0;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = 0;
    return 0;
}

//STNixRecorderRef (shared pointer)

#define STNixRecorderRef_Zero     { NULL, NULL }

void NixRecorder_retain(STNixRecorderRef ref){
    NixSharedPtr_retain(ref.ptr);
}

void NixRecorder_release(STNixRecorderRef* ref){
    if(ref != NULL && ref->ptr != NULL){
        if(0 == NixSharedPtr_release(ref->ptr)){
            if(ref->itf != NULL && ref->itf->free != NULL){
                (*ref->itf->free)(*ref);
            }
        }
    }
}

NIX_REF_METHOD_DEFINITION_BOOL(NixRecorder, setCallback, (STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData), (ref, callback, callbackData))
NIX_REF_METHOD_DEFINITION_BOOL(NixRecorder, start, (STNixRecorderRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixRecorder, stop, (STNixRecorderRef ref), (ref))
NIX_REF_METHOD_DEFINITION_BOOL(NixRecorder, flush, (STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying), (ref, includeCurrentPartialBuff, discardWithoutNotifying))
NIX_REF_METHOD_DEFINITION_BOOL(NixRecorder, isCapturing, (STNixRecorderRef ref), (ref))
NixUI32 NixRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){  //relative to first buffer in queue
    if(ref.itf != NULL && ref.itf->getBuffersFilledCount != NULL){
        return (*ref.itf->getBuffersFilledCount)(ref, optDstBytesCount, optDstBlocksCount, optDstMsecsCount);
    }
    if(optDstBytesCount != NULL) *optDstBytesCount = 0;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = 0;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = 0;
    return 0;
}

// STNixMutexRef

void NixMutex_lock(STNixMutexRef ref){
    if(ref.opq != NULL && ref.itf != NULL && ref.itf->lock != NULL){
        (*ref.itf->lock)(ref);
    }
}

void NixMutex_unlock(STNixMutexRef ref){
    if(ref.opq != NULL && ref.itf != NULL && ref.itf->unlock != NULL){
        (*ref.itf->unlock)(ref);
    }
}

void NixMutex_free(STNixMutexRef* ref){
    if(ref->opq != NULL && ref->itf != NULL && ref->itf->free != NULL){
        (*ref->itf->free)(ref);
    }
}

//

#define NIX_ITF_SET_MISSING_METHOD_TO_NOP(ITF, ITF_NAME, M_NAME) \
    if(itf->M_NAME == NULL) itf->M_NAME = ITF_NAME ## _nop_ ## M_NAME

#define NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(ITF, ITF_NAME, M_NAME) \
    if(itf->M_NAME == NULL) itf->M_NAME = ITF_NAME ## _default_ ## M_NAME

//STNixMemoryItf (API)

void* NixMemoryItf_default_malloc(const NixUI32 newSz, const char* dbgHintStr){
    return malloc(newSz);
}

void* NixMemoryItf_default_realloc(void* ptr, const NixUI32 newSz, const char* dbgHintStr){
    //"If there is not enough memory, the old memory block is not freed and null pointer is returned."
    return realloc(ptr, newSz);
}

void NixMemoryItf_default_free(void* ptr){
    free(ptr);
}

//Links NULL methods to a DEFAULT implementation,
//this reduces the need to check for functions NULL pointers.
void NixMemoryItf_fillMissingMembers(STNixMemoryItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMemoryItf, malloc);
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMemoryItf, realloc);
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMemoryItf, free);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixMutexItf (API)

#if !defined(NIX_MUTEX_T) || !defined(NIX_MUTEX_INIT) || !defined(NIX_MUTEX_DESTROY) || !defined(NIX_MUTEX_LOCK) || !defined(NIX_MUTEX_UNLOCK)
#   ifdef _WIN32
//#     define WIN32_LEAN_AND_MEAN
#       include <windows.h>             //for CRITICAL_SECTION
#       define NIX_MUTEX_T              CRITICAL_SECTION
#       define NIX_MUTEX_INIT(PTR)      InitializeCriticalSection(PTR)
#       define NIX_MUTEX_DESTROY(PTR)   DeleteCriticalSection(PTR)
#       define NIX_MUTEX_LOCK(PTR)      EnterCriticalSection(PTR)
#       define NIX_MUTEX_UNLOCK(PTR)    LeaveCriticalSection(PTR)
#   else
#       include <pthread.h>             //for pthread_mutex_t
#       define NIX_MUTEX_T              pthread_mutex_t
#       define NIX_MUTEX_INIT(PTR)      pthread_mutex_init(PTR, NULL)
#       define NIX_MUTEX_DESTROY(PTR)   pthread_mutex_destroy(PTR)
#       define NIX_MUTEX_LOCK(PTR)      pthread_mutex_lock(PTR)
#       define NIX_MUTEX_UNLOCK(PTR)    pthread_mutex_unlock(PTR)
#   endif
#endif

typedef struct STNixMutexOpq_ {
    NIX_MUTEX_T             mutex;
    struct STNixContextItf_ ctx;
} STNixMutexOpq;

STNixMutexRef NixMutexItf_default_alloc(struct STNixContextItf_* ctx){
    STNixMutexRef r = STNixMutexRef_Zero;
    STNixMutexOpq* obj = (STNixMutexOpq*)(*ctx->mem.malloc)(sizeof(STNixMutexOpq), "NixMutexItf_default_alloc");
    if(obj != NULL){
        NIX_MUTEX_INIT(&obj->mutex);
        obj->ctx = *ctx;
        r.opq = obj;
        r.itf = &obj->ctx.mutex;
    }
    return r;
}

void NixMutexItf_default_free(STNixMutexRef* pObj){
    if(pObj != NULL){
        STNixMutexOpq* obj = (STNixMutexOpq*)pObj->opq;
        if(obj != NULL){
            NIX_MUTEX_DESTROY(&obj->mutex);
            (*obj->ctx.mem.free)(obj);
        }
        pObj->opq = NULL;
        pObj->itf = NULL;
    }
}

void NixMutexItf_default_lock(STNixMutexRef pObj){
    if(pObj.opq != NULL){
        STNixMutexOpq* obj = (STNixMutexOpq*)pObj.opq;
        NIX_MUTEX_LOCK(&obj->mutex);
    }
}

void NixMutexItf_default_unlock(STNixMutexRef pObj){
    if(pObj.opq != NULL){
        STNixMutexOpq* obj = (STNixMutexOpq*)pObj.opq;
        NIX_MUTEX_UNLOCK(&obj->mutex);
    }
}

//Links NULL methods to a DEFAULT implementation,
//this reduces the need to check for functions NULL pointers.
void NixMutexItf_fillMissingMembers(STNixMutexItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMutexItf, alloc);
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMutexItf, free);
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMutexItf, lock);
    NIX_ITF_SET_MISSING_METHOD_TO_DEFAULT(itf, NixMutexItf, unlock);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixContextRef

typedef struct STNixContextOpq_ {
    NixUI32 dummy;  //empty struct
} STNixContextOpq;

STNixContextRef NixContext_alloc(STNixContextItf* ctx){
    STNixContextRef r = STNixContextRef_Zero;
    if(ctx != NULL){
        STNixContextOpq* opq = (STNixContextOpq*)(ctx->mem.malloc)(sizeof(STNixContextOpq), "NixContext_alloc::opq");
        STNixContextItf* itf = (STNixContextItf*)(ctx->mem.malloc)(sizeof(STNixContextItf), "NixContext_alloc::itf");
        STNixSharedPtr* ptr  = NixSharedPtr_alloc(ctx, opq, "NixContext_alloc");
        if(opq != NULL && itf != NULL && ptr != NULL){
            memset(opq, 0, sizeof(*opq));
            memcpy(itf, ctx, sizeof(*itf));
            r.ptr = ptr; ptr = NULL; //consume
            r.itf = itf; itf = NULL; //consume
            opq = NULL; //consume
        }
        //release (if not consumed)
        if(opq != NULL){
            (ctx->mem.free)(opq);
            opq = NULL;
        }
        if(itf != NULL){
            (ctx->mem.free)(itf);
            itf = NULL;
        }
        if(ptr != NULL){
            NixSharedPtr_free(ptr);
            ptr = NULL;
        }
    }
    return r;
}

void NixContext_retain(STNixContextRef ref){
    NixSharedPtr_retain(ref.ptr);
}

void NixContext_release(STNixContextRef* ref){
    if(ref != NULL && 0 == NixSharedPtr_release(ref->ptr)){
        STNixContextOpq* opq = (STNixContextOpq*)NixSharedPtr_getOpq(ref->ptr);
        STNixContextRef cpy = *ref;
        *ref = (STNixContextRef)STNixContextRef_Zero;
        //free
        if(opq != NULL){
            (cpy.itf->mem.free)(opq);
            opq = NULL;
        }
        if(cpy.itf != NULL){
            (cpy.itf->mem.free)(cpy.itf);
            cpy.itf = NULL;
        }
        if(cpy.ptr != NULL){
            NixSharedPtr_free(cpy.ptr);
            cpy.ptr = NULL;
        }
    }
}

void NixContext_set(STNixContextRef* ref, STNixContextRef other){
    //retain first
    if(other.ptr != NULL){
        NixContext_retain(other);
    }
    //release after
    if(ref->ptr != NULL){
        NixContext_release(ref);
    }
    //set
    *ref = other;
}

void NixContext_null(STNixContextRef* ref){
    *ref = (STNixContextRef)STNixContextRef_Zero;
}

//context (memory)

void* NixContext_malloc(STNixContextRef ref, const NixUI32 newSz, const char* dbgHintStr){
    return (ref.itf != NULL && ref.itf->mem.malloc != NULL ? (*ref.itf->mem.malloc)(newSz, dbgHintStr) : NULL);
}

void* NixContext_mrealloc(STNixContextRef ref, void* ptr, const NixUI32 newSz, const char* dbgHintStr){
    return (ref.itf != NULL && ref.itf->mem.realloc != NULL ? (*ref.itf->mem.realloc)(ptr, newSz, dbgHintStr) : NULL);
}

void NixContext_mfree(STNixContextRef ref, void* ptr){
    if(ref.itf != NULL && ref.itf->mem.free != NULL){
        (*ref.itf->mem.free)(ptr);
    }
}

//context (mutex)

STNixMutexRef NixContext_mutex_alloc(STNixContextRef ref){
    return (ref.itf != NULL && ref.itf->mutex.alloc != NULL ? (*ref.itf->mutex.alloc)(ref.itf) : (STNixMutexRef)STNixMutexRef_Zero);
}


//STNixContextItf (API)

STNixContextItf NixContextItf_getDefault(void){
    STNixContextItf itf;
    memset(&itf, 0, sizeof(itf));
    NixContextItf_fillMissingMembers(&itf);
    return itf;
}

//Links NULL methods to a DEFAULT implementation,
//this reduces the need to check for functions NULL pointers.
void NixContextItf_fillMissingMembers(STNixContextItf* itf){
    if(itf == NULL) return;
    NixMemoryItf_fillMissingMembers(&itf->mem);
    NixMutexItf_fillMissingMembers(&itf->mutex);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixEngineItf

STNixEngineRef  NixEngineItf_nop_alloc(STNixContextRef ctx) { return (STNixEngineRef)STNixEngineRef_Zero; }
void            NixEngineItf_nop_free(STNixEngineRef ref) { }
void            NixEngineItf_nop_printCaps(STNixEngineRef ref) { }
NixBOOL         NixEngineItf_nop_ctxIsActive(STNixEngineRef ref) { return NIX_FALSE; }
NixBOOL         NixEngineItf_nop_ctxActivate(STNixEngineRef ref) { return NIX_FALSE; }
NixBOOL         NixEngineItf_nop_ctxDeactivate(STNixEngineRef ref) { return NIX_FALSE; }
void            NixEngineItf_nop_tick(STNixEngineRef ref) { }
//Factory
STNixSourceRef  NixEngineItf_nop_allocSource(STNixEngineRef ref) { return (STNixSourceRef)STNixSourceRef_Zero; }
STNixBufferRef  NixEngineItf_nop_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes) { return (STNixBufferRef)STNixBufferRef_Zero; }
STNixRecorderRef NixEngineItf_nop_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer) { return (STNixRecorderRef)STNixRecorderRef_Zero; }

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixEngineItf_fillMissingMembers(STNixEngineItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, alloc);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, free);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, printCaps);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, ctxIsActive);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, ctxActivate);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, ctxActivate);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, ctxDeactivate);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, tick);
    //Factory
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, allocSource);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, allocBuffer);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixEngineItf, allocRecorder);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixBufferItf

STNixBufferRef  NixBufferItf_nop_alloc(STNixContextRef ctx, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes) { return (STNixBufferRef)STNixBufferRef_Zero; }
void            NixBufferItf_nop_free(STNixBufferRef ref) { }
NixBOOL         NixBufferItf_nop_setData(STNixBufferRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes) { return NIX_FALSE; }
NixBOOL         NixBufferItf_nop_fillWithZeroes(STNixBufferRef ref) { return NIX_FALSE; }

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixBufferItf_fillMissingMembers(STNixBufferItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixBufferItf, alloc);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixBufferItf, free);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixBufferItf, setData);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixBufferItf, fillWithZeroes);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixSourceItf

STNixSourceRef  NixSourceItf_nop_alloc(STNixEngineRef eng) { return (STNixSourceRef)STNixSourceRef_Zero; }
void            NixSourceItf_nop_free(STNixSourceRef ref) { }
void            NixSourceItf_nop_setCallback(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData) { }
NixBOOL         NixSourceItf_nop_setVolume(STNixSourceRef ref, const float vol)  { return NIX_FALSE; }
NixBOOL         NixSourceItf_nop_setRepeat(STNixSourceRef ref, const NixBOOL isRepeat)  { return NIX_FALSE; }
void            NixSourceItf_nop_play(STNixSourceRef ref) { }
void            NixSourceItf_nop_pause(STNixSourceRef ref) { }
void            NixSourceItf_nop_stop(STNixSourceRef ref) { }
NixBOOL         NixSourceItf_nop_isPlaying(STNixSourceRef ref) { return NIX_FALSE; }
NixBOOL         NixSourceItf_nop_isPaused(STNixSourceRef ref) { return NIX_FALSE; }
NixBOOL         NixSourceItf_nop_isRepeat(STNixSourceRef ref) { return NIX_FALSE; }
NixFLOAT        NixSourceItf_nop_getVolume(STNixSourceRef ref) { return 0.f; }
NixBOOL         NixSourceItf_nop_setBuffer(STNixSourceRef ref, STNixBufferRef buff) { return NIX_FALSE; }
NixBOOL         NixSourceItf_nop_queueBuffer(STNixSourceRef ref, STNixBufferRef buff) { return NIX_FALSE; }
NixBOOL         NixSourceItf_nop_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset){ return NIX_FALSE; } //relative to first buffer in queue
NixUI32         NixSourceItf_nop_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount) { if(optDstBytesCount != NULL) *optDstBytesCount = 0; if(optDstBlocksCount != NULL) *optDstBlocksCount = 0; if(optDstMsecsCount != NULL) *optDstMsecsCount = 0; return 0; }
NixUI32         NixSourceItf_nop_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount) { if(optDstBytesCount != NULL) *optDstBytesCount = 0; if(optDstBlocksCount != NULL) *optDstBlocksCount = 0; if(optDstMsecsCount != NULL) *optDstMsecsCount = 0; return 0; }


//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixSourceItf_fillMissingMembers(STNixSourceItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, alloc);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, free);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, setCallback);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, setVolume);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, setRepeat);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, play);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, pause);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, stop);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, isPlaying);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, isPaused);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, isRepeat);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, getVolume);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, setBuffer);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, queueBuffer);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, setBufferOffset);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, getBuffersCount);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixSourceItf, getBlocksOffset);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixRecorderItf

STNixRecorderRef NixRecorderItf_nop_alloc(STNixEngineRef eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer) { return (STNixRecorderRef)STNixRecorderRef_Zero; }
void            NixRecorderItf_nop_free(STNixRecorderRef ref) { }
NixBOOL         NixRecorderItf_nop_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData) { return NIX_FALSE; }
NixBOOL         NixRecorderItf_nop_start(STNixRecorderRef ref) { return NIX_FALSE; }
NixBOOL         NixRecorderItf_nop_stop(STNixRecorderRef ref) { return NIX_FALSE; }
NixBOOL         NixRecorderItf_nop_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying) { return NIX_FALSE; }
NixBOOL         NixRecorderItf_nop_isCapturing(STNixRecorderRef ref) { return NIX_FALSE; }
NixUI32         NixRecorderItf_nop_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount) { if(optDstBytesCount != NULL) *optDstBytesCount = 0; if(optDstBlocksCount != NULL) *optDstBlocksCount = 0; if(optDstMsecsCount != NULL) *optDstMsecsCount = 0; return 0; }

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixRecorderItf_fillMissingMembers(STNixRecorderItf* itf){
    if(itf == NULL) return;
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, alloc);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, free);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, setCallback);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, start);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, stop);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, flush);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, isCapturing);
    NIX_ITF_SET_MISSING_METHOD_TO_NOP(itf, NixRecorderItf, getBuffersFilledCount);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//STNixApiItf

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixApiItf_fillMissingMembers(STNixApiItf* itf){
    if(itf == NULL) return;
    NixEngineItf_fillMissingMembers(&itf->engine);
    NixBufferItf_fillMissingMembers(&itf->buffer);
    NixSourceItf_fillMissingMembers(&itf->source);
    NixRecorderItf_fillMissingMembers(&itf->recorder);
    //validate missing implementations
#   ifdef NIX_ASSERTS_ACTIVATED
    {
        void** ptr = (void**)itf;
        void** afterEnd = ptr + (sizeof(*itf) / sizeof(void*));
        while(ptr < afterEnd){
            NIX_ASSERT(*ptr != NULL) //function pointer should be defined
            ptr++;
        }
    }
#   endif
}

//------
//PCMBuffer
//------

void NixPCMBuffer_init(STNixContextRef ctx, STNixPCMBuffer* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
}

void NixPCMBuffer_destroy(STNixPCMBuffer* obj){
    if(obj->ptr != NULL){
        NixContext_mfree(obj->ctx, obj->ptr);
        obj->ptr = NULL;
    }
    obj->use = obj->sz = 0;
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixPCMBuffer_setData(STNixPCMBuffer* obj, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        const NixUI32 reqBytes = (audioDataPCMBytes / audioDesc->blockAlign * audioDesc->blockAlign);
        //destroy current buffer (if necesary)
        if(!STNixAudioDesc_isEqual(&obj->desc, audioDesc) || obj->sz < reqBytes){
            if(obj->ptr != NULL){
                NixContext_mfree(obj->ctx, obj->ptr);
                obj->ptr = NULL;
            }
            obj->use = obj->sz = 0;
        }
        //set fmt
        obj->desc = *audioDesc;
        obj->use = 0;
        //copy data
        if(reqBytes <= 0){
            r = NIX_TRUE;
        } else {
            //allocate buffer (if necesary)
            if(obj->ptr == NULL){
                obj->ptr = (NixUI8*)NixContext_malloc(obj->ctx, reqBytes, "NixPCMBuffer_setData::ptr");
                if(obj->ptr != NULL){
                    obj->sz = reqBytes;
                }
            }
            //results
            if(obj->ptr != NULL){
                //copy data
                if(audioDataPCM != NULL){
                    memcpy(obj->ptr, audioDataPCM, reqBytes);
                }
                obj->use = reqBytes;
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL NixPCMBuffer_fillWithZeroes(STNixPCMBuffer* obj){
    NixBOOL r = NIX_FALSE;
    if(obj->ptr != NULL){
        if(obj->use < obj->sz){
            memset(&((NixBYTE*)obj->ptr)[obj->use], 0, obj->sz - obj->use);
            obj->use = obj->sz;
        }
        r = NIX_TRUE;
    }
    return r;
}

//------
//Notif
//------

void NixSourceNotif_init(STNixSourceNotif* obj, STNixSourceRef src, const STNixSourceCallback callback){
    memset(obj, 0, sizeof(*obj));
    if(!NixSource_isNull(src)){
        NixSource_set(&obj->source, src);
    }
    obj->callback = callback;
}

void NixSourceNotif_destroy(STNixSourceNotif* obj){
    {
        NixSI32 i; for(i= 0; i < obj->buffsUse; i++){
            STNixBufferRef* buff = &obj->buffs[i];
            if(!NixBuffer_isNull(*buff)){
                NixBuffer_release(buff);
                NixBuffer_null(buff);
            }
        }
        obj->buffsUse = 0;
    }
    if(!NixSource_isNull(obj->source)){
        NixSource_release(&obj->source);
        NixSource_null(&obj->source);
    }
}

NixBOOL NixSourceNotif_addBuff(STNixSourceNotif* obj, STNixBufferRef buff){
    NixBOOL r = NIX_FALSE;
    if(!NixBuffer_isNull(buff) && obj->buffsUse < sizeof(obj->buffs) / sizeof(obj->buffs[0])){
        obj->buffs[obj->buffsUse++] = buff;
        NixBuffer_retain(buff);
        r = NIX_TRUE;
    }
    return r;
}

//------
//NotifQueue
//------

void NixNotifQueue_init(STNixContextRef ctx, STNixNotifQueue* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
    obj->arr = obj->arrEmbedded;
    obj->sz = (sizeof(obj->arrEmbedded) / sizeof(obj->arrEmbedded[0]));
}

void NixNotifQueue_destroy(STNixNotifQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNixSourceNotif* b = &obj->arr[i];
            NixSourceNotif_destroy(b);
        }
        if(obj->arr != obj->arrEmbedded){
            NixContext_mfree(obj->ctx, obj->arr);
        }
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixNotifQueue_addBuff(STNixNotifQueue* obj, STNixSourceRef src, const STNixSourceCallback callback, STNixBufferRef buff){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        STNixSourceNotif* lst = (obj->use != 0 ? &obj->arr[obj->use - 1] : NULL);
        if(lst != NULL && NixSource_isSame(src, lst->source)){
            //accumulate buffer in last record
            r = NixSourceNotif_addBuff(lst, buff);
        }
        if(!r){
            //resize array (if necesary)
            if(obj->use >= obj->sz){
                const NixUI32 szN = obj->use + 4;
                STNixSourceNotif* arrN = (STNixSourceNotif*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixSourceNotif) * szN, "NixNotifQueue_addBuff::arrN");
                if(arrN != NULL){
                    obj->arr = arrN;
                    obj->sz = szN;
                }
            }
            //add
            if(obj->use >= obj->sz){
                NIX_PRINTF_ERROR("NixNotifQueue_addBuff failed (no allocated space).\n");
            } else {
                //become the owner of the pair
                STNixSourceNotif n;
                NixSourceNotif_init(&n, src, callback);
                if(!NixSourceNotif_addBuff(&n, buff)){
                    NixSourceNotif_destroy(&n);
                    NIX_ASSERT(NIX_FALSE) //program logic error
                } else {
                    obj->arr[obj->use++] = n;
                    r = NIX_TRUE;
                }
            }
        }
    }
    return r;
}

//STNixPCMBuffer (API, common)

STNixBufferRef  nixPCMBuffer_alloc(STNixContextRef ctx, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
void            nixPCMBuffer_free(STNixBufferRef ref);
NixBOOL         nixPCMBuffer_setData(STNixBufferRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL         nixPCMBuffer_fillWithZeroes(STNixBufferRef ref);

NixBOOL NixPCMBuffer_getApiItf(STNixBufferItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        //
        dst->alloc      = nixPCMBuffer_alloc;
        dst->free       = nixPCMBuffer_free;
        dst->setData    = nixPCMBuffer_setData;
        dst->fillWithZeroes = nixPCMBuffer_fillWithZeroes;
        //
        NixBufferItf_fillMissingMembers(dst);
        //
        r = NIX_TRUE;
    }
    return r;
}


//------
//PCMBuffer API
//------

STNixBufferRef nixPCMBuffer_alloc(STNixContextRef ctx, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixBufferRef r = STNixBufferRef_Zero;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        STNixPCMBuffer* obj = (STNixPCMBuffer*)NixContext_malloc(ctx, sizeof(STNixPCMBuffer), "STNixPCMBuffer");
        STNixBufferItf* itf = NULL;
        if(obj != NULL){
            NixPCMBuffer_init(ctx, obj);
            if(!NixPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes)){
                NIX_PRINTF_ERROR("nixPCMBuffer_alloc::NixPCMBuffer_setData failed.\n");
            } else {
                itf = (STNixBufferItf*)NixContext_malloc(ctx, sizeof(STNixBufferItf), "STNixBufferItf");
                if(itf == NULL){
                    NIX_PRINTF_ERROR("nixPCMBuffer_alloc::malloc(STNixBufferItf) failed.\n");
                } else if(!NixPCMBuffer_getApiItf(itf)){
                    NIX_PRINTF_ERROR("nixPCMBuffer_alloc::NixPCMBuffer_getApiItf failed.\n");
                } else if(NULL == (r.ptr = NixSharedPtr_alloc(ctx.itf, obj, "nixPCMBuffer_alloc"))){
                    NIX_PRINTF_ERROR("nixPCMBuffer_alloc::NixSharedPtr_alloc failed.\n");
                } else {
                    NIX_ASSERT(r.ptr != NULL)
                    r.itf = itf; itf = NULL; //consume
                    obj = NULL; //consume
                }
            }
        }
        //release (if not consumed)
        if(itf != NULL){
            NixContext_mfree(ctx, itf);
            itf = NULL;
        }
        if(obj != NULL){
            NixPCMBuffer_destroy(obj);
            NixContext_mfree(ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixPCMBuffer_free(STNixBufferRef pObj){
    if(pObj.ptr != NULL){
        STNixPCMBuffer* obj = (STNixPCMBuffer*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixPCMBuffer_destroy(obj);
            }
            if(memItf.free != NULL){
                if(pObj.itf != NULL){
                    (*memItf.free)(pObj.itf);
                }
                pObj.itf = NULL;
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}
   
NixBOOL nixPCMBuffer_setData(STNixBufferRef pObj, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && audioDesc != NULL && audioDesc->blockAlign > 0){
        STNixPCMBuffer* obj = (STNixPCMBuffer*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

NixBOOL nixPCMBuffer_fillWithZeroes(STNixBufferRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixPCMBuffer* obj = (STNixPCMBuffer*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixPCMBuffer_fillWithZeroes(obj);
    }
    return r;
}

#define NIX_FMT_CONVERTER_FREQ_PRECISION    512 //fixed point-denominator
#define NIX_FMT_CONVERTER_CHANNELS_MAX      2

//PCMFormat converter

typedef struct STNixFmtConvChannel_ {
    void*   ptr;
    NixUI32 sampleAlign; //jumps to get the next sample
} STNixFmtConvChannel;

typedef struct STNixFmtConvSide_ {
    STNixAudioDesc      desc;
    STNixFmtConvChannel channels[NIX_FMT_CONVERTER_CHANNELS_MAX];
} STNixFmtConvSide;

typedef struct STNixFmtConv_ {
    STNixContextRef     ctx;
    STNixFmtConvSide    src;
    STNixFmtConvSide    dst;
    //accum
    struct {
        NixUI32         fixed;
        NixUI32         count;
        //accumulated values
        union {
            NixFLOAT    accumFloat[NIX_FMT_CONVERTER_CHANNELS_MAX];
            NixSI64     accumSI64[NIX_FMT_CONVERTER_CHANNELS_MAX];
            NixSI32     accumSI32[NIX_FMT_CONVERTER_CHANNELS_MAX];
        };
    } samplesAccum;
} STNixFmtConv;

void* NixFmtConverter_alloc(STNixContextRef ctx){
    STNixFmtConv* r = (STNixFmtConv*)NixContext_malloc(ctx, sizeof(STNixFmtConv), "STNixFmtConv");
    memset(r, 0, sizeof(STNixFmtConv));
    NixContext_set(&r->ctx, ctx);
    return r;
}

void NixFmtConverter_free(void* pObj){
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL){
        STNixContextRef ctxCpy = obj->ctx;
        {
            NixContext_null(&obj->ctx);
            NixContext_mfree(ctxCpy, obj);
        }
        NixContext_release(&ctxCpy); //release ctx after memory is freed
        //
        obj = NULL;
    }
}

NixBOOL NixFmtConverter_prepare(void* pObj, const STNixAudioDesc* srcDesc, const STNixAudioDesc* dstDesc){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL && srcDesc != NULL && dstDesc != NULL && srcDesc->channels > 0 && srcDesc->channels <= NIX_FMT_CONVERTER_CHANNELS_MAX && dstDesc->channels > 0 && dstDesc->channels <= NIX_FMT_CONVERTER_CHANNELS_MAX){
        if(
           //src
           (srcDesc->samplesFormat == ENNixSampleFmt_Float && srcDesc->bitsPerSample == 32)
           || (srcDesc->samplesFormat == ENNixSampleFmt_Int && (srcDesc->bitsPerSample == 8 || srcDesc->bitsPerSample == 16 || srcDesc->bitsPerSample == 32))
           //dst
           || (dstDesc->samplesFormat == ENNixSampleFmt_Float && dstDesc->bitsPerSample == 32)
           || (dstDesc->samplesFormat == ENNixSampleFmt_Int && (dstDesc->bitsPerSample == 8 || dstDesc->bitsPerSample == 16 || dstDesc->bitsPerSample == 32))
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

NixBOOL NixFmtConverter_setPtrAtSrc(void* pObj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL && ptr != NULL && sampleAlign > 0 && iChannel < obj->src.desc.channels && iChannel < (sizeof(obj->src.channels) / sizeof(obj->src.channels[0]))){
        STNixFmtConvChannel* ch = &obj->src.channels[iChannel];
        ch->ptr = ptr;
        ch->sampleAlign = sampleAlign;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixFmtConverter_setPtrAtDst(void* pObj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL && ptr != NULL && sampleAlign > 0 && iChannel < obj->dst.desc.channels && iChannel < (sizeof(obj->dst.channels) / sizeof(obj->dst.channels[0]))){
        STNixFmtConvChannel* ch = &obj->dst.channels[iChannel];
        ch->ptr = ptr;
        ch->sampleAlign = sampleAlign;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixFmtConverter_setPtrAtSrcInterlaced(void* pObj, const STNixAudioDesc* desc, void* ptr, const NixUI32 iFirstSample){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL && desc != NULL){
        r = NIX_TRUE;
        NixUI32 iCh, chCountDst = desc->channels;
        for(iCh = 0; iCh < chCountDst && iCh < NIX_FMT_CONVERTER_CHANNELS_MAX; ++iCh){
            const NixUI32 samplePos = (iFirstSample * desc->blockAlign);
            const NixUI32 sampleChannelPos = samplePos + ((desc->bitsPerSample / 8) * iCh);
            if(!NixFmtConverter_setPtrAtSrc(pObj, iCh, &(((NixBYTE*)ptr)[sampleChannelPos]), desc->blockAlign)){
                NIX_PRINTF_ERROR("NixFmtConverter_setPtrAtSrcInterlaced::NixFmtConverter_setPtrAtSrc(iCh:%d, blockAlign:%d, ptr:%s) failed.\n", iCh, desc->blockAlign, (ptr != NULL ? "not-null" : "NULL"));
                r = NIX_FALSE;
                break;
            }
        }
    }
    return r;
}

NixBOOL NixFmtConverter_setPtrAtDstInterlaced(void* pObj, const STNixAudioDesc* desc, void* ptr, const NixUI32 iFirstSample){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL && desc != NULL){
        r = NIX_TRUE;
        NixUI32 iCh, chCountDst = desc->channels;
        for(iCh = 0; iCh < chCountDst && iCh < NIX_FMT_CONVERTER_CHANNELS_MAX; ++iCh){
            const NixUI32 samplePos = (iFirstSample * desc->blockAlign);
            const NixUI32 sampleChannelPos = samplePos + ((desc->bitsPerSample / 8) * iCh);
            if(!NixFmtConverter_setPtrAtDst(pObj, iCh, &(((NixBYTE*)ptr)[sampleChannelPos]), desc->blockAlign)){
                r = NIX_FALSE;
                break;
            }
        }
    }
    return r;
}

NixBOOL NixFmtConverter_convertSameFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);
NixBOOL NixFmtConverter_convertIncFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);
NixBOOL NixFmtConverter_convertDecFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);

NixBOOL NixFmtConverter_convert(void* pObj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    STNixFmtConv* obj = (STNixFmtConv*)pObj;
    if(obj != NULL){
        if(obj->src.desc.samplerate == obj->dst.desc.samplerate){
            //same freq
            r = NixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        } else if(obj->src.desc.samplerate < obj->dst.desc.samplerate){
            //increasing freq
            r = NixFmtConverter_convertIncFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        } else {
            //decreasing freq
            r = NixFmtConverter_convertDecFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
        }
    }
    return r;
}

#define FMT_CONVERTER_IS_FLOAT32(DESC)  ((DESC).samplesFormat == ENNixSampleFmt_Float && (DESC).bitsPerSample == 32)
#define FMT_CONVERTER_IS_SI32(DESC)     ((DESC).samplesFormat == ENNixSampleFmt_Int && (DESC).bitsPerSample == 32)
#define FMT_CONVERTER_IS_SI16(DESC)     ((DESC).samplesFormat == ENNixSampleFmt_Int && (DESC).bitsPerSample == 16)
#define FMT_CONVERTER_IS_UI8(DESC)      ((DESC).samplesFormat == ENNixSampleFmt_Int && (DESC).bitsPerSample == 8)

#define FMT_CONVERTER_SAME_FREQ_1_CH(LEFT_TYPE, RIGHT_TYPE, RIGHT_MATH_CAST, RIGHT_MATH_OP1, RIGHT_MATH_OP2) \
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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

NixBOOL NixFmtConverter_convertSameFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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

NixBOOL NixFmtConverter_convertIncFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    //increasing freq
    const NixUI32 delta = (obj->dst.desc.samplerate - obj->src.desc.samplerate);
    const NixUI32 repeatPerOrgSample = delta * NIX_FMT_CONVERTER_FREQ_PRECISION / obj->src.desc.samplerate;
    if(repeatPerOrgSample == 0){
        //freq difference is below 'NIX_FMT_CONVERTER_FREQ_PRECISION', just ignore
        r = NixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
    STNixFmtConvChannel* dstCh1 = &obj->dst.channels[1]; \
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
    STNixFmtConvChannel* srcCh0 = &obj->src.channels[0]; \
    STNixFmtConvChannel* srcCh1 = &obj->src.channels[1]; \
    STNixFmtConvChannel* dstCh0 = &obj->dst.channels[0]; \
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


NixBOOL NixFmtConverter_convertDecFreq_(STNixFmtConv* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten){
    NixBOOL r = NIX_FALSE;
    //decreasing freq
    const NixUI32 accumPerOrgSample = obj->dst.desc.samplerate * NIX_FMT_CONVERTER_FREQ_PRECISION / obj->src.desc.samplerate;
    if(accumPerOrgSample <= 0){
        //freq difference is below 'NIX_FMT_CONVERTER_FREQ_PRECISION', just ignore
        r = NixFmtConverter_convertSameFreq_(obj, srcBlocks, dstBlocks, dstAmmBlocksRead, dstAmmBlocksWritten);
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

NixUI32 NixFmtConverter_maxChannels(void){
    return NIX_FMT_CONVERTER_CHANNELS_MAX;
}

NixUI32 NixFmtConverter_blocksForNewFrequency(const NixUI32 ammBlocksOrg, const NixUI32 freqOrg, const NixUI32 freqNew){   //ammount of output samples from one frequeny to another, +1 for safety
    NixUI32 r = ammBlocksOrg;
    if(freqOrg < freqNew){
        //increasing freq
        const NixUI32 delta = (freqNew - freqOrg);
        const NixUI32 repeatPerOrgSample = delta * NIX_FMT_CONVERTER_FREQ_PRECISION / freqOrg;
        r = ammBlocksOrg + ( ammBlocksOrg * repeatPerOrgSample / NIX_FMT_CONVERTER_FREQ_PRECISION ) + 1;
    } else if(freqOrg > freqNew){
        //decreasing freq
        const NixUI32 accumPerOrgSample = freqNew * NIX_FMT_CONVERTER_FREQ_PRECISION / freqOrg;
        if(accumPerOrgSample > 0){
            r = ( ammBlocksOrg * accumPerOrgSample / NIX_FMT_CONVERTER_FREQ_PRECISION) + 1;
        } else {
            r = ammBlocksOrg + 1;
        }
    }
    return r;
}


