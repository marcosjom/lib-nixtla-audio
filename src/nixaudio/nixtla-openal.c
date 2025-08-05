//
//  nixtla-aaudio.c
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 12/07/25.
//  Copyright (c) 2025 Marcos Ortega. All rights reserved.
//
//  This entire notice must be retained in this source code.
//  This source code is under MIT Licence.
//
//  This software is provided "as is", with absolutely no warranty expressed
//  or implied. Any use is at your own risk.
//
//  Latest fixes enhancements and documentation at https://github.com/marcosjom/lib-nixtla-audio
//

//
//This file adds support for OpenAL for playing and recording.
//

#include "nixtla-audio-private.h"
#include "nixaudio/nixtla-audio.h"
#include "nixtla-openal.h"
#include <string.h> //for memset()

#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#   include <OpenAL/al.h>
#   include <OpenAL/alc.h>
#else
#   include <AL/al.h>
#   include <AL/alc.h>
#endif

#define NIX_OPENAL_NULL     AL_NONE

#ifdef NIX_ASSERTS_ACTIVATED
#   define NIX_OPENAL_ERR_VERIFY(nomFunc)   { ALenum idErrorAL=alGetError(); if(idErrorAL != AL_NO_ERROR){ NIX_PRINTF_ERROR("'%s' (#%d) en %s\n", alGetString(idErrorAL), idErrorAL, nomFunc);} NIX_ASSERT(idErrorAL == AL_NO_ERROR);}
#else
#   define NIX_OPENAL_ERR_VERIFY(nomFunc)   { }
#endif

//------
//API Itf
//------

//OpenAL interface

//Engine
STNixEngineRef  nixOpenALEngine_alloc(STNixContextRef ctx);
void            nixOpenALEngine_free(STNixEngineRef ref);
void            nixOpenALEngine_printCaps(STNixEngineRef ref);
NixBOOL         nixOpenALEngine_ctxIsActive(STNixEngineRef ref);
NixBOOL         nixOpenALEngine_ctxActivate(STNixEngineRef ref);
NixBOOL         nixOpenALEngine_ctxDeactivate(STNixEngineRef ref);
void            nixOpenALEngine_tick(STNixEngineRef ref);
//Factory
STNixSourceRef  nixOpenALEngine_allocSource(STNixEngineRef ref);
STNixBufferRef  nixOpenALEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
STNixRecorderRef nixOpenALEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
//Source
STNixSourceRef  nixOpenALSource_alloc(STNixEngineRef eng);
void            nixOpenALSource_free(STNixSourceRef ref);
void            nixOpenALSource_setCallback(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData);
NixBOOL         nixOpenALSource_setVolume(STNixSourceRef ref, const float vol);
NixBOOL         nixOpenALSource_setRepeat(STNixSourceRef ref, const NixBOOL isRepeat);
void            nixOpenALSource_play(STNixSourceRef ref);
void            nixOpenALSource_pause(STNixSourceRef ref);
void            nixOpenALSource_stop(STNixSourceRef ref);
NixBOOL         nixOpenALSource_isPlaying(STNixSourceRef ref);
NixBOOL         nixOpenALSource_isPaused(STNixSourceRef ref);
NixBOOL         nixOpenALSource_isRepeat(STNixSourceRef ref);
NixFLOAT        nixOpenALSource_getVolume(STNixSourceRef ref);
NixBOOL         nixOpenALSource_setBuffer(STNixSourceRef ref, STNixBufferRef buff);  //static-source
NixBOOL         nixOpenALSource_queueBuffer(STNixSourceRef ref, STNixBufferRef buff); //stream-source
NixBOOL         nixOpenALSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset); //relative to first buffer in queue
NixUI32         nixOpenALSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //all buffer queue
NixUI32         nixOpenALSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //relative to first buffer in queue
//Recorder
STNixRecorderRef nixOpenALRecorder_alloc(STNixEngineRef eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
void            nixOpenALRecorder_free(STNixRecorderRef ref);
NixBOOL         nixOpenALRecorder_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL         nixOpenALRecorder_start(STNixRecorderRef ref);
NixBOOL         nixOpenALRecorder_stop(STNixRecorderRef ref);
NixBOOL         nixOpenALRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying);
NixBOOL         nixOpenALRecorder_isCapturing(STNixRecorderRef ref);
NixUI32         nixOpenALRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);

NixBOOL nixOpenALEngine_getApiItf(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->engine.alloc       = nixOpenALEngine_alloc;
        dst->engine.free        = nixOpenALEngine_free;
        dst->engine.printCaps   = nixOpenALEngine_printCaps;
        dst->engine.ctxIsActive = nixOpenALEngine_ctxIsActive;
        dst->engine.ctxActivate = nixOpenALEngine_ctxActivate;
        dst->engine.ctxDeactivate = nixOpenALEngine_ctxDeactivate;
        dst->engine.tick        = nixOpenALEngine_tick;
        //Factory
        dst->engine.allocSource = nixOpenALEngine_allocSource;
        dst->engine.allocBuffer = nixOpenALEngine_allocBuffer;
        dst->engine.allocRecorder = nixOpenALEngine_allocRecorder;
        //PCMBuffer
        NixPCMBuffer_getApiItf(&dst->buffer);
        //Source
        dst->source.alloc       = nixOpenALSource_alloc;
        dst->source.free        = nixOpenALSource_free;
        dst->source.setCallback = nixOpenALSource_setCallback;
        dst->source.setVolume   = nixOpenALSource_setVolume;
        dst->source.setRepeat   = nixOpenALSource_setRepeat;
        dst->source.play        = nixOpenALSource_play;
        dst->source.pause       = nixOpenALSource_pause;
        dst->source.stop        = nixOpenALSource_stop;
        dst->source.isPlaying   = nixOpenALSource_isPlaying;
        dst->source.isPaused    = nixOpenALSource_isPaused;
        dst->source.isRepeat    = nixOpenALSource_isRepeat;
        dst->source.getVolume   = nixOpenALSource_getVolume;
        dst->source.setBuffer   = nixOpenALSource_setBuffer;  //static-source
        dst->source.queueBuffer = nixOpenALSource_queueBuffer; //stream-source
        dst->source.setBufferOffset = nixOpenALSource_setBufferOffset; //relative to first buffer in queue
        dst->source.getBuffersCount = nixOpenALSource_getBuffersCount; //all buffer queue
        dst->source.getBlocksOffset = nixOpenALSource_getBlocksOffset; //relative to first buffer in queue
        //Recorder
        dst->recorder.alloc     = nixOpenALRecorder_alloc;
        dst->recorder.free      = nixOpenALRecorder_free;
        dst->recorder.setCallback   = nixOpenALRecorder_setCallback;
        dst->recorder.start     = nixOpenALRecorder_start;
        dst->recorder.stop      = nixOpenALRecorder_stop;
        dst->recorder.flush     = nixOpenALRecorder_flush;
        dst->recorder.isCapturing = nixOpenALRecorder_isCapturing;
        dst->recorder.getBuffersFilledCount = nixOpenALRecorder_getBuffersFilledCount;
        //
        r = NIX_TRUE;
    }
    return r;
}

struct STNixOpenALEngine_;
struct STNixOpenALSource_;
struct STNixOpenALQueue_;
struct STNixOpenALQueuePair_;
struct STNixOpenALRecorder_;

//------
//Engine
//------

typedef struct STNixOpenALEngine_ {
    STNixContextRef ctx;
    STNixApiItf     apiItf;
    NixUI32         maskCapabilities;
    NixBOOL         contextALIsCurrent;
    ALCcontext*     contextAL;
    ALCdevice*      deviceAL;
    ALCdevice*      idCaptureAL;                //OpenAL specific
    NixUI32         captureMainBufferBytesCount;    //OpenAL specific
    //srcs
    struct {
        STNixMutexRef   mutex;
        struct STNixOpenALSource_** arr;
        NixUI32         use;
        NixUI32         sz;
    } srcs;
    struct STNixOpenALRecorder_* rec;
} STNixOpenALEngine;

void NixOpenALEngine_init(STNixContextRef ctx, STNixOpenALEngine* obj);
void NixOpenALEngine_destroy(STNixOpenALEngine* obj);
NixBOOL NixOpenALEngine_srcsAdd(STNixOpenALEngine* obj, struct STNixOpenALSource_* src);
void NixOpenALEngine_tick(STNixOpenALEngine* obj, const NixBOOL isFinalCleanup);


//------
//QueuePair (Buffers)
//------

typedef struct STNixOpenALQueuePair_ {
    STNixBufferRef  org;    //original buffer (owned by the user)
    ALuint          idBufferAL; //converted buffer (owned by the source)
} STNixOpenALQueuePair;

void NixOpenALQueuePair_init(STNixOpenALQueuePair* obj);
void NixOpenALQueuePair_destroy(STNixOpenALQueuePair* obj);
void NixOpenALQueuePair_moveOrg(STNixOpenALQueuePair* obj, STNixOpenALQueuePair* to);
void NixOpenALQueuePair_moveCnv(STNixOpenALQueuePair* obj, STNixOpenALQueuePair* to);

//------
//Queue (Buffers)
//------

typedef struct STNixOpenALQueue_ {
    STNixContextRef         ctx;
    STNixOpenALQueuePair*   arr;
    NixUI32                 use;
    NixUI32                 sz;
} STNixOpenALQueue;

void NixOpenALQueue_init(STNixContextRef ctx, STNixOpenALQueue* obj);
void NixOpenALQueue_destroy(STNixOpenALQueue* obj);
//
NixBOOL NixOpenALQueue_flush(STNixOpenALQueue* obj);
NixBOOL NixOpenALQueue_prepareForSz(STNixOpenALQueue* obj, const NixUI32 minSz);
NixBOOL NixOpenALQueue_pushOwning(STNixOpenALQueue* obj, STNixOpenALQueuePair* pair);
NixBOOL NixOpenALQueue_popOrphaning(STNixOpenALQueue* obj, STNixOpenALQueuePair* dst);
NixBOOL NixOpenALQueue_popMovingTo(STNixOpenALQueue* obj, STNixOpenALQueue* other);

//------
//Source
//------

typedef struct STNixOpenALSource_ {
    STNixContextRef         ctx;
    STNixSourceRef          self;
    struct STNixOpenALEngine_* eng;    //parent engine
    STNixAudioDesc          buffsFmt;   //first attached buffers' format (defines the converter config)
    STNixAudioDesc          srcFmt;
    ALenum                  srcFmtAL;
    ALuint                  idSourceAL;
    //queues
    struct {
        STNixMutexRef       mutex;
        //conv
        struct {
            void*           obj;   //NixFmtConverter
            //buff
            struct {
                void*       ptr;
                NixUI32     sz;
            } buff;
        } conv;
        STNixSourceCallback callback;
        STNixOpenALQueue    notify; //buffers (consumed, pending to notify)
        STNixOpenALQueue    reuse;  //buffers (conversion buffers)
        STNixOpenALQueue    pend;   //to be played/filled
        NixUI32             pendBlockIdx;  //current sample playing/filling
    } queues;
    //props
    float                   volume;
    NixUI8                  stateBits;  //packed bools to reduce padding, NIX_OpenALSource_BIT_
} STNixOpenALSource;

void NixOpenALSource_init(STNixContextRef ctx, STNixOpenALSource* obj);
void NixOpenALSource_destroy(STNixOpenALSource* obj);
NixBOOL NixOpenALSource_queueBufferForOutput(STNixOpenALSource* obj, STNixBufferRef buff, const NixBOOL isStream);
NixBOOL NixOpenALSource_pendPopOldestBuffLocked_(STNixOpenALSource* obj);
NixBOOL NixOpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNixOpenALSource* obj);

#define NIX_OpenALSource_BIT_isStatic   (0x1 << 0)  //source expects only one buffer, repeats or pauses after playing it
#define NIX_OpenALSource_BIT_isChanging (0x1 << 1)  //source is changing state after a call to request*()
#define NIX_OpenALSource_BIT_isRepeat   (0x1 << 2)
#define NIX_OpenALSource_BIT_isPlaying  (0x1 << 3)
#define NIX_OpenALSource_BIT_isPaused   (0x1 << 4)
#define NIX_OpenALSource_BIT_isClosing  (0x1 << 5)
#define NIX_OpenALSource_BIT_isOrphan   (0x1 << 6)  //source is waiting for close(), wait for the change of state and NixOpenALSource_release + free.
//
#define NixOpenALSource_isStatic(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isStatic) != 0)
#define NixOpenALSource_isChanging(OBJ)        (((OBJ)->stateBits & NIX_OpenALSource_BIT_isChanging) != 0)
#define NixOpenALSource_isRepeat(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isRepeat) != 0)
#define NixOpenALSource_isPlaying(OBJ)         (((OBJ)->stateBits & NIX_OpenALSource_BIT_isPlaying) != 0)
#define NixOpenALSource_isPaused(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isPaused) != 0)
#define NixOpenALSource_isClosing(OBJ)         (((OBJ)->stateBits & NIX_OpenALSource_BIT_isClosing) != 0)
#define NixOpenALSource_isOrphan(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isOrphan) != 0)
//
#define NixOpenALSource_setIsStatic(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isStatic : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isStatic)
#define NixOpenALSource_setIsChanging(OBJ, V)  (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isChanging : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isChanging)
#define NixOpenALSource_setIsRepeat(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isRepeat : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isRepeat)
#define NixOpenALSource_setIsPlaying(OBJ, V)   (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isPlaying : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isPlaying)
#define NixOpenALSource_setIsPaused(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isPaused : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isPaused)
#define NixOpenALSource_setIsClosing(OBJ)      (OBJ)->stateBits = ((OBJ)->stateBits | NIX_OpenALSource_BIT_isClosing)
#define NixOpenALSource_setIsOrphan(OBJ)       (OBJ)->stateBits = ((OBJ)->stateBits | NIX_OpenALSource_BIT_isOrphan)

//------
//Recorder
//------

typedef struct STNixOpenALRecorder_ {
    STNixContextRef         ctx;
    NixBOOL                 engStarted;
    STNixEngineRef          engRef;
    STNixRecorderRef        selfRef;
    ALuint                  idCaptureAL;
    STNixAudioDesc          capFmt;
    //callback
    struct {
        NixRecorderCallbackFnc func;
        void*               data;
    } callback;
    //cfg
    struct {
        STNixAudioDesc      fmt;
        NixUI16             blocksPerBuffer;
        NixUI16             maxBuffers;
    } cfg;
    //queues
    struct {
        STNixMutexRef       mutex;
        void*               conv;   //NixFmtConverter
        STNixOpenALQueue    notify;
        STNixOpenALQueue    reuse;
        //filling
        struct {
            NixUI8*         tmp;
            NixUI32         tmpSz;
            NixSI32         iCurSample; //at first buffer in 'reuse'
        } filling;
    } queues;
} STNixOpenALRecorder;

void NixOpenALRecorder_init(STNixContextRef ctx, STNixOpenALRecorder* obj);
void NixOpenALRecorder_destroy(STNixOpenALRecorder* obj);
//
NixBOOL NixOpenALRecorder_prepare(STNixOpenALRecorder* obj, STNixOpenALEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
NixBOOL NixOpenALRecorder_setCallback(STNixOpenALRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL NixOpenALRecorder_start(STNixOpenALRecorder* obj);
NixBOOL NixOpenALRecorder_stop(STNixOpenALRecorder* obj);
NixBOOL NixOpenALRecorder_flush(STNixOpenALRecorder* obj);
void NixOpenALRecorder_consumeInputBuffer(STNixOpenALRecorder* obj);
void NixOpenALRecorder_notifyBuffers(STNixOpenALRecorder* obj, const NixBOOL discardWithoutNotifying);

//------
//Engine
//------

void NixOpenALEngine_init(STNixContextRef ctx, STNixOpenALEngine* obj){
    memset(obj, 0, sizeof(STNixOpenALEngine));
    //
    NixContext_set(&obj->ctx, ctx);
    nixOpenALEngine_getApiItf(&obj->apiItf);
    //
    obj->deviceAL = NIX_OPENAL_NULL;
    obj->contextAL = NIX_OPENAL_NULL;
    //srcs
    {
        obj->srcs.mutex = NixContext_mutex_alloc(obj->ctx);
    }
}
  
void NixOpenALEngine_destroy(STNixOpenALEngine* obj){
    //srcs
    {
        //cleanup
        while(obj->srcs.arr != NULL && obj->srcs.use > 0){
            NixOpenALEngine_tick(obj, NIX_TRUE);
        }
        //
        if(obj->srcs.arr != NULL){
            NixContext_mfree(obj->ctx, obj->srcs.arr);
            obj->srcs.arr = NULL;
        }
        NixMutex_free(&obj->srcs.mutex);
    }
    //api
    if(alcMakeContextCurrent(NULL) == AL_FALSE){
        NIX_PRINTF_ERROR("alcMakeContextCurrent(NULL) failed\n");
    } else {
        obj->contextALIsCurrent = NIX_FALSE;
        alcDestroyContext(obj->contextAL); NIX_OPENAL_ERR_VERIFY("alcDestroyContext");
        if(!alcCloseDevice(obj->deviceAL)){
            NIX_PRINTF_ERROR("alcCloseDevice failed\n");
        } NIX_OPENAL_ERR_VERIFY("alcCloseDevice");
        obj->deviceAL = NIX_OPENAL_NULL;
        obj->contextAL = NIX_OPENAL_NULL;
    }
    //rec (recorder)
    if(obj->rec != NULL){
        obj->rec = NULL;
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixOpenALEngine_srcsAdd(STNixOpenALEngine* obj, struct STNixOpenALSource_* src){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        NixMutex_lock(obj->srcs.mutex);
        {
            //resize array (if necesary)
            if(obj->srcs.use >= obj->srcs.sz){
                const NixUI32 szN = obj->srcs.use + 4;
                STNixOpenALSource** arrN = (STNixOpenALSource**)NixContext_mrealloc(obj->ctx, obj->srcs.arr, sizeof(STNixOpenALSource*) * szN, "STNixOpenALEngine::srcsN");
                if(arrN != NULL){
                    obj->srcs.arr = arrN;
                    obj->srcs.sz = szN;
                }
            }
            //add
            if(obj->srcs.use >= obj->srcs.sz){
                NIX_PRINTF_ERROR("nixOpenALSource_create::STNixOpenALEngine::srcs failed (no allocated space).\n");
            } else {
                //become the owner of the pair
                obj->srcs.arr[obj->srcs.use++] = src;
                r = NIX_TRUE;
            }
        }
        NixMutex_unlock(obj->srcs.mutex);
    }
    return r;
}

void NixOpenALEngine_removeSrcRecordLocked_(STNixOpenALEngine* obj, NixSI32* idx){
    STNixOpenALSource* src = obj->srcs.arr[*idx];
    if(src != NULL){
        NixOpenALSource_destroy(src);
        NixContext_mfree(obj->ctx, src);
    }
    //fill gap
    --obj->srcs.use;
    {
        NixUI32 i2;
        for(i2 = *idx; i2 < obj->srcs.use; ++i2){
            obj->srcs.arr[i2] = obj->srcs.arr[i2 + 1];
        }
    }
    *idx = *idx - 1; //process record again
}

void NixOpenALEngine_tick_addQueueNotifSrcLocked_(STNixNotifQueue* notifs, STNixOpenALSource* src){
    if(src->queues.notify.use > 0){
        NixSI32 i; for(i = 0; i < src->queues.notify.use; i++){
            STNixOpenALQueuePair* pair = &src->queues.notify.arr[i];
            if(!NixNotifQueue_addBuff(notifs, src->self, src->queues.callback, pair->org)){
                NIX_ASSERT(NIX_FALSE); //program logic error
            }
        }
        if(!NixOpenALQueue_flush(&src->queues.notify)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        }
    }
}

void NixOpenALEngine_tick(STNixOpenALEngine* obj, const NixBOOL isFinalCleanup){
    if(obj != NULL){
        //srcs
        {
            STNixNotifQueue notifs;
            NixNotifQueue_init(obj->ctx, &notifs);
            NixMutex_lock(obj->srcs.mutex);
            if(obj->srcs.arr != NULL && obj->srcs.use > 0){
                //NIX_PRINTF_INFO("NixOpenALEngine_tick::%d sources.\n", obj->srcs.use);
                NixSI32 i; for(i = 0; i < (NixSI32)obj->srcs.use; ++i){
                    STNixOpenALSource* src = obj->srcs.arr[i];
                    //NIX_PRINTF_INFO("NixOpenALEngine_tick::source(#%d/%d).\n", i + 1, obj->srcs.use);
                    if(NixOpenALSource_isOrphan(src)){
                        //src
                        if(src->idSourceAL != NIX_OPENAL_NULL){
                            alSourceStop(src->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
                            alDeleteSources(1, &src->idSourceAL); NIX_OPENAL_ERR_VERIFY("alDeleteSources");
                            src->idSourceAL = NIX_OPENAL_NULL;
                        }
                    }
                    if(src->idSourceAL == NIX_OPENAL_NULL){
                        //remove
                        //NIX_PRINTF_INFO("NixOpenALEngine_tick::source(#%d/%d); remove-NULL.\n", i + 1, obj->srcs.use);
                        NixOpenALEngine_removeSrcRecordLocked_(obj, &i);
                        src = NULL;
                    } else if(!NixOpenALSource_isOrphan(src)){
                        //remove processed buffers
                        if(src != NULL && !NixOpenALSource_isStatic(src)){
                            ALint csmdAmm = 0;
                            alGetSourceiv(src->idSourceAL, AL_BUFFERS_PROCESSED, &csmdAmm); NIX_OPENAL_ERR_VERIFY("alGetSourceiv(AL_BUFFERS_PROCESSED)");
                            if(csmdAmm > 0){
                                NixMutex_lock(src->queues.mutex);
                                {
                                    NIX_ASSERT(csmdAmm <= src->queues.pend.use) //Just checking, this should be always be true
                                    while(csmdAmm > 0){
                                        if(!NixOpenALSource_pendPopOldestBuffLocked_(src)){
                                            NIX_ASSERT(NIX_FALSE) //program logic error
                                            break;
                                        }
                                        --csmdAmm;
                                    }
                                }
                                NixMutex_unlock(src->queues.mutex);
                            }
                        }
                        //post-process
                        if(src != NULL){
                            //add to notify queue
                            {
                                NixMutex_lock(src->queues.mutex);
                                {
                                    NixOpenALEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
                                }
                                NixMutex_unlock(src->queues.mutex);
                            }
                        }
                    }
                }
            }
            NixMutex_unlock(obj->srcs.mutex);
            //notify (unloked)
            if(notifs.use > 0){
                //NIX_PRINTF_INFO("NixOpenALEngine_tick::notify %d.\n", notifs.use);
                NixUI32 i;
                for(i = 0; i < notifs.use; ++i){
                    STNixSourceNotif* n = &notifs.arr[i];
                    //NIX_PRINTF_INFO("NixOpenALEngine_tick::notify(#%d/%d).\n", i + 1, notifs.use);
                    if(n->callback.func != NULL){
                        (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
                    }
                }
            }
            //NIX_PRINTF_INFO("NixOpenALEngine_tick::NixNotifQueue_destroy.\n");
            NixNotifQueue_destroy(&notifs);
        }
        //recorder
        if(obj->rec != NULL){
            //Note: when the capture is stopped, ALC_CAPTURE_SAMPLES returns the maximun size instead of the captured-only size.
            if(obj->rec->engStarted){
                NixOpenALRecorder_consumeInputBuffer(obj->rec);
            }
            NixOpenALRecorder_notifyBuffers(obj->rec, NIX_FALSE);
        }
    }
}



//------
//QueuePair (Buffers)
//------

void NixOpenALQueuePair_init(STNixOpenALQueuePair* obj){
    memset(obj, 0, sizeof(*obj));
    obj->idBufferAL = NIX_OPENAL_NULL;
}

void NixOpenALQueuePair_destroy(STNixOpenALQueuePair* obj){
    {
        NixBuffer_release(&obj->org);
        NixBuffer_null(&obj->org);
    }
    if(obj->idBufferAL != NIX_OPENAL_NULL){
        alDeleteBuffers(1, &obj->idBufferAL); NIX_OPENAL_ERR_VERIFY("alDeleteBuffers");
        obj->idBufferAL = NIX_OPENAL_NULL;
    }
}

void NixOpenALQueuePair_moveOrg(STNixOpenALQueuePair* obj, STNixOpenALQueuePair* to){
    NixBuffer_set(&to->org, obj->org);
    NixBuffer_release(&obj->org);
    NixBuffer_null(&obj->org);
}

void NixOpenALQueuePair_moveCnv(STNixOpenALQueuePair* obj, STNixOpenALQueuePair* to){
    if(to->idBufferAL != NIX_OPENAL_NULL){
        alDeleteBuffers(1, &to->idBufferAL); NIX_OPENAL_ERR_VERIFY("alDeleteBuffers");
        to->idBufferAL = NIX_OPENAL_NULL;
    }
    to->idBufferAL = obj->idBufferAL;
    obj->idBufferAL = NIX_OPENAL_NULL;
}

//------
//Queue (Buffers)
//------

void NixOpenALQueue_init(STNixContextRef ctx, STNixOpenALQueue* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
}

void NixOpenALQueue_destroy(STNixOpenALQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNixOpenALQueuePair* b = &obj->arr[i];
            NixOpenALQueuePair_destroy(b);
        }
        NixContext_mfree(obj->ctx, obj->arr);
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixOpenALQueue_flush(STNixOpenALQueue* obj){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        if(obj->arr != NULL){
            NixUI32 i; for(i = 0; i < obj->use; i++){
                STNixOpenALQueuePair* b = &obj->arr[i];
                NixOpenALQueuePair_destroy(b);
            }
            obj->use = 0;
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixOpenALQueue_prepareForSz(STNixOpenALQueue* obj, const NixUI32 minSz){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        //resize array (if necesary)
        if(minSz > obj->sz){
            const NixUI32 szN = minSz;
            STNixOpenALQueuePair* arrN = (STNixOpenALQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixOpenALQueuePair) * szN, "NixOpenALQueue_prepareForSz::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //analyze
        if(minSz > obj->sz){
            NIX_PRINTF_ERROR("NixOpenALQueue_prepareForSz failed (no allocated space).\n");
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixOpenALQueue_pushOwning(STNixOpenALQueue* obj, STNixOpenALQueuePair* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNixOpenALQueuePair* arrN = (STNixOpenALQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixOpenALQueuePair) * szN, "NixOpenALQueue_pushOwning::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //add
        if(obj->use >= obj->sz){
            NIX_PRINTF_ERROR("NixOpenALQueue_pushOwning failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixOpenALQueue_popOrphaning(STNixOpenALQueue* obj, STNixOpenALQueuePair* dst){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && obj->use > 0 && dst != NULL){
        *dst = obj->arr[0];
        --obj->use;
        //fill the gap
        {
            NixUI32 i; for(i = 0; i < obj->use; ++i){
                obj->arr[i] = obj->arr[i + 1];
            }
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixOpenALQueue_popMovingTo(STNixOpenALQueue* obj, STNixOpenALQueue* other){
    NixBOOL r = NIX_FALSE;
    STNixOpenALQueuePair pair;
    if(!NixOpenALQueue_popOrphaning(obj, &pair)){
        //
    } else {
        if(!NixOpenALQueue_pushOwning(other, &pair)){
            //program logic error
            NIX_ASSERT(NIX_FALSE);
            NixOpenALQueuePair_destroy(&pair);
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//Source
//------

void NixOpenALSource_init(STNixContextRef ctx, STNixOpenALSource* obj){
    memset(obj, 0, sizeof(STNixOpenALSource));
    NixContext_set(&obj->ctx, ctx);
    obj->idSourceAL = NIX_OPENAL_NULL;
    obj->volume     = 1.f;
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixOpenALQueue_init(ctx, &obj->queues.notify);
        NixOpenALQueue_init(ctx, &obj->queues.pend);
        NixOpenALQueue_init(ctx, &obj->queues.reuse);
    }
}

void NixOpenALSource_destroy(STNixOpenALSource* obj){
    //src
    if(obj->idSourceAL != NIX_OPENAL_NULL){
        alSourceStop(obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
        alDeleteSources(1, &obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alDeleteSources");
        obj->idSourceAL = NIX_OPENAL_NULL;
    }
    //queues
    {
        {
            if(obj->queues.conv.buff.ptr != NULL){
                NixContext_mfree(obj->ctx, obj->queues.conv.buff.ptr);
                obj->queues.conv.buff.ptr = NULL;
            }
            obj->queues.conv.buff.sz = 0;
        }
        if(obj->queues.conv.obj != NULL){
            NixFmtConverter_free(obj->queues.conv.obj);
            obj->queues.conv.obj = NULL;
        }
        NixOpenALQueue_destroy(&obj->queues.pend);
        NixOpenALQueue_destroy(&obj->queues.reuse);
        NixOpenALQueue_destroy(&obj->queues.notify);
        NixMutex_free(&obj->queues.mutex);
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixOpenALSource_queueBufferForOutput(STNixOpenALSource* obj, STNixBufferRef pBuff, const NixBOOL isStream){
    NixBOOL r = NIX_FALSE;
    if(pBuff.ptr != NULL){
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            //error
        } else {
            STNixAudioDesc dataFmt;
            void* data = NULL;
            NixUI32 dataSz = 0;
            if(obj->queues.conv.obj == NULL){
                data = buff->ptr;
                dataSz = buff->use;
                dataFmt = buff->desc;
            } else {
                //populate with converted data
                const NixUI32 buffSamples = (buff->use / buff->desc.blockAlign);
                const NixUI32 buffConvSz = (buffSamples * obj->srcFmt.blockAlign);
                //resize cnv buffer (if necesary)
                if(obj->queues.conv.buff.sz < buffConvSz){
                    void* cnvBuffN = (void*)NixContext_malloc(obj->ctx, buffConvSz, "cnvBuffSzN");
                    if(cnvBuffN != NULL){
                        if(obj->queues.conv.buff.ptr != NULL){
                            NixContext_mfree(obj->ctx, obj->queues.conv.buff.ptr);
                            obj->queues.conv.buff.ptr = NULL;
                        }
                        obj->queues.conv.buff.ptr = cnvBuffN;
                        obj->queues.conv.buff.sz = buffConvSz;
                    }
                }
                //convert
                if(buffConvSz > obj->queues.conv.buff.sz){
                    NIX_PRINTF_ERROR("NixOpenALSource_queueBufferForOutput, could not allocate conversion buffer.\n");
                } else if(!NixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv.obj, &buff->desc, buff->ptr, 0)){
                    NIX_PRINTF_ERROR("NixFmtConverter_setPtrAtSrcInterlaced, failed.\n");
                } else if(!NixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv.obj, &obj->srcFmt, obj->queues.conv.buff.ptr, 0)){
                    NIX_PRINTF_ERROR("NixFmtConverter_setPtrAtDstInterlaced, failed.\n");
                } else {
                    const NixUI32 srcBlocks = (buff->use / buff->desc.blockAlign);
                    const NixUI32 dstBlocks = (obj->queues.conv.buff.sz / obj->srcFmt.blockAlign);
                    NixUI32 ammBlocksRead = 0;
                    NixUI32 ammBlocksWritten = 0;
                    if(!NixFmtConverter_convert(obj->queues.conv.obj, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                        NIX_PRINTF_ERROR("NixOpenALSource_queueBufferForOutput::NixFmtConverter_convert failed from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s).\n"
                                         , obj->buffsFmt.samplerate
                                         , obj->buffsFmt.channels
                                         , obj->buffsFmt.bitsPerSample
                                         , obj->buffsFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->buffsFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                         , obj->srcFmt.samplerate
                                         , obj->srcFmt.channels
                                         , obj->srcFmt.bitsPerSample
                                         , obj->srcFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->srcFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                         );
                    } else {
                        data = obj->queues.conv.buff.ptr;
                        dataSz = ammBlocksWritten * obj->srcFmt.blockAlign;
                        dataFmt = obj->srcFmt;
                    }
                }
            }
            //
            if(data != NULL && dataSz > 0){
                STNixOpenALQueuePair pair;
                NixOpenALQueuePair_init(&pair);
                //reuse or create bufferAL
                {
                    STNixOpenALQueuePair reuse;
                    if(!NixOpenALQueue_popOrphaning(&obj->queues.reuse, &reuse)){
                        //no reusable buffer available, create new
                        ALenum errorAL;
                        alGenBuffers(1, &pair.idBufferAL);
                        if(AL_NONE != (errorAL = alGetError())){
                            NIX_PRINTF_ERROR("alGenBuffers failed: #%d '%s' idBufferAL(%d)\n", errorAL, alGetString(errorAL), pair.idBufferAL);
                            pair.idBufferAL = NIX_OPENAL_NULL;
                        }
                    } else {
                        //reuse buffer
                        NIX_ASSERT(reuse.org.ptr == NULL) //program logic error
                        NIX_ASSERT(reuse.idBufferAL != NIX_OPENAL_NULL) //program logic error
                        if(reuse.idBufferAL == NIX_OPENAL_NULL){
                            NIX_PRINTF_ERROR("NixOpenALSource_queueBufferForOutput::reuse.cnv should not be NULL.\n");
                        } else {
                            pair.idBufferAL = reuse.idBufferAL; reuse.idBufferAL = NIX_OPENAL_NULL; //consume
                        }
                        NixOpenALQueuePair_destroy(&reuse);
                    }
                }
                //populate bufferAL
                if(pair.idBufferAL != NIX_OPENAL_NULL){
                    ALenum errorAL;
                    alBufferData(pair.idBufferAL, obj->srcFmtAL, data, dataSz, dataFmt.samplerate);
                    if(AL_NONE != (errorAL = alGetError())){
                        NIX_PRINTF_ERROR("alBufferData failed: #%d '%s' idBufferAL(%d)\n", errorAL, alGetString(errorAL), pair.idBufferAL);
                        alDeleteBuffers(1, &pair.idBufferAL); NIX_OPENAL_ERR_VERIFY("alDeleteBuffers");
                        pair.idBufferAL = NIX_OPENAL_NULL;
                    }
                }
                //
                if(pair.idBufferAL != NIX_OPENAL_NULL){
                    r = NIX_TRUE;
                    //link buffer to source
                    {
                        if(isStream){
                            //queue buffer
                            ALenum errorAL;
                            alSourceQueueBuffers(obj->idSourceAL, 1, &pair.idBufferAL);
                            if(AL_NONE != (errorAL = alGetError())){
                                r = NIX_FALSE;
                            } else if(NixOpenALSource_isPlaying(obj) && !NixOpenALSource_isPaused(obj)){
                                //start playing if necesary
                                ALint sourceState;
                                alGetSourcei(obj->idSourceAL, AL_SOURCE_STATE, &sourceState);    NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SOURCE_STATE)");
                                if(sourceState != AL_PLAYING){
                                    alSourcePlay(obj->idSourceAL);
                                }
                            }
                        } else {
                            //set buffer
                            ALenum errorAL;
                            alSourcei(obj->idSourceAL, AL_BUFFER, pair.idBufferAL);
                            if(AL_NONE != (errorAL = alGetError())){
                                r = NIX_FALSE;
                            }
                        }
                    }
                    //
                    if(r){
                        NixBuffer_set(&pair.org, pBuff);
                        NixMutex_lock(obj->queues.mutex);
                        {
                            if(!NixOpenALQueue_pushOwning(&obj->queues.pend, &pair)){
                                NIX_PRINTF_ERROR("NixOpenALSource_queueBufferForOutput::NixOpenALQueue_pushOwning failed.\n");
                                r = NIX_FALSE;
                            } else {
                                //added to queue
                                NixOpenALQueue_prepareForSz(&obj->queues.reuse, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                                NixOpenALQueue_prepareForSz(&obj->queues.notify, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                                //this is the first buffer i the queue
                                if(obj->queues.pend.use == 1){
                                    obj->queues.pendBlockIdx = 0;
                                }
                                r = NIX_TRUE;
                            }
                        }
                        NixMutex_unlock(obj->queues.mutex);
                    }
                }
                if(!r){
                    NixOpenALQueuePair_destroy(&pair);
                }
            }
        }
    }
    return r;
}

NixBOOL NixOpenALSource_pendPopOldestBuffLocked_(STNixOpenALSource* obj){
    NixBOOL r = NIX_FALSE;
    if(obj->queues.pend.use > 0){
        STNixOpenALQueuePair pair;
        if(!NixOpenALQueue_popOrphaning(&obj->queues.pend, &pair)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        } else {
            //move "cnv" to reusable queue
            if(pair.idBufferAL != NIX_OPENAL_NULL){
                ALenum errorAL;
                ALuint idBufferAL = pair.idBufferAL;
                alSourceUnqueueBuffers(obj->idSourceAL, 1, &idBufferAL);
                if(AL_NO_ERROR != (errorAL = alGetError())){
                    NIX_PRINTF_ERROR("alSourceUnqueueBuffers failed: #%d '%s' idBufferAL(%d)\n", errorAL, alGetString(errorAL), pair.idBufferAL);
                } else {
                    STNixOpenALQueuePair reuse;
                    NixOpenALQueuePair_init(&reuse);
                    NixOpenALQueuePair_moveCnv(&pair, &reuse);
                    if(!NixOpenALQueue_pushOwning(&obj->queues.reuse, &reuse)){
                        NIX_PRINTF_ERROR("NixOpenALSource_pendPopOldestBuffLocked_::NixOpenALQueue_pushOwning(reuse) failed.\n");
                        NixOpenALQueuePair_destroy(&reuse);
                    }
                }
            }
            //move "org" to notify queue
            if(!NixBuffer_isNull(pair.org)){
                STNixOpenALQueuePair notif;
                NixOpenALQueuePair_init(&notif);
                NixOpenALQueuePair_moveOrg(&pair, &notif);
                if(!NixOpenALQueue_pushOwning(&obj->queues.notify, &notif)){
                    NIX_PRINTF_ERROR("NixOpenALSource_pendPopOldestBuffLocked_::NixOpenALQueue_pushOwning(notify) failed.\n");
                    NixOpenALQueuePair_destroy(&notif);
                }
            }
            NIX_ASSERT(pair.org.ptr == NULL); //program logic error
            NIX_ASSERT(pair.idBufferAL == NIX_OPENAL_NULL); //program logic error
            NixOpenALQueuePair_destroy(&pair);
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixOpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNixOpenALSource* obj){
    NixBOOL r = NIX_TRUE;
    NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
        STNixOpenALQueuePair* pair = &obj->queues.pend.arr[i];
        //move "org" to notify queue
        if(!NixBuffer_isNull(pair->org)){
            STNixOpenALQueuePair notif;
            NixOpenALQueuePair_init(&notif);
            NixOpenALQueuePair_moveOrg(pair, &notif);
            if(!NixOpenALQueue_pushOwning(&obj->queues.notify, &notif)){
                NIX_PRINTF_ERROR("NixOpenALSource_pendPopOldestBuffLocked_::NixOpenALQueue_pushOwning(notify) failed.\n");
                NixOpenALQueuePair_destroy(&notif);
            }
        }
    }
    return r;
}

//------
//Recorder
//------

void NixOpenALRecorder_init(STNixContextRef ctx, STNixOpenALRecorder* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
    obj->idCaptureAL = NIX_OPENAL_NULL;
    //cfg
    {
        //
    }
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixOpenALQueue_init(ctx, &obj->queues.notify);
        NixOpenALQueue_init(ctx, &obj->queues.reuse);
    }
}

void NixOpenALRecorder_destroy(STNixOpenALRecorder* obj){
    //queues
    {
        NixMutex_lock(obj->queues.mutex);
        {
            NixOpenALQueue_destroy(&obj->queues.notify);
            NixOpenALQueue_destroy(&obj->queues.reuse);
            if(obj->queues.conv != NULL){
                NixFmtConverter_free(obj->queues.conv);
                obj->queues.conv = NULL;
            }
            //tmp
            {
                if(obj->queues.filling.tmp != NULL){
                    NixContext_mfree(obj->ctx, obj->queues.filling.tmp);
                    obj->queues.filling.tmp = NULL;
                }
                obj->queues.filling.tmpSz = 0;
            }
        }
        NixMutex_unlock(obj->queues.mutex);
        NixMutex_free(&obj->queues.mutex);
    }
    //
    if(obj->idCaptureAL != NIX_OPENAL_NULL){
        ALenum errorAL;
        alcCaptureStop(obj->idCaptureAL);
        if(AL_NO_ERROR != (errorAL = alGetError())){
            NIX_PRINTF_ERROR("alcCaptureStop failed: #%d '%s'.\n", errorAL, alGetString(errorAL));
        }
        if(!alcCaptureCloseDevice(obj->idCaptureAL)){
            NIX_PRINTF_ERROR("alcCaptureCloseDevice failed\n");
        }
        obj->idCaptureAL = NIX_OPENAL_NULL;
    }
    if(obj->engRef.ptr != NULL){
        STNixOpenALEngine* eng = (STNixOpenALEngine*)NixSharedPtr_getOpq(obj->engRef.ptr);
        if(eng != NULL && eng->rec == obj){
            eng->rec = NULL;
        }
        NixEngine_release(&obj->engRef);
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixOpenALRecorder_prepare(STNixOpenALRecorder* obj, STNixOpenALEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    NixBOOL r = NIX_FALSE;
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.conv == NULL && audioDesc->blockAlign > 0){
        STNixAudioDesc inDesc = STNixAudioDesc_Zero;
        ALenum apiFmt = AL_UNDETERMINED;
        switch(audioDesc->channels){
            case 1:
                switch(audioDesc->bitsPerSample){
                    case 8:
                        apiFmt = AL_FORMAT_MONO8;
                        inDesc.samplesFormat   = ENNixSampleFmt_Int;
                        inDesc.bitsPerSample   = 8;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                    default:
                        apiFmt = AL_FORMAT_MONO16;
                        inDesc.samplesFormat   = ENNixSampleFmt_Int;
                        inDesc.bitsPerSample   = 16;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                }
                break;
            case 2:
                switch(audioDesc->bitsPerSample){
                    case 8:
                        apiFmt = AL_FORMAT_STEREO8;
                        inDesc.samplesFormat   = ENNixSampleFmt_Int;
                        inDesc.bitsPerSample   = 8;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                    default:
                        apiFmt = AL_FORMAT_STEREO16;
                        inDesc.samplesFormat   = ENNixSampleFmt_Int;
                        inDesc.bitsPerSample   = 16;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                }
                break;
                break;
            default:
                break;
        }
        inDesc.blockAlign = (inDesc.bitsPerSample / 8) * inDesc.channels;
        //
        if(apiFmt == AL_UNDETERMINED){
            NIX_PRINTF_ERROR("Unsupported format for OpenAL (2 channels max)\n");
        } else {
            const NixUI32 capPerBuffSz = inDesc.blockAlign * blocksPerBuffer;
            const NixUI32 capMainBuffSz = capPerBuffSz * 2;
            ALenum errAL;
            ALuint capDev = alcCaptureOpenDevice(NULL/*devName*/, inDesc.samplerate, apiFmt, capMainBuffSz);
            if (AL_NO_ERROR != (errAL = alGetError())) {
                NIX_PRINTF_ERROR("alcCaptureOpenDevice failed\n");
            } else {
                //tmp
                {
                    if(obj->queues.filling.tmp != NULL){
                        NixContext_mfree(obj->ctx, obj->queues.filling.tmp);
                        obj->queues.filling.tmp = NULL;
                    }
                    obj->queues.filling.tmpSz = 0;
                }
                NixUI8* tmpBuff = (NixUI8*)NixContext_malloc(obj->ctx, capMainBuffSz, "tmpBuff");
                if(tmpBuff == NULL){
                    NIX_PRINTF_ERROR("NixOpenALRecorder_prepare::allocation of temporary buffer failed.\n");
                } else {
                    void* conv = NixFmtConverter_alloc(obj->ctx);
                    if(!NixFmtConverter_prepare(conv, &inDesc, audioDesc)){
                        NIX_PRINTF_ERROR("NixOpenALRecorder_prepare::NixFmtConverter_prepare failed.\n");
                        NixFmtConverter_free(conv);
                        conv = NULL;
                    } else {
                        //allocate reusable buffers
                        while(obj->queues.reuse.use < buffersCount){
                            STNixOpenALQueuePair pair;
                            NixOpenALQueuePair_init(&pair);
                            pair.org = (eng->apiItf.buffer.alloc)(obj->ctx, audioDesc, NULL, audioDesc->blockAlign * blocksPerBuffer);
                            if(pair.org.ptr == NULL){
                                NIX_PRINTF_ERROR("NixOpenALRecorder_prepare::pair.org allocation failed.\n");
                                NixOpenALQueuePair_destroy(&pair);
                                break;
                            } else if(!NixOpenALQueue_pushOwning(&obj->queues.reuse, &pair)){
                                NixOpenALQueuePair_destroy(&pair);
                                break;
                            }
                        }
                        //
                        if(obj->queues.reuse.use <= 0){
                            NIX_PRINTF_ERROR("NixOpenALRecorder_prepare::no reusable buffer could be allocated.\n");
                        } else {
                            //prepared
                            obj->queues.filling.iCurSample = 0;
                            obj->queues.conv = conv; conv = NULL; //consume
                            obj->queues.filling.tmp = tmpBuff; tmpBuff = NULL; //consume
                            obj->queues.filling.tmpSz = capMainBuffSz;
                            //cfg
                            obj->cfg.fmt = *audioDesc;
                            obj->cfg.maxBuffers = buffersCount;
                            obj->cfg.blocksPerBuffer = blocksPerBuffer;
                            //
                            obj->capFmt = inDesc;
                            r = NIX_TRUE;
                        }
                    }
                    //release (if not consumed)
                    if(conv != NULL){
                        NixFmtConverter_free(conv);
                        conv = NULL;
                    }
                }
                //release (if not consumed)
                if(tmpBuff != NULL){
                    NixContext_mfree(obj->ctx, tmpBuff);
                    tmpBuff = NULL;
                }
                obj->idCaptureAL = capDev;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

NixBOOL NixOpenALRecorder_setCallback(STNixOpenALRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    {
        obj->callback.func = callback;
        obj->callback.data = callbackData;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixOpenALRecorder_start(STNixOpenALRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(!obj->engStarted){
        if(obj->idCaptureAL != NIX_OPENAL_NULL){
            ALenum errorAL;
            alcCaptureStart(obj->idCaptureAL);
            if(AL_NO_ERROR != (errorAL = alGetError())){
                NIX_PRINTF_ERROR("alcCaptureStart failed: #%d '%s'.\n", errorAL, alGetString(errorAL));
                r = NIX_FALSE;
            } else {
                obj->engStarted = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL NixOpenALRecorder_stop(STNixOpenALRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(obj->idCaptureAL != NIX_OPENAL_NULL){
        ALenum errorAL;
        alcCaptureStop(obj->idCaptureAL);
        if(AL_NO_ERROR != (errorAL = alGetError())){
            NIX_PRINTF_ERROR("alcCaptureStop failed: #%d '%s'.\n", errorAL, alGetString(errorAL));
        }
        obj->engStarted = NIX_FALSE;
    }
    NixOpenALRecorder_flush(obj);
    return r;
}

NixBOOL NixOpenALRecorder_flush(STNixOpenALRecorder* obj){
    NixBOOL r = NIX_TRUE;
    //move filling buffer to notify (if data is available)
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.reuse.use > 0){
        STNixOpenALQueuePair* pair = &obj->queues.reuse.arr[0];
        if(!NixBuffer_isNull(pair->org) && ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr))->use > 0){
            obj->queues.filling.iCurSample = 0;
            if(!NixOpenALQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                //program logic error
                r = NIX_FALSE;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

void NixOpenALRecorder_consumeInputBuffer(STNixOpenALRecorder* obj){
    if(obj->queues.conv != NULL && obj->idCaptureAL != NIX_OPENAL_NULL){
        NixUI32 inIdx = 0;
        ALCint inSz = 0;
        //get samples
        {
            ALenum errorAL;
            alcGetIntegerv(obj->idCaptureAL, ALC_CAPTURE_SAMPLES, (ALCsizei)sizeof(inSz), &inSz);
            if(AL_NO_ERROR != (errorAL = alGetError())){
                NIX_PRINTF_ERROR("alcGetIntegerv(ALC_CAPTURE_SAMPLES) failed with error #%d\n", (NixSI32)errorAL, alGetString(errorAL));
                inSz = 0;
            } else {
                NIX_ASSERT(inSz >= 0)
                const NixUI32 tmpSzInSamples = (obj->queues.filling.tmpSz / obj->capFmt.blockAlign);
                if(inSz > tmpSzInSamples){
                    inSz = tmpSzInSamples;
                }
                if(inSz > 0){
                    alcCaptureSamples(obj->idCaptureAL, (ALCvoid *)obj->queues.filling.tmp, inSz);
                    if(AL_NO_ERROR != (errorAL = alGetError())){
                        NIX_PRINTF_ERROR("alcCaptureSamples failed with error #%d\n", (NixSI32)errorAL, alGetString(errorAL));
                        inSz = 0;
                    } else {
                        //
                    }
                }
            }
        }
        NixMutex_lock(obj->queues.mutex);
        {
            //process
            while(inIdx < inSz){
                if(obj->queues.reuse.use <= 0){
                    //move oldest-notify buffer to reuse
                    if(!NixOpenALQueue_popMovingTo(&obj->queues.notify, &obj->queues.reuse)){
                        //program logic error
                        NIX_ASSERT(NIX_FALSE);
                        break;
                    }
                } else {
                    STNixOpenALQueuePair* pair = &obj->queues.reuse.arr[0];
                    if(NixBuffer_isNull(pair->org) || ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr))->desc.blockAlign <= 0){
                        //just remove
                        STNixOpenALQueuePair pair;
                        if(!NixOpenALQueue_popOrphaning(&obj->queues.reuse, &pair)){
                            NIX_ASSERT(NIX_FALSE);
                            //program logic error
                            break;
                        }
                        NixOpenALQueuePair_destroy(&pair);
                    } else {
                        STNixPCMBuffer* org = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
                        const NixUI32 outSz = (org->sz / org->desc.blockAlign);
                        const NixUI32 outAvail = (obj->queues.filling.iCurSample >= outSz ? 0 : outSz - obj->queues.filling.iCurSample);
                        const NixUI32 inAvail = inSz - inIdx;
                        NixUI32 ammBlocksRead = 0, ammBlocksWritten = 0;
                        if(outAvail > 0 && inAvail > 0){
                            //dst
                            NixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv, &org->desc, org->ptr, obj->queues.filling.iCurSample);
                            //src
                            NixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &obj->capFmt, obj->queues.filling.tmp, inIdx);
                            //convert
                            if(!NixFmtConverter_convert(obj->queues.conv, inAvail, outAvail, &ammBlocksRead, &ammBlocksWritten)){
                                //error
                                break;
                            } else if(ammBlocksRead == 0 && ammBlocksWritten == 0){
                                //converter did nothing, avoid infinite cycle
                                break;
                            } else {
                                inIdx += ammBlocksRead;
                                obj->queues.filling.iCurSample += ammBlocksWritten;
                                org->use = (obj->queues.filling.iCurSample * org->desc.blockAlign); NIX_ASSERT(org->use <= org->sz)
                            }
                        }
                        //move reusable buffer to notify
                        if(ammBlocksWritten == outAvail){
                            obj->queues.filling.iCurSample = 0;
                            if(!NixOpenALQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                                //program logic error
                                NIX_ASSERT(NIX_FALSE);
                                break;
                            }
                        }
                    }
                }
            }
        }
        NixMutex_unlock(obj->queues.mutex);
    }
}

void NixOpenALRecorder_notifyBuffers(STNixOpenALRecorder* obj, const NixBOOL discardWithoutNotifying){
    NixMutex_lock(obj->queues.mutex);
    {
        const NixUI32 maxProcess = obj->queues.notify.use;
        NixUI32 ammProcessed = 0;
        while(ammProcessed < maxProcess && obj->queues.notify.use > 0){
            STNixOpenALQueuePair pair;
            if(!NixOpenALQueue_popOrphaning(&obj->queues.notify, &pair)){
                NIX_ASSERT(NIX_FALSE);
                //program logic error
                break;
            } else {
                //notify (unlocked)
                if(!discardWithoutNotifying && !NixBuffer_isNull(pair.org) && ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair.org.ptr))->desc.blockAlign > 0 && obj->callback.func != NULL){
                    STNixPCMBuffer* org = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair.org.ptr);
                    NixMutex_unlock(obj->queues.mutex);
                    {
                        (*obj->callback.func)(&obj->engRef, &obj->selfRef, org->desc, org->ptr, org->use, (org->use / org->desc.blockAlign), obj->callback.data);
                    }
                    NixMutex_lock(obj->queues.mutex);
                }
                //move to reuse
                if(!NixOpenALQueue_pushOwning(&obj->queues.reuse, &pair)){
                    //program logic error
                    NIX_ASSERT(NIX_FALSE);
                    NixOpenALQueuePair_destroy(&pair);
                }
            }
            //processed
            ++ammProcessed;
        }
    }
    NixMutex_unlock(obj->queues.mutex);
}

//------
//Engine API
//------

STNixEngineRef nixOpenALEngine_alloc(STNixContextRef ctx){
    STNixEngineRef r = STNixEngineRef_Zero;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixContext_malloc(ctx, sizeof(STNixOpenALEngine), "STNixOpenALEngine");
    if(obj != NULL){
        NixOpenALEngine_init(ctx, obj);
        obj->deviceAL = alcOpenDevice(NULL);
        if(obj->deviceAL == NULL){
            NIX_PRINTF_ERROR("OpenAL::alcOpenDevice failed.\n");
            obj->deviceAL = NIX_OPENAL_NULL;
        } else {
            obj->contextAL = alcCreateContext(obj->deviceAL, NULL);
            if(obj->contextAL == NULL){
                NIX_PRINTF_ERROR("OpenAL::alcCreateContext failed\n");
                obj->contextAL = NIX_OPENAL_NULL;
            } else {
                if(alcMakeContextCurrent(obj->contextAL) == AL_FALSE){
                    NIX_PRINTF_ERROR("OpenAL::alcMakeContextCurrent failed\n");
                } else if(NULL == (r.ptr = NixSharedPtr_alloc(ctx.itf, obj, "nixOpenALEngine_alloc"))){
                    NIX_PRINTF_ERROR("nixAAudioEngine_create::NixSharedPtr_alloc failed.\n");
                } else {
                    obj->contextALIsCurrent = NIX_TRUE;
                    //Masc of capabilities
                    obj->maskCapabilities   |= (alcIsExtensionPresent(obj->deviceAL, "ALC_EXT_CAPTURE") != ALC_FALSE || alcIsExtensionPresent(obj->deviceAL, "ALC_EXT_capture") != ALC_FALSE) ? NIX_CAP_AUDIO_CAPTURE : 0;
                    obj->maskCapabilities   |= (alIsExtensionPresent("AL_EXT_STATIC_BUFFER") != AL_FALSE) ? NIX_CAP_AUDIO_STATIC_BUFFERS : 0;
                    obj->maskCapabilities   |= (alIsExtensionPresent("AL_EXT_OFFSET") != AL_FALSE) ? NIX_CAP_AUDIO_SOURCE_OFFSETS : 0;
                    //
                    r.itf = &obj->apiItf.engine;
                    obj = NULL; //consume
                }
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixOpenALEngine_destroy(obj);
            NixContext_mfree(ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixOpenALEngine_free(STNixEngineRef pObj){
    if(pObj.ptr != NULL){
        STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixOpenALEngine_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

void nixOpenALEngine_printCaps(STNixEngineRef pObj){
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL && obj->deviceAL != NIX_OPENAL_NULL){
        ALCint versionMayorALC, versionMenorALC;
        const char* strAlVersion;
        const char* strAlRenderer;
        const char* strAlVendor;
        const char* strAlExtensions;
        const char* strAlcExtensions;
        const char* defDeviceName;
        strAlVersion        = alGetString(AL_VERSION);    NIX_OPENAL_ERR_VERIFY("alGetString(AL_VERSION)");
        strAlRenderer       = alGetString(AL_RENDERER);    NIX_OPENAL_ERR_VERIFY("alGetString(AL_RENDERER)");
        strAlVendor         = alGetString(AL_VENDOR);    NIX_OPENAL_ERR_VERIFY("alGetString(AL_VENDOR)");
        strAlExtensions     = alGetString(AL_EXTENSIONS);    NIX_OPENAL_ERR_VERIFY("alGetString(AL_EXTENSIONS)");
        strAlcExtensions    = alcGetString(obj->deviceAL, ALC_EXTENSIONS); NIX_OPENAL_ERR_VERIFY("alcGetString(ALC_EXTENSIONS)");
        alcGetIntegerv(obj->deviceAL, ALC_MAJOR_VERSION, (ALCsizei)sizeof(versionMayorALC), &versionMayorALC);
        alcGetIntegerv(obj->deviceAL, ALC_MINOR_VERSION, (ALCsizei)sizeof(versionMenorALC), &versionMenorALC);
        //
        printf("----------- OPENAL -------------\n");
        printf("Version:          AL('%s') ALC(%d.%d):\n", strAlVersion, versionMayorALC, versionMenorALC);
        printf("Renderizador:     '%s'\n", strAlRenderer);
        printf("Vendedor:         '%s'\n", strAlVendor);
        printf("EXTCaptura:       %s\n", (obj->maskCapabilities & NIX_CAP_AUDIO_CAPTURE)?"supported":"unsupported");
        printf("EXTBuffEstaticos: %s\n", (obj->maskCapabilities & NIX_CAP_AUDIO_STATIC_BUFFERS)?"supported":"unsupported");
        printf("EXTOffsets:       %s\n", (obj->maskCapabilities & NIX_CAP_AUDIO_SOURCE_OFFSETS)?"supported":"unsupported");
        printf("Extensions AL:    '%s'\n", strAlExtensions);
        printf("Extensions ALC:   '%s'\n", strAlcExtensions);
        //List sound devices
        defDeviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER); NIX_OPENAL_ERR_VERIFY("alcGetString(ALC_DEFAULT_DEVICE_SPECIFIER)")
        printf("DefautlDevice:    '%s'\n", defDeviceName);
        {
            NixSI32 pos = 0, deviceCount = 0;
            const ALCchar* deviceList = alcGetString(NULL, ALC_DEVICE_SPECIFIER); NIX_OPENAL_ERR_VERIFY("alcGetString(ALC_DEVICE_SPECIFIER)")
            while(deviceList[pos]!='\0'){
                const char* strDevice = &(deviceList[pos]); NixUI32 strSize = 0;
                while(strDevice[strSize]!='\0') strSize++;
                pos += strSize + 1; deviceCount++;
                printf("Device #%d:        '%s'\n", deviceCount, strDevice);
            }
        }
        //List capture devices
        if(obj->maskCapabilities & NIX_CAP_AUDIO_CAPTURE){
            const char* defCaptureDeviceName = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER); NIX_OPENAL_ERR_VERIFY("alcGetString(ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER)")
            {
                NixSI32 pos = 0, deviceCount = 0;
                const ALCchar* deviceList = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER); NIX_OPENAL_ERR_VERIFY("alcGetString(ALC_CAPTURE_DEVICE_SPECIFIER)")
                while(deviceList[pos]!='\0'){
                    const char* strDevice = &(deviceList[pos]); NixUI32 strSize = 0;
                    while(strDevice[strSize]!='\0') strSize++;
                    pos += strSize + 1; deviceCount++;
                    printf("Capture #%d:       '%s'\n", deviceCount, strDevice);
                }
            }
            printf("DefautlCapture:   '%s'\n", defCaptureDeviceName);
        }
        
    }
}

NixBOOL nixOpenALEngine_ctxIsActive(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = obj->contextALIsCurrent;
    }
    return r;
}

NixBOOL nixOpenALEngine_ctxActivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        if(alcMakeContextCurrent(obj->contextAL) == AL_FALSE){
            NIX_PRINTF_ERROR("OpenAL::alcMakeContextCurrent(context) failed\n");
        } else {
            obj->contextALIsCurrent = NIX_TRUE;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL nixOpenALEngine_ctxDeactivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        if(alcMakeContextCurrent(NULL) == AL_FALSE){
            NIX_PRINTF_ERROR("OpenAL::alcMakeContextCurrent(NULL) failed\n");
        } else {
            obj->contextALIsCurrent = NIX_FALSE;
            r = NIX_TRUE;
        }
    }
    return r;
}

void nixOpenALEngine_tick(STNixEngineRef pObj){
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        NixOpenALEngine_tick(obj, NIX_FALSE);
    }
}

//Factory

STNixSourceRef nixOpenALEngine_allocSource(STNixEngineRef ref){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.source.alloc != NULL){
        r = (*obj->apiItf.source.alloc)(ref);
    }
    return r;
}

STNixBufferRef nixOpenALEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixBufferRef r = STNixBufferRef_Zero;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.buffer.alloc != NULL){
        r = (*obj->apiItf.buffer.alloc)(obj->ctx, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

STNixRecorderRef nixOpenALEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixOpenALEngine* obj = (STNixOpenALEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.recorder.alloc != NULL){
        r = (*obj->apiItf.recorder.alloc)(ref, audioDesc, buffersCount, blocksPerBuffer);
    }
    return r;
}

//------
//Source API
//------

STNixSourceRef nixOpenALSource_alloc(STNixEngineRef pEng){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixOpenALEngine* eng = (STNixOpenALEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng == NULL){
        NIX_PRINTF_ERROR("nixOpenALSource_alloc::NixSharedPtr_getOpq returned NULL\n");
    } else {
        STNixOpenALSource* obj = (STNixOpenALSource*)NixContext_malloc(eng->ctx, sizeof(STNixOpenALSource), "STNixOpenALSource");
        if(obj == NULL){
            NIX_PRINTF_ERROR("nixOpenALSource_alloc::NixContext_malloc returned NULL\n");
        } else {
            NixOpenALSource_init(eng->ctx, obj);
            obj->eng = eng;
            //
            ALenum errorAL;
            alGenSources(1, &obj->idSourceAL);
            if(AL_NO_ERROR != (errorAL = alGetError())){
                NIX_PRINTF_ERROR("alGenSources failed with error #%d\n", (NixSI32)errorAL);
                obj->idSourceAL = NIX_OPENAL_NULL;
            } else {
                obj->idSourceAL = obj->idSourceAL;
                //add to engine
                if(!NixOpenALEngine_srcsAdd(eng, obj)){
                    NIX_PRINTF_ERROR("nixOpenALSource_create::NixOpenALEngine_srcsAdd failed.\n");
                } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "nixOpenALSource_alloc"))){
                    NIX_PRINTF_ERROR("nixAAudioEngine_create::NixSharedPtr_alloc failed.\n");
                } else {
                    r.itf = &eng->apiItf.source;
                    obj->self = r;
                    obj = NULL; //consume
                }
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixOpenALSource_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixOpenALSource_removeAllBuffersAndNotify_(STNixOpenALSource* obj){
    STNixNotifQueue notifs;
    NixNotifQueue_init(obj->ctx, &notifs);
    //move all pending buffers to notify
    NixMutex_lock(obj->queues.mutex);
    {
        NixOpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(obj);
        NixOpenALEngine_tick_addQueueNotifSrcLocked_(&notifs, obj);
    }
    NixMutex_unlock(obj->queues.mutex);
    //notify
    {
        NixUI32 i;
        for(i = 0; i < notifs.use; ++i){
            STNixSourceNotif* n = &notifs.arr[i];
            NIX_PRINTF_INFO("nixOpenALSource_removeAllBuffersAndNotify_::notify(#%d/%d).\n", i + 1, notifs.use);
            if(n->callback.func != NULL){
                (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
            }
        }
    }
    NixNotifQueue_destroy(&notifs);
}

void nixOpenALSource_free(STNixSourceRef pObj){ //orphans the source, will automatically be destroyed after internal cleanup
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            //set final state
            {
                //nullify self-reference before notifying
                //to avoid reviving this object during final notification.
                NixSource_null(&obj->self);
                //Flag as orphan, for cleanup inside 'tick'
                NixOpenALSource_setIsOrphan(obj); //source is waiting for close(), wait for the change of state and NixOpenALSource_release + free.
                NixOpenALSource_setIsPlaying(obj, NIX_FALSE);
                NixOpenALSource_setIsPaused(obj, NIX_FALSE);
            }
            //flush all pending buffers
            {
                //close
                if(obj->idSourceAL != NIX_OPENAL_NULL){
                    alSourceStop(obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
                }
                nixOpenALSource_removeAllBuffersAndNotify_(obj);
            }
        }
    }
}

void nixOpenALSource_setCallback(STNixSourceRef pObj, NixSourceCallbackFnc callback, void* callbackData){
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        obj->queues.callback.func = callback;
        obj->queues.callback.data = callbackData;
    }
}

NixBOOL nixOpenALSource_setVolume(STNixSourceRef pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        obj->volume = vol;
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcef(obj->idSourceAL, AL_GAIN, vol);
            NIX_OPENAL_ERR_VERIFY("alSourcef(AL_GAIN)");
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixOpenALSource_setRepeat(STNixSourceRef pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixOpenALSource_setIsRepeat(obj, isRepeat);
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcei(obj->idSourceAL, AL_LOOPING, isRepeat ? AL_TRUE : AL_FALSE); NIX_OPENAL_ERR_VERIFY("alSourcei(AL_LOOPING)");
        }
        r = NIX_TRUE;
    }
    return r;
}

void nixOpenALSource_play(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcePlay(obj->idSourceAL);    NIX_OPENAL_ERR_VERIFY("alSourcePlay");
        }
        NixOpenALSource_setIsPlaying(obj, NIX_TRUE);
        NixOpenALSource_setIsPaused(obj, NIX_FALSE);
    }
}

void nixOpenALSource_pause(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->idSourceAL != NULL){
            alSourcePause(obj->idSourceAL);    NIX_OPENAL_ERR_VERIFY("alSourcePause");
        }
        NixOpenALSource_setIsPaused(obj, NIX_TRUE);
    }
}

void nixOpenALSource_stop(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourceStop(obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
        }
        NixOpenALSource_setIsPlaying(obj, NIX_FALSE);
        NixOpenALSource_setIsPaused(obj, NIX_FALSE);
        //flush all pending buffers
        nixOpenALSource_removeAllBuffersAndNotify_(obj);
    }
}

NixBOOL nixOpenALSource_isPlaying(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            ALint sourceState;
            alGetSourcei(obj->idSourceAL, AL_SOURCE_STATE, &sourceState);    NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SOURCE_STATE)");
            r = sourceState == AL_PLAYING ? NIX_TRUE : NIX_FALSE;
        }
    }
    return r;
}

NixBOOL nixOpenALSource_isPaused(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixOpenALSource_isPlaying(obj) && NixOpenALSource_isPaused(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixBOOL nixOpenALSource_isRepeat(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixOpenALSource_isRepeat(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixFLOAT nixOpenALSource_getVolume(STNixSourceRef pObj){
    NixFLOAT r = 0.f;
    if(pObj.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = obj->volume;
    }
    return r;
}

ALenum nixOpenALSource_alFormat(const STNixAudioDesc* fmt){
    ALenum dataFormat = AL_UNDETERMINED;
    switch(fmt->bitsPerSample){
        case 8:
            switch(fmt->channels){
                case 1:
                    dataFormat = AL_FORMAT_MONO8;
                    break;
                case 2:
                    dataFormat = AL_FORMAT_STEREO8;
                    break;
            }
            break;
        case 16:
            switch(fmt->channels){
                case 1:
                    dataFormat = AL_FORMAT_MONO16;
                    break;
                case 2:
                    dataFormat = AL_FORMAT_STEREO16;
                    break;
            }
            break;
        default:
            break;
    }
    return dataFormat;
}

NixBOOL nixOpenALSource_prepareSourceForFmtAndSz_(STNixOpenALSource* obj, const STNixAudioDesc* fmt, const NixUI32 buffSz){
    NixBOOL r = NIX_FALSE;
    if(fmt != NULL && fmt->blockAlign > 0 && fmt->bitsPerSample > 0 && fmt->channels > 0 && fmt->samplerate > 0 && fmt->samplesFormat > ENNixSampleFmt_Unknown && fmt->samplesFormat <= ENNixSampleFmt_Count){
        ALenum fmtAL = nixOpenALSource_alFormat(fmt);
        if(fmtAL != AL_UNDETERMINED){
            //buffer format cmpatible qith OpenAL
            obj->buffsFmt   = *fmt;
            obj->srcFmtAL   = fmtAL;
            obj->srcFmt     = *fmt;
            r = NIX_TRUE;
        } else {
            //prepare converter
            void* conv = NixFmtConverter_alloc(obj->ctx);
            STNixAudioDesc convFmt = STNixAudioDesc_Zero;
            convFmt.channels        = fmt->channels;
            convFmt.bitsPerSample   = 16;
            convFmt.samplerate      = fmt->samplerate;
            convFmt.samplesFormat   = ENNixSampleFmt_Int;
            convFmt.blockAlign      = (convFmt.bitsPerSample / 8) * convFmt.channels;
            if(!NixFmtConverter_prepare(conv, fmt, &convFmt)){
                NIX_PRINTF_ERROR("nixOpenALSource_prepareSourceForFmtAndSz_, NixFmtConverter_prepare failed.\n");
            } else {
                fmtAL = nixOpenALSource_alFormat(&convFmt);
                if(fmtAL == AL_UNDETERMINED){
                    NIX_PRINTF_ERROR("nixOpenALSource_prepareSourceForFmtAndSz_, NixFmtConverter_prepare sucess but OpenAL unsupported format.\n");
                } else {
                    //allocate conversion buffer
                    const NixUI32 srcSamples = buffSz / fmt->blockAlign * fmt->blockAlign;
                    const NixUI32 cnvBytes = srcSamples * convFmt.blockAlign;
                    NIX_ASSERT(obj->queues.conv.buff.ptr == NULL && obj->queues.conv.buff.sz == 0)
                    obj->queues.conv.buff.ptr = (void*)NixContext_malloc(obj->ctx, cnvBytes, "obj->queues.conv.buff.ptr");
                    if(obj->queues.conv.buff.ptr != NULL){
                        obj->queues.conv.buff.sz = cnvBytes;
                        obj->queues.conv.obj = conv; conv = NULL;
                        obj->buffsFmt   = *fmt;
                        obj->srcFmtAL   = fmtAL;
                        obj->srcFmt     = convFmt;
                        r = NIX_TRUE;
                    }
                }
            }
            //release (if not consumed)
            if(conv != NULL){
                NixFmtConverter_free(conv);
                conv = NULL;
            }
        }
    }
    return r;
}
    
NixBOOL nixOpenALSource_setBuffer(STNixSourceRef pObj, STNixBufferRef pBuff){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixOpenALSource* obj    = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(obj->buffsFmt.blockAlign == 0){
            if(!nixOpenALSource_prepareSourceForFmtAndSz_(obj, &buff->desc, buff->sz)){
                NIX_PRINTF_ERROR("nixOpenALSource_setBuffer::nixOpenALSource_prepareSourceForFmtAndSz_ failed.\n");
            }
        }
        //apply
        if(obj->idSourceAL == NIX_OPENAL_NULL){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, no source available.\n");
        } else if(obj->queues.pend.use != 0){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, source already has buffer.\n");
        } else if(NixOpenALSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, source is already static.\n");
        } else if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            NixOpenALSource_setIsStatic(obj, NIX_TRUE);
            //schedule
            NixBOOL isStream = NIX_FALSE;
            if(!NixOpenALSource_queueBufferForOutput(obj, pBuff, isStream)){
                NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, NixOpenALSource_queueBufferForOutput failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixOpenALSource_queueBuffer(STNixSourceRef pObj, STNixBufferRef pBuff){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixOpenALSource* obj    = (STNixOpenALSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(obj->buffsFmt.blockAlign == 0){
            if(!nixOpenALSource_prepareSourceForFmtAndSz_(obj, &buff->desc, buff->sz)){
                //error
            }
        }
        //
        if(obj->idSourceAL == NIX_OPENAL_NULL){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, no source available.\n");
        } else if(NixOpenALSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, source is static.\n");
        } else if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            //schedule
            NixBOOL isStream = NIX_TRUE;
            if(!NixOpenALSource_queueBufferForOutput(obj, pBuff, isStream)){
                NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, NixOpenALSource_queueBufferForOutput failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixOpenALSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset){ //relative to first buffer in queue
    NixBOOL r = NIX_FALSE;
    if(ref.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->queues.pend.use > 0){
            STNixOpenALQueuePair* pair = &obj->queues.pend.arr[0];
            STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
            if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                switch (type) {
                    case ENNixOffsetType_Blocks:
                        obj->queues.pendBlockIdx = offset;
                        r = NIX_TRUE;
                        break;
                    case ENNixOffsetType_Msecs:
                        obj->queues.pendBlockIdx = offset * buff->desc.samplerate / 1000;
                        r = NIX_TRUE;
                        break;
                    case ENNixOffsetType_Bytes:
                        obj->queues.pendBlockIdx = offset / buff->desc.blockAlign;
                        r = NIX_TRUE;
                        break;
                    default:
                        break;
                }
            }
        }
        NixMutex_unlock(obj->queues.mutex);
    }
    return r;
}

NixUI32 nixOpenALSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){   //all buffer queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        {
            NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
                STNixOpenALQueuePair* pair = &obj->queues.pend.arr[0];
                STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
                if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                    const NixUI32 blocks = buff->use / buff->desc.blockAlign;
                    bytesCount += buff->use;
                    blocksCount += blocks;
                    msecsCount += blocks * 1000 / buff->desc.samplerate;
                    r++;
                }
            }
        }
        NixMutex_unlock(obj->queues.mutex);
    }
    if(optDstBytesCount != NULL) *optDstBytesCount = bytesCount;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = blocksCount;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = msecsCount;
    return r;
}

NixUI32 nixOpenALSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){  //relative to first buffer in queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixOpenALSource* obj = (STNixOpenALSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->idSourceAL != NIX_OPENAL_NULL && obj->queues.pend.use > 0 && obj->srcFmt.samplerate > 0){
            ALint processedBuffers = 0, sampleOffset = 0;
            alGetSourcei(obj->idSourceAL, AL_BUFFERS_PROCESSED, &processedBuffers);
            alGetSourcei(obj->idSourceAL, AL_SAMPLE_OFFSET, &sampleOffset);
            if(processedBuffers <= obj->queues.pend.use){
                STNixOpenALQueuePair* pair = &obj->queues.pend.arr[processedBuffers];
                STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
                if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                    blocksCount = (sampleOffset / buff->desc.channels) * buff->desc.samplerate / obj->srcFmt.samplerate;
                    bytesCount  = blocksCount * buff->desc.blockAlign;
                    msecsCount  = (blocksCount * 1000 / buff->desc.samplerate);
                    r = blocksCount;
                }
            }
        }
        NixMutex_unlock(obj->queues.mutex);
    }
    if(optDstBytesCount != NULL) *optDstBytesCount = bytesCount;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = blocksCount;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = msecsCount;
    return r;
}

//------
//Recorder API
//------

STNixRecorderRef nixOpenALRecorder_alloc(STNixEngineRef pEng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixOpenALEngine* eng = (STNixOpenALEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0 && audioDesc->blockAlign > 0 && eng->rec == NULL){
        STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixContext_malloc(eng->ctx, sizeof(STNixOpenALRecorder), "STNixOpenALRecorder");
        if(obj != NULL){
            NixOpenALRecorder_init(eng->ctx, obj);
            if(!NixOpenALRecorder_prepare(obj, eng, audioDesc, buffersCount, blocksPerBuffer)){
                NIX_PRINTF_ERROR("NixOpenALRecorder_create, NixOpenALRecorder_prepare failed.\n");
            } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "nixOpenALRecorder_alloc"))){
                NIX_PRINTF_ERROR("nixAAudioEngine_create::NixSharedPtr_alloc failed.\n");
            } else {
                r.itf           = &eng->apiItf.recorder;
                obj->engRef     = pEng; NixEngine_retain(pEng);
                obj->selfRef    = r;
                eng->rec        = obj; obj = NULL; //consume
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixOpenALRecorder_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixOpenALRecorder_free(STNixRecorderRef ref){
    if(ref.ptr != NULL){
        STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
        NixSharedPtr_free(ref.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixOpenALRecorder_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

NixBOOL nixOpenALRecorder_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = NixOpenALRecorder_setCallback(obj, callback, callbackData);
    }
    return r;
}

NixBOOL nixOpenALRecorder_start(STNixRecorderRef ref){
    NixBOOL r = NIX_FALSE;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = NixOpenALRecorder_start(obj);
    }
    return r;
}

NixBOOL nixOpenALRecorder_stop(STNixRecorderRef ref){
    NixBOOL r = NIX_FALSE;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = NixOpenALRecorder_stop(obj);
    }
    return r;
}

NixBOOL nixOpenALRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying){
    NixBOOL r = NIX_FALSE;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        if(includeCurrentPartialBuff){
            NixOpenALRecorder_flush(obj);
        }
        NixOpenALRecorder_notifyBuffers(obj, discardWithoutNotifying);
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixOpenALRecorder_isCapturing(STNixRecorderRef ref){
    NixBOOL r = NIX_FALSE;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = obj->engStarted;
    }
    return r;
}

NixUI32 nixOpenALRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    STNixOpenALRecorder* obj = (STNixOpenALRecorder*)NixSharedPtr_getOpq(ref.ptr);
    //move samples to filled buffer
    if(obj->engStarted){
        NixOpenALRecorder_consumeInputBuffer(obj);
    }
    //calculate filled buffers
    NixMutex_lock(obj->queues.mutex);
    {
        NixUI32 i; for(i = 0; i < obj->queues.notify.use; i++){
            STNixOpenALQueuePair* pair = &obj->queues.notify.arr[0];
            STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
            if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                const NixUI32 blocks = buff->use / buff->desc.blockAlign;
                bytesCount += buff->use;
                blocksCount += blocks;
                msecsCount += blocks * 1000 / buff->desc.samplerate;
                r++;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    if(optDstBytesCount != NULL) *optDstBytesCount = bytesCount;
    if(optDstBlocksCount != NULL) *optDstBlocksCount = blocksCount;
    if(optDstMsecsCount != NULL) *optDstMsecsCount = msecsCount;
    return r;
}
