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
//This file adds support for AAudio for playing and recording in Android 8+ (API 26+, Oreo)
//

#include "nixtla-audio-private.h"
#include "nixtla-audio.h"
#include "nixtla-aaudio.h"
#include <string.h> //for memset()

#ifdef __ANDROID__
#   include <aaudio/AAudio.h>
#endif

//------
//API Itf
//------

//AAudio interface (Android 8.0+, Oreo, API level 26+)

//Engine
STNixEngineRef  nixAAudioEngine_alloc(STNixContextRef ctx);
void            nixAAudioEngine_free(STNixEngineRef ref);
void            nixAAudioEngine_printCaps(STNixEngineRef ref);
NixBOOL         nixAAudioEngine_ctxIsActive(STNixEngineRef ref);
NixBOOL         nixAAudioEngine_ctxActivate(STNixEngineRef ref);
NixBOOL         nixAAudioEngine_ctxDeactivate(STNixEngineRef ref);
void            nixAAudioEngine_tick(STNixEngineRef ref);
//Factory
STNixSourceRef  nixAAudioEngine_allocSource(STNixEngineRef ref);
STNixBufferRef  nixAAudioEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
STNixRecorderRef nixAAudioEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
//Source
STNixSourceRef  nixAAudioSource_alloc(STNixEngineRef eng);
void            nixAAudioSource_free(STNixSourceRef ref);
void            nixAAudioSource_setCallback(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData);
NixBOOL         nixAAudioSource_setVolume(STNixSourceRef ref, const float vol);
NixBOOL         nixAAudioSource_setRepeat(STNixSourceRef ref, const NixBOOL isRepeat);
void            nixAAudioSource_play(STNixSourceRef ref);
void            nixAAudioSource_pause(STNixSourceRef ref);
void            nixAAudioSource_stop(STNixSourceRef ref);
NixBOOL         nixAAudioSource_isPlaying(STNixSourceRef ref);
NixBOOL         nixAAudioSource_isPaused(STNixSourceRef ref);
NixBOOL         nixAAudioSource_isRepeat(STNixSourceRef ref);
NixFLOAT        nixAAudioSource_getVolume(STNixSourceRef ref);
NixBOOL         nixAAudioSource_setBuffer(STNixSourceRef ref, STNixBufferRef buff);  //static-source
NixBOOL         nixAAudioSource_queueBuffer(STNixSourceRef ref, STNixBufferRef buff); //stream-source
NixBOOL         nixAAudioSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset); //relative to first buffer in queue
NixUI32         nixAAudioSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);   //all buffer queue
NixUI32         nixAAudioSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //relative to first buffer in queue
//Recorder
STNixRecorderRef nixAAudioRecorder_alloc(STNixEngineRef eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
void            nixAAudioRecorder_free(STNixRecorderRef ref);
NixBOOL         nixAAudioRecorder_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL         nixAAudioRecorder_start(STNixRecorderRef ref);
NixBOOL         nixAAudioRecorder_stop(STNixRecorderRef ref);
NixBOOL         nixAAudioRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying);
NixBOOL         nixAAudioRecorder_isCapturing(STNixRecorderRef ref);
NixUI32         nixAAudioRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);

NixBOOL nixAAudioEngine_getApiItf(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->engine.alloc      = nixAAudioEngine_alloc;
        dst->engine.free        = nixAAudioEngine_free;
        dst->engine.printCaps   = nixAAudioEngine_printCaps;
        dst->engine.ctxIsActive = nixAAudioEngine_ctxIsActive;
        dst->engine.ctxActivate = nixAAudioEngine_ctxActivate;
        dst->engine.ctxDeactivate = nixAAudioEngine_ctxDeactivate;
        dst->engine.tick        = nixAAudioEngine_tick;
        //Factory
        dst->engine.allocSource = nixAAudioEngine_allocSource;
        dst->engine.allocBuffer = nixAAudioEngine_allocBuffer;
        dst->engine.allocRecorder = nixAAudioEngine_allocRecorder;
        //PCMBuffer
        NixPCMBuffer_getApiItf(&dst->buffer);
        //Source
        dst->source.alloc       = nixAAudioSource_alloc;
        dst->source.free        = nixAAudioSource_free;
        dst->source.setCallback = nixAAudioSource_setCallback;
        dst->source.setVolume   = nixAAudioSource_setVolume;
        dst->source.setRepeat   = nixAAudioSource_setRepeat;
        dst->source.play        = nixAAudioSource_play;
        dst->source.pause       = nixAAudioSource_pause;
        dst->source.stop        = nixAAudioSource_stop;
        dst->source.isPlaying   = nixAAudioSource_isPlaying;
        dst->source.isPaused    = nixAAudioSource_isPaused;
        dst->source.isRepeat    = nixAAudioSource_isRepeat;
        dst->source.getVolume   = nixAAudioSource_getVolume;
        dst->source.setBuffer   = nixAAudioSource_setBuffer;  //static-source
        dst->source.queueBuffer = nixAAudioSource_queueBuffer; //stream-source
        dst->source.setBufferOffset = nixAAudioSource_setBufferOffset; //relative to first buffer in queue
        dst->source.getBuffersCount = nixAAudioSource_getBuffersCount; //all buffer queue
        dst->source.getBlocksOffset = nixAAudioSource_getBlocksOffset; //relative to first buffer in queue
        //Recorder
        dst->recorder.alloc     = nixAAudioRecorder_alloc;
        dst->recorder.free      = nixAAudioRecorder_free;
        dst->recorder.setCallback = nixAAudioRecorder_setCallback;
        dst->recorder.start     = nixAAudioRecorder_start;
        dst->recorder.stop      = nixAAudioRecorder_stop;
        dst->recorder.flush     = nixAAudioRecorder_flush;
        dst->recorder.isCapturing = nixAAudioRecorder_isCapturing;
        dst->recorder.getBuffersFilledCount = nixAAudioRecorder_getBuffersFilledCount;
        //
        r = NIX_TRUE;
    }
    return r;
}


// Macros to allow compiling code outside of Android;
// just to use my prefered IDE before testing on Android.
#ifndef __ANDROID__
#   include <stdlib.h> //for malloc
#   define aaudio_allowed_capture_policy_t              int
#   define aaudio_channel_mask_t                        unsigned int
#   define aaudio_content_type_t                        int
#   define aaudio_data_callback_result_t                int
#   define aaudio_direction_t                           int
#   define aaudio_format_t                              int
#   define aaudio_input_preset_t                        int
#   define aaudio_performance_mode_t                    int
#   define aaudio_result_t                              int
#   define aaudio_session_id_t                          int
#   define aaudio_sharing_mode_t                        int
#   define aaudio_spatialization_behavior_t             int
#   define aaudio_stream_state_t                        int
#   define aaudio_usage_t                               int
//
#   define AAudioStreamBuilder                          unsigned int
#   define AAudio_createStreamBuilder(PTR)              AAUDIO_OK; *(PTR) = (AAudioStreamBuilder*)malloc(sizeof(AAudioStreamBuilder))
#   define AAudioStreamBuilder_delete(B)                free(B)
#   define AAudioStreamBuilder_setDataCallback(B, C, D)
#   define AAudioStreamBuilder_setSampleRate(B, D)
#   define AAudioStreamBuilder_setDeviceId(B, D);
#   define AAudioStreamBuilder_setDirection(B, D);
#   define AAudioStreamBuilder_setSharingMode(B, D);
#   define AAudioStreamBuilder_setChannelCount(B, D);
#   define AAudioStreamBuilder_setFormat(B, D);
#   define AAudioStreamBuilder_setBufferCapacityInFrames(B, D);
#   define AAudioStreamBuilder_setDataCallback(B, C, D)
#   define AAudioStreamBuilder_setErrorCallback(B, C, D)
#   define AAudioStreamBuilder_openStream(B, S)         AAUDIO_OK; *(S) = (AAudioStream*)malloc(sizeof(AAudioStream))
//
#   define AAudioStream                                 unsigned int
#   define AAudioStream_getState(S)                     AAUDIO_STREAM_STATE_CLOSED
#   define AAudioStream_requestStart(S)                 AAUDIO_OK
#   define AAudioStream_requestPause(S)                 AAUDIO_OK
#   define AAudioStream_requestStop(S)                  AAUDIO_OK
#   define AAudioStream_requestFlush(S)                 AAUDIO_OK
#   define AAudioStream_close(S)                        AAUDIO_OK
#   define AAudioStream_getDirection(S)                 AAUDIO_DIRECTION_OUTPUT
#   define AAudioStream_getSharingMode(S)               AAUDIO_SHARING_MODE_SHARED
#   define AAudioStream_getSampleRate(S)                44100
#   define AAudioStream_getChannelCount(S)              2
#   define AAudioStream_getFormat(S)                    AAUDIO_FORMAT_PCM_I16
//
#   define AAudio_convertResultToText(C)                ""
//
    enum AAUDIO_DIRECTION {
     AAUDIO_DIRECTION_OUTPUT,
     AAUDIO_DIRECTION_INPUT
    };
    enum AAUDIO_SHARING_MODE {
      AAUDIO_SHARING_MODE_EXCLUSIVE,
      AAUDIO_SHARING_MODE_SHARED
    };
    enum AAUDIO_FORMAT {
      AAUDIO_FORMAT_INVALID = -1,
      AAUDIO_FORMAT_UNSPECIFIED = 0,
      AAUDIO_FORMAT_PCM_I16,
      AAUDIO_FORMAT_PCM_FLOAT,
      AAUDIO_FORMAT_PCM_I24_PACKED,
      AAUDIO_FORMAT_PCM_I32,
      AAUDIO_FORMAT_IEC61937
    };
    enum AAUDIO_CALLBACK_RESULT {
      AAUDIO_CALLBACK_RESULT_CONTINUE = 0,
      AAUDIO_CALLBACK_RESULT_STOP
    };
    enum AUDIO_RESULT {
      AAUDIO_OK,
      AAUDIO_ERROR_BASE = -900,
      AAUDIO_ERROR_DISCONNECTED,
      AAUDIO_ERROR_ILLEGAL_ARGUMENT,
      AAUDIO_ERROR_INTERNAL = AAUDIO_ERROR_ILLEGAL_ARGUMENT + 2,
      AAUDIO_ERROR_INVALID_STATE,
      AAUDIO_ERROR_INVALID_HANDLE = AAUDIO_ERROR_INVALID_STATE + 3,
      AAUDIO_ERROR_UNIMPLEMENTED = AAUDIO_ERROR_INVALID_HANDLE + 2,
      AAUDIO_ERROR_UNAVAILABLE,
      AAUDIO_ERROR_NO_FREE_HANDLES,
      AAUDIO_ERROR_NO_MEMORY,
      AAUDIO_ERROR_NULL,
      AAUDIO_ERROR_TIMEOUT,
      AAUDIO_ERROR_WOULD_BLOCK,
      AAUDIO_ERROR_INVALID_FORMAT,
      AAUDIO_ERROR_OUT_OF_RANGE,
      AAUDIO_ERROR_NO_SERVICE,
      AAUDIO_ERROR_INVALID_RATE
    };
    enum AAUDIO_STREAM_STATE {
      AAUDIO_STREAM_STATE_UNINITIALIZED = 0,
      AAUDIO_STREAM_STATE_UNKNOWN,
      AAUDIO_STREAM_STATE_OPEN,
      AAUDIO_STREAM_STATE_STARTING,
      AAUDIO_STREAM_STATE_STARTED,
      AAUDIO_STREAM_STATE_PAUSING,
      AAUDIO_STREAM_STATE_PAUSED,
      AAUDIO_STREAM_STATE_FLUSHING,
      AAUDIO_STREAM_STATE_FLUSHED,
      AAUDIO_STREAM_STATE_STOPPING,
      AAUDIO_STREAM_STATE_STOPPED,
      AAUDIO_STREAM_STATE_CLOSING,
      AAUDIO_STREAM_STATE_CLOSED,
      AAUDIO_STREAM_STATE_DISCONNECTED
    };
#endif

struct STNixAAudioEngine_;
struct STNixAAudioSource_;
struct STNixAAudioQueue_;
struct STNixAAudioQueuePair_;
struct STNixAAudioRecorder_;

//------
//Engine
//------

typedef struct STNixAAudioEngine_ {
    STNixContextRef ctx;
    STNixApiItf     apiItf;
    //srcs
    struct {
        STNixMutexRef       mutex;
        struct STNixAAudioSource_** arr;
        NixUI32             use;
        NixUI32             sz;
        NixUI32             changingStateCountHint;
    } srcs;
    struct STNixAAudioRecorder_* rec;
} STNixAAudioEngine;

void NixAAudioEngine_init(STNixContextRef ctx, STNixAAudioEngine* obj);
void NixAAudioEngine_destroy(STNixAAudioEngine* obj);
NixBOOL NixAAudioEngine_srcsAdd(STNixAAudioEngine* obj, struct STNixAAudioSource_* src);
void NixAAudioEngine_tick(STNixAAudioEngine* obj, const NixBOOL isFinalCleanup);

//------
//QueuePair (Buffers)
//------

typedef struct STNixAAudioQueuePair_ {
    STNixContextRef ctx;
    STNixBufferRef  org;    //original buffer (owned by the user)
    STNixPCMBuffer* cnv;    //converted buffer (owned by the source)
} STNixAAudioQueuePair;

void NixAAudioQueuePair_init(STNixContextRef ctx, STNixAAudioQueuePair* obj);
void NixAAudioQueuePair_destroy(STNixAAudioQueuePair* obj);
void NixAAudioQueuePair_moveOrg(STNixAAudioQueuePair* obj, STNixAAudioQueuePair* to);
void NixAAudioQueuePair_moveCnv(STNixAAudioQueuePair* obj, STNixAAudioQueuePair* to);

//------
//Queue (Buffers)
//------

typedef struct STNixAAudioQueue_ {
    STNixContextRef         ctx;
    STNixAAudioQueuePair*   arr;
    NixUI32                 use;
    NixUI32                 sz;
} STNixAAudioQueue;

void NixAAudioQueue_init(STNixContextRef ctx, STNixAAudioQueue* obj);
void NixAAudioQueue_destroy(STNixAAudioQueue* obj);
//
NixBOOL NixAAudioQueue_flush(STNixAAudioQueue* obj);
NixBOOL NixAAudioQueue_prepareForSz(STNixAAudioQueue* obj, const NixUI32 minSz);
NixBOOL NixAAudioQueue_pushOwning(STNixAAudioQueue* obj, STNixAAudioQueuePair* pair);
NixBOOL NixAAudioQueue_popOrphaning(STNixAAudioQueue* obj, STNixAAudioQueuePair* dst);
NixBOOL NixAAudioQueue_popMovingTo(STNixAAudioQueue* obj, STNixAAudioQueue* other);

//------
//Source
//------

typedef struct STNixAAudioSource_ {
    STNixContextRef         ctx;
    STNixSourceRef          self;
    struct STNixAAudioEngine_* eng;    //parent engine
    STNixAudioDesc          buffsFmt;   //first attached buffers' format (defines the converter config)
    STNixAudioDesc          srcFmt;
    AAudioStream*           src;
    //queues
    struct {
        STNixMutexRef       mutex;
        void*               conv;   //NixFmtConverter
        STNixSourceCallback callback;
        STNixAAudioQueue    notify; //buffers (consumed, pending to notify)
        STNixAAudioQueue    reuse;  //buffers (conversion buffers)
        STNixAAudioQueue    pend;   //to be played/filled
        NixUI32             pendBlockIdx;  //current sample playing/filling
    } queues;
    //props
    float                   volume;
    NixUI8                  stateBits;  //packed bools to reduce padding, NIX_AAudioSource_BIT_
} STNixAAudioSource;

void NixAAudioSource_init(STNixContextRef ctx, STNixAAudioSource* obj);
void NixAAudioSource_destroy(STNixAAudioSource* obj);
NixBOOL NixAAudioSource_queueBufferForOutput(STNixAAudioSource* obj, STNixBufferRef pBuff);
NixUI32 NixAAudioSource_feedSamplesTo(STNixAAudioSource* obj, void* dst, const NixUI32 samplesMax, NixBOOL* dstExplicitStop);
NixBOOL NixAAudioSource_pendPopOldestBuffLocked_(STNixAAudioSource* obj);
NixBOOL NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNixAAudioSource* obj);

#define NIX_AAudioSource_BIT_isStatic   (0x1 << 0)  //source expects only one buffer, repeats or pauses after playing it
#define NIX_AAudioSource_BIT_isChanging (0x1 << 1)  //source is changing state after a call to request*()
#define NIX_AAudioSource_BIT_isRepeat   (0x1 << 2)
#define NIX_AAudioSource_BIT_isPlaying  (0x1 << 3)
#define NIX_AAudioSource_BIT_isPaused   (0x1 << 4)
#define NIX_AAudioSource_BIT_isClosing  (0x1 << 5)
#define NIX_AAudioSource_BIT_isOrphan   (0x1 << 6)  //source is waiting for close(), wait for the change of state and NixAAudioSource_release + free.
//
#define NixAAudioSource_isStatic(OBJ)          (((OBJ)->stateBits & NIX_AAudioSource_BIT_isStatic) != 0)
#define NixAAudioSource_isChanging(OBJ)        (((OBJ)->stateBits & NIX_AAudioSource_BIT_isChanging) != 0)
#define NixAAudioSource_isRepeat(OBJ)          (((OBJ)->stateBits & NIX_AAudioSource_BIT_isRepeat) != 0)
#define NixAAudioSource_isPlaying(OBJ)         (((OBJ)->stateBits & NIX_AAudioSource_BIT_isPlaying) != 0)
#define NixAAudioSource_isPaused(OBJ)          (((OBJ)->stateBits & NIX_AAudioSource_BIT_isPaused) != 0)
#define NixAAudioSource_isClosing(OBJ)         (((OBJ)->stateBits & NIX_AAudioSource_BIT_isClosing) != 0)
#define NixAAudioSource_isOrphan(OBJ)          (((OBJ)->stateBits & NIX_AAudioSource_BIT_isOrphan) != 0)
//
#define NixAAudioSource_setIsStatic(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AAudioSource_BIT_isStatic : (OBJ)->stateBits & ~NIX_AAudioSource_BIT_isStatic)
#define NixAAudioSource_setIsChanging(OBJ, V)  (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AAudioSource_BIT_isChanging : (OBJ)->stateBits & ~NIX_AAudioSource_BIT_isChanging)
#define NixAAudioSource_setIsRepeat(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AAudioSource_BIT_isRepeat : (OBJ)->stateBits & ~NIX_AAudioSource_BIT_isRepeat)
#define NixAAudioSource_setIsPlaying(OBJ, V)   (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AAudioSource_BIT_isPlaying : (OBJ)->stateBits & ~NIX_AAudioSource_BIT_isPlaying)
#define NixAAudioSource_setIsPaused(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AAudioSource_BIT_isPaused : (OBJ)->stateBits & ~NIX_AAudioSource_BIT_isPaused)
#define NixAAudioSource_setIsClosing(OBJ)      (OBJ)->stateBits = ((OBJ)->stateBits | NIX_AAudioSource_BIT_isClosing)
#define NixAAudioSource_setIsOrphan(OBJ)       (OBJ)->stateBits = ((OBJ)->stateBits | NIX_AAudioSource_BIT_isOrphan)

//------
//Recorder
//------

typedef struct STNixAAudioRecorder_ {
    STNixContextRef         ctx;
    NixBOOL                 engStarted;
    STNixEngineRef          engRef;
    STNixRecorderRef        selfRef;
    AAudioStream*           rec;
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
        STNixAAudioQueue    notify;
        STNixAAudioQueue    reuse;
        //filling
        struct {
            NixSI32         iCurSample; //at first buffer in 'reuse'
        } filling;
    } queues;
} STNixAAudioRecorder;

void NixAAudioRecorder_init(STNixContextRef ctx,STNixAAudioRecorder* obj);
void NixAAudioRecorder_destroy(STNixAAudioRecorder* obj);
//
NixBOOL NixAAudioRecorder_prepare(STNixAAudioRecorder* obj, STNixAAudioEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
NixBOOL NixAAudioRecorder_setCallback(STNixAAudioRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL NixAAudioRecorder_start(STNixAAudioRecorder* obj);
NixBOOL NixAAudioRecorder_stop(STNixAAudioRecorder* obj);
NixBOOL NixAAudioRecorder_flush(STNixAAudioRecorder* obj);
void NixAAudioRecorder_consumeInputBuffer(STNixAAudioRecorder* obj, void* audioData, const NixSI32 numFrames);
void NixAAudioRecorder_notifyBuffers(STNixAAudioRecorder* obj, const NixBOOL discardWithoutNotifying);

//------
//Engine
//------

void NixAAudioEngine_init(STNixContextRef ctx, STNixAAudioEngine* obj){
    memset(obj, 0, sizeof(STNixAAudioEngine));
    //
    NixContext_set(&obj->ctx, ctx);
    nixAAudioEngine_getApiItf(&obj->apiItf);
    //srcs
    {
        obj->srcs.mutex = NixContext_mutex_alloc(obj->ctx);
    }
}

   
void NixAAudioEngine_destroy(STNixAAudioEngine* obj){
    //srcs
    {
        //cleanup
        while(obj->srcs.arr != NULL && obj->srcs.use > 0){
            NixAAudioEngine_tick(obj, NIX_TRUE);
        }
        //
        if(obj->srcs.arr != NULL){
            NixContext_mfree(obj->ctx, obj->srcs.arr);
            obj->srcs.arr = NULL;
        }
        NixMutex_free(&obj->srcs.mutex);
    }
    //rec (recorder)
    if(obj->rec != NULL){
        obj->rec = NULL;
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixAAudioEngine_srcsAdd(STNixAAudioEngine* obj, struct STNixAAudioSource_* src){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        NixMutex_lock(obj->srcs.mutex);
        {
            //resize array (if necesary)
            if(obj->srcs.use >= obj->srcs.sz){
                const NixUI32 szN = obj->srcs.use + 4;
                STNixAAudioSource** arrN = (STNixAAudioSource**)NixContext_mrealloc(obj->ctx, obj->srcs.arr, sizeof(STNixAAudioSource*) * szN, "STNixAAudioEngine::srcsN");
                if(arrN != NULL){
                    obj->srcs.arr = arrN;
                    obj->srcs.sz = szN;
                }
            }
            //add
            if(obj->srcs.use >= obj->srcs.sz){
                NIX_PRINTF_ERROR("nixAAudioSource_create::STNixAAudioEngine::srcs failed (no allocated space).\n");
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

void NixAAudioEngine_removeSrcRecordLocked_(STNixAAudioEngine* obj, NixSI32* idx){
    STNixAAudioSource* src = obj->srcs.arr[*idx];
    if(src != NULL){
        NixAAudioSource_destroy(src);
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

void NixAAudioEngine_tick_addQueueNotifSrcLocked_(STNixNotifQueue* notifs, STNixAAudioSource* src){
    if(src->queues.notify.use > 0){
        NixSI32 i; for(i = 0; i < src->queues.notify.use; i++){
            STNixAAudioQueuePair* pair = &src->queues.notify.arr[i];
            if(!NixNotifQueue_addBuff(notifs, src->self, src->queues.callback, pair->org)){
                NIX_ASSERT(NIX_FALSE); //program logic error
            }
        }
        if(!NixAAudioQueue_flush(&src->queues.notify)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        }
    }
}
    
void NixAAudioEngine_tick(STNixAAudioEngine* obj, const NixBOOL isFinalCleanup){
    if(obj != NULL){
        //srcs
        {
            STNixNotifQueue notifs;
            NixNotifQueue_init(obj->ctx, &notifs);
            NixMutex_lock(obj->srcs.mutex);
            if(obj->srcs.arr != NULL && obj->srcs.use > 0){
                NixUI32 changingStateCount = 0;
                NixSI32 i; for(i = 0; i < (NixSI32)obj->srcs.use; ++i){
                    STNixAAudioSource* src = obj->srcs.arr[i];
                    if(src->src == NULL && NixAAudioSource_isOrphan(src)){
                        //remove
                        NixAAudioEngine_removeSrcRecordLocked_(obj, &i);
                        src = NULL;
                    } else {
                        aaudio_stream_state_t state = AAUDIO_STREAM_STATE_UNKNOWN;
                        if(NixAAudioSource_isChanging(src) || NixAAudioSource_isOrphan(src) || isFinalCleanup){
                            aaudio_stream_state_t state = AAudioStream_getState(src->src);
                            switch (state) {
                                case AAUDIO_STREAM_STATE_STARTING:
                                case AAUDIO_STREAM_STATE_PAUSING:
                                case AAUDIO_STREAM_STATE_FLUSHING:
                                case AAUDIO_STREAM_STATE_STOPPING:
                                case AAUDIO_STREAM_STATE_CLOSING:
                                    ++changingStateCount;
                                    break;
                                case AAUDIO_STREAM_STATE_FLUSHED:
                                case AAUDIO_STREAM_STATE_STOPPED:
                                    NixAAudioSource_setIsPlaying(src, NIX_FALSE);
                                    NixAAudioSource_setIsPaused(src, NIX_FALSE);
                                    NixAAudioSource_setIsChanging(src, NIX_FALSE);
                                    //move all pending buffers to notify
                                    NixMutex_lock(src->queues.mutex);
                                    {
                                        NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(src);
                                    }
                                    NixMutex_unlock(src->queues.mutex);
                                    break;
                                case AAUDIO_STREAM_STATE_STARTED:
                                    NixAAudioSource_setIsPlaying(src, NIX_TRUE);
                                    NixAAudioSource_setIsPaused(src, NIX_FALSE);
                                    NixAAudioSource_setIsChanging(src, NIX_FALSE);
                                    break;
                                case AAUDIO_STREAM_STATE_PAUSED:
                                    NixAAudioSource_setIsPlaying(src, NIX_TRUE);
                                    NixAAudioSource_setIsPaused(src, NIX_TRUE);
                                    NixAAudioSource_setIsChanging(src, NIX_FALSE);
                                    break;
                                case AAUDIO_STREAM_STATE_CLOSED:
                                    NixAAudioSource_setIsPlaying(src, NIX_FALSE);
                                    NixAAudioSource_setIsPaused(src, NIX_FALSE);
                                    NixAAudioSource_setIsChanging(src, NIX_FALSE);
                                    //move all pending buffers to notify
                                    NixMutex_lock(src->queues.mutex);
                                    {
                                        NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(src);
                                        //add notif before removing
                                        NixAAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
                                    }
                                    NixMutex_unlock(src->queues.mutex);
                                    //release and remove
                                    NixAAudioEngine_removeSrcRecordLocked_(obj, &i);
                                    src = NULL;
                                    break;
                                default:
                                    //AAUDIO_STREAM_STATE_UNINITIALIZED
                                    //AAUDIO_STREAM_STATE_UNKNOWN
                                    //AAUDIO_STREAM_STATE_OPEN
                                    //
                                    //AAUDIO_STREAM_STATE_DISCONNECTED
                                    break;
                            }
                        }
                        //post-process
                        if(src != NULL){
                            //count as changing state
                            if(NixAAudioSource_isOrphan(src)){
                                ++changingStateCount;
                            }
                            //close orphaned sources (or final-cleanup)
                            if((isFinalCleanup || NixAAudioSource_isOrphan(src)) && state != AAUDIO_STREAM_STATE_CLOSING && state != AAUDIO_STREAM_STATE_CLOSED){
                                if(AAUDIO_OK != AAudioStream_close(src->src)){
                                    NIX_PRINTF_ERROR("NixAAudioEngine_tick::AAudioStream_close failed.\n");
                                } else {
                                    NixAAudioSource_setIsClosing(src);
                                    NixAAudioSource_setIsChanging(src, NIX_TRUE);
                                    ++changingStateCount;
                                }
                                NixAAudioSource_setIsPlaying(src, NIX_FALSE);
                                NixAAudioSource_setIsPaused(src, NIX_FALSE);
                            }
                            //add to notify queue
                            {
                                NixMutex_lock(src->queues.mutex);
                                {
                                    NixAAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
                                }
                                NixMutex_unlock(src->queues.mutex);
                            }
                        }
                    }
                }
                obj->srcs.changingStateCountHint = changingStateCount;
            }
            NixMutex_unlock(obj->srcs.mutex);
            //notify (unloked)
            if(notifs.use > 0){
                NixUI32 i; for(i = 0; i < notifs.use; ++i){
                    STNixSourceNotif* n = &notifs.arr[i];
                    if(n->callback.func != NULL){
                        (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
                    }
                }
            }
            NixNotifQueue_destroy(&notifs);
        }
        //recorder
        if(obj->rec != NULL){
            NixAAudioRecorder_notifyBuffers(obj->rec, NIX_FALSE);
        }
    }
}

//------
//QueuePair (Buffers)
//------

void NixAAudioQueuePair_init(STNixContextRef ctx, STNixAAudioQueuePair* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
}

void NixAAudioQueuePair_destroy(STNixAAudioQueuePair* obj){
    if(obj->org.ptr != NULL){
        NixBuffer_release(&obj->org);
        obj->org.ptr = NULL;
    }
    if(obj->cnv != NULL){
        NixPCMBuffer_destroy(obj->cnv);
        NixContext_mfree(obj->ctx, obj->cnv);
        obj->cnv = NULL;
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

void NixAAudioQueuePair_moveOrg(STNixAAudioQueuePair* obj, STNixAAudioQueuePair* to){
    NixBuffer_set(&to->org, obj->org);
    NixBuffer_release(&obj->org);
    NixBuffer_null(&obj->org);
}

void NixAAudioQueuePair_moveCnv(STNixAAudioQueuePair* obj, STNixAAudioQueuePair* to){
    if(to->cnv != NULL){
        NixPCMBuffer_destroy(to->cnv);
        NixContext_mfree(to->ctx, to->cnv);
        to->cnv = NULL;
    }
    to->cnv = obj->cnv;
    obj->cnv = NULL;
}

//------
//Queue (Buffers)
//------

void NixAAudioQueue_init(STNixContextRef ctx, STNixAAudioQueue* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
}

void NixAAudioQueue_destroy(STNixAAudioQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNixAAudioQueuePair* b = &obj->arr[i];
            NixAAudioQueuePair_destroy(b);
        }
        NixContext_mfree(obj->ctx, obj->arr);
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixAAudioQueue_flush(STNixAAudioQueue* obj){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        if(obj->arr != NULL){
            NixUI32 i; for(i = 0; i < obj->use; i++){
                STNixAAudioQueuePair* b = &obj->arr[i];
                NixAAudioQueuePair_destroy(b);
            }
            obj->use = 0;
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixAAudioQueue_prepareForSz(STNixAAudioQueue* obj, const NixUI32 minSz){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        //resize array (if necesary)
        if(minSz > obj->sz){
            const NixUI32 szN = minSz;
            STNixAAudioQueuePair* arrN = (STNixAAudioQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixAAudioQueuePair) * szN, "NixAAudioQueue_prepareForSz::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //analyze
        if(minSz > obj->sz){
            NIX_PRINTF_ERROR("NixAAudioQueue_prepareForSz failed (no allocated space).\n");
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAAudioQueue_pushOwning(STNixAAudioQueue* obj, STNixAAudioQueuePair* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNixAAudioQueuePair* arrN = (STNixAAudioQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixAAudioQueuePair) * szN, "NixAAudioQueue_pushOwning::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //add
        if(obj->use >= obj->sz){
            NIX_PRINTF_ERROR("NixAAudioQueue_pushOwning failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAAudioQueue_popOrphaning(STNixAAudioQueue* obj, STNixAAudioQueuePair* dst){
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

NixBOOL NixAAudioQueue_popMovingTo(STNixAAudioQueue* obj, STNixAAudioQueue* other){
    NixBOOL r = NIX_FALSE;
    STNixAAudioQueuePair pair;
    if(!NixAAudioQueue_popOrphaning(obj, &pair)){
        //
    } else {
        if(!NixAAudioQueue_pushOwning(other, &pair)){
            //program logic error
            NIX_ASSERT(NIX_FALSE);
            NixAAudioQueuePair_destroy(&pair);
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//Source
//------

void NixAAudioSource_init(STNixContextRef ctx, STNixAAudioSource* obj){
    memset(obj, 0, sizeof(STNixAAudioSource));
    NixContext_set(&obj->ctx, ctx);
    obj->volume = 1.f;
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixAAudioQueue_init(ctx, &obj->queues.notify);
        NixAAudioQueue_init(ctx, &obj->queues.pend);
        NixAAudioQueue_init(ctx, &obj->queues.reuse);
    }
}

void NixAAudioSource_destroy(STNixAAudioSource* obj){
    //src
    if(obj->src != NULL){
#       ifdef NIX_ASSERTS_ACTIVATED
        {
            aaudio_stream_state_t state = AAudioStream_getState(obj->src);
            NIX_ASSERT(state == AAUDIO_STREAM_STATE_CLOSED || state == AAUDIO_STREAM_STATE_UNINITIALIZED)
        }
#       endif
        obj->src = NULL;
    }
    //queues
    {
        if(obj->queues.conv != NULL){
            NixFmtConverter_free(obj->queues.conv);
            obj->queues.conv = NULL;
        }
        NixAAudioQueue_destroy(&obj->queues.pend);
        NixAAudioQueue_destroy(&obj->queues.reuse);
        NixAAudioQueue_destroy(&obj->queues.notify);
        NixMutex_free(&obj->queues.mutex);
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixAAudioSource_queueBufferForOutput(STNixAAudioSource* obj, STNixBufferRef pBuff){
    NixBOOL r = NIX_FALSE;
    STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
    if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
        //error
    } else {
        STNixAAudioQueuePair pair;
        NixAAudioQueuePair_init(obj->ctx, &pair);
        r = NIX_TRUE;
        //prepare conversion buffer (if necesary)
        if(obj->queues.conv != NULL){
            //create copy buffer
            const NixUI32 buffBlocksMax    = (buff->sz / buff->desc.blockAlign);
            const NixUI32 blocksReq        = NixFmtConverter_blocksForNewFrequency(buffBlocksMax, obj->buffsFmt.samplerate, obj->srcFmt.samplerate);
            STNixAAudioQueuePair reuse;
            if(!NixAAudioQueue_popOrphaning(&obj->queues.reuse, &reuse)){
                //no reusable buffer available, create new
                pair.cnv = (STNixPCMBuffer*)NixContext_malloc(obj->ctx, sizeof(STNixPCMBuffer), "NixAAudioSource_queueBufferForOutput::pair.cnv");
                if(pair.cnv == NULL){
                    NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::pair.cnv could be allocated.\n");
                    r = NIX_FALSE;
                } else {
                    NixPCMBuffer_init(obj->ctx, pair.cnv);
                }
            } else {
                //reuse buffer
                NIX_ASSERT(reuse.org.ptr == NULL) //program logic error
                NIX_ASSERT(reuse.cnv != NULL) //program logic error
                if(reuse.cnv == NULL){
                    NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::reuse.cnv should not be NULL.\n");
                    r = NIX_FALSE;
                } else {
                    pair.cnv = reuse.cnv; reuse.cnv = NULL; //consume
                }
                NixAAudioQueuePair_destroy(&reuse);
            }
            //convert
            if(pair.cnv == NULL){
                r = NIX_FALSE;
            } else if(!NixPCMBuffer_setData(pair.cnv, &obj->srcFmt, NULL, blocksReq * obj->srcFmt.blockAlign)){
                NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::NixPCMBuffer_setData failed.\n");
                r = NIX_FALSE;
            } else if(!NixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &buff->desc, buff->ptr, 0)){
                NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                r = NIX_FALSE;
            } else if(!NixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv, &pair.cnv->desc, pair.cnv->ptr, 0)){
                NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtDstInterlaced failed.\n");
                r = NIX_FALSE;
            } else {
                const NixUI32 srcBlocks = (buff->use / buff->desc.blockAlign);
                const NixUI32 dstBlocks = (pair.cnv->sz / pair.cnv->desc.blockAlign);
                NixUI32 ammBlocksRead = 0;
                NixUI32 ammBlocksWritten = 0;
                if(!NixFmtConverter_convert(obj->queues.conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                    NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::NixFmtConverter_convert failed from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s).\n"
                                     , obj->buffsFmt.samplerate
                                     , obj->buffsFmt.channels
                                     , obj->buffsFmt.bitsPerSample
                                     , obj->buffsFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->buffsFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                     , obj->srcFmt.samplerate
                                     , obj->srcFmt.channels
                                     , obj->srcFmt.bitsPerSample
                                     , obj->srcFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->srcFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                     );
                    r = NIX_FALSE;
                }
            }
        }
        //add to queue
        if(r){
            NixBuffer_set(&pair.org, pBuff);
            NixMutex_lock(obj->queues.mutex);
            {
                if(!NixAAudioQueue_pushOwning(&obj->queues.pend, &pair)){
                    NIX_PRINTF_ERROR("NixAAudioSource_queueBufferForOutput::NixAAudioQueue_pushOwning failed.\n");
                    r = NIX_FALSE;
                } else {
                    //added to queue
                    NixAAudioQueue_prepareForSz(&obj->queues.reuse, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    NixAAudioQueue_prepareForSz(&obj->queues.notify, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    //this is the first buffer i the queue
                    if(obj->queues.pend.use == 1){
                        obj->queues.pendBlockIdx = 0;
                    }
                    //start/resume stream?
                }
            }
            NixMutex_unlock(obj->queues.mutex);
        }
        if(!r){
            NixAAudioQueuePair_destroy(&pair);
        }
    }
    return r;
}

NixBOOL NixAAudioSource_pendPopOldestBuffLocked_(STNixAAudioSource* obj){
    NixBOOL r = NIX_FALSE;
    if(obj->queues.pend.use > 0){
        STNixAAudioQueuePair pair;
        if(!NixAAudioQueue_popOrphaning(&obj->queues.pend, &pair)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        } else {
            //move "cnv" to reusable queue
            if(pair.cnv != NULL){
                STNixAAudioQueuePair reuse;
                NixAAudioQueuePair_init(obj->ctx, &reuse);
                NixAAudioQueuePair_moveCnv(&pair, &reuse);
                if(!NixAAudioQueue_pushOwning(&obj->queues.reuse, &reuse)){
                    NIX_PRINTF_ERROR("NixAAudioSource_pendPopOldestBuffLocked_::NixAAudioQueue_pushOwning(reuse) failed.\n");
                    NixAAudioQueuePair_destroy(&reuse);
                }
            }
            //move "org" to notify queue
            if(!NixBuffer_isNull(pair.org)){
                STNixAAudioQueuePair notif;
                NixAAudioQueuePair_init(obj->ctx, &notif);
                NixAAudioQueuePair_moveOrg(&pair, &notif);
                if(!NixAAudioQueue_pushOwning(&obj->queues.notify, &notif)){
                    NIX_PRINTF_ERROR("NixAAudioSource_pendPopOldestBuffLocked_::NixAAudioQueue_pushOwning(notify) failed.\n");
                    NixAAudioQueuePair_destroy(&notif);
                }
            }
            NIX_ASSERT(pair.org.ptr == NULL); //program logic error
            NIX_ASSERT(pair.cnv == NULL); //program logic error
            NixAAudioQueuePair_destroy(&pair);
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNixAAudioSource* obj){
    NixBOOL r = NIX_TRUE;
    NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
        STNixAAudioQueuePair* pair = &obj->queues.pend.arr[i];
        //move "org" to notify queue
        if(!NixBuffer_isNull(pair->org)){
            STNixAAudioQueuePair notif;
            NixAAudioQueuePair_init(obj->ctx, &notif);
            NixAAudioQueuePair_moveOrg(pair, &notif);
            if(!NixAAudioQueue_pushOwning(&obj->queues.notify, &notif)){
                NIX_PRINTF_ERROR("NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_::NixAAudioQueue_pushOwning(notify) failed.\n");
                NixAAudioQueuePair_destroy(&notif);
            }
        }
    }
    return r;
}
    
NixUI32 NixAAudioSource_feedSamplesTo(STNixAAudioSource* obj, void* pDst, const NixUI32 samplesMax, NixBOOL* dstExplicitStop){
    NixUI32 r = 0;
    NixMutex_lock(obj->queues.mutex);
    {
        while(r < samplesMax && obj->queues.pend.use > 0){
            NixBOOL remove = NIX_FALSE, isFullyConsumed = NIX_FALSE;
            STNixAAudioQueuePair* pair = &obj->queues.pend.arr[0];
            STNixPCMBuffer* buff = (pair->cnv != NULL ? pair->cnv : (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr));
            NIX_ASSERT(buff != NULL) //program logic error
            if(buff == NULL || buff->ptr == NULL || buff->desc.blockAlign <= 0 || obj->srcFmt.blockAlign <= 0 || buff->desc.blockAlign != obj->srcFmt.blockAlign){
                //just remove
                remove = NIX_TRUE;
            } else {
                const NixUI32 blocks = ( buff->use / buff->desc.blockAlign );
                if(blocks <= obj->queues.pendBlockIdx){
                    //just remove
                    remove = isFullyConsumed = NIX_TRUE;
                } else {
                    //fill samples
                    const NixUI32 blocksAvailRead = (blocks - obj->queues.pendBlockIdx);
                    const NixUI32 blocksAvailWrite = (samplesMax - r);
                    const NixUI32 blocksDo = (blocksAvailRead < blocksAvailWrite ? blocksAvailRead : blocksAvailWrite);
                    if(blocksDo > 0){
                        NixBYTE* dst = &((NixBYTE*)pDst)[r * obj->srcFmt.blockAlign];
                        NixBYTE* src = &((NixBYTE*)buff->ptr)[obj->queues.pendBlockIdx * buff->desc.blockAlign];
                        //ToDo: copy applying volume
                        memcpy(dst, src, blocksDo * obj->srcFmt.blockAlign);
                        obj->queues.pendBlockIdx += blocksDo;
                        r += blocksDo;
                    }
                    if(blocksAvailRead == blocksDo){
                        remove = isFullyConsumed = NIX_TRUE;
                    }
                }
            }
            //
            if(remove){
                if(NixAAudioSource_isStatic(obj) && isFullyConsumed && obj->queues.pend.use == 1){
                    if(NixAAudioSource_isRepeat(obj)){
                        //consume again
                        obj->queues.pendBlockIdx = 0;
                    } else {
                        //pause while referencing the buffer as fully processed
                        {
                            NixAAudioSource_setIsPaused(obj, NIX_TRUE);
                        }
                        //stop consuming
                        {
                            if(dstExplicitStop != NULL){
                                *dstExplicitStop = NIX_TRUE;
                            }
                        }
                        break;
                    }
                } else {
                    //remove
                    NixAAudioSource_pendPopOldestBuffLocked_(obj);
                    //prepare for next buffer
                    obj->queues.pendBlockIdx = 0;
                }
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

//------
//Recorder
//------

void NixAAudioRecorder_init(STNixContextRef ctx, STNixAAudioRecorder* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
    //cfg
    {
        //
    }
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixAAudioQueue_init(ctx, &obj->queues.notify);
        NixAAudioQueue_init(ctx, &obj->queues.reuse);
    }
}

void NixAAudioRecorder_destroy(STNixAAudioRecorder* obj){
    //queues
    {
        NixMutex_lock(obj->queues.mutex);
        {
            NixAAudioQueue_destroy(&obj->queues.notify);
            NixAAudioQueue_destroy(&obj->queues.reuse);
            if(obj->queues.conv != NULL){
                NixFmtConverter_free(obj->queues.conv);
                obj->queues.conv = NULL;
            }
        }
        NixMutex_unlock(obj->queues.mutex);
        NixMutex_free(&obj->queues.mutex);
    }
    //
    if(obj->rec != NULL){
        AAudioStream_close(obj->rec);
        obj->rec = NULL;
    }
    if(obj->engRef.ptr != NULL){
        STNixAAudioEngine* eng = (STNixAAudioEngine*)NixSharedPtr_getOpq(obj->engRef.ptr);
        if(eng != NULL && eng->rec == obj){
            eng->rec = NULL;
        }
        NixEngine_release(&obj->engRef);
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

void nixAAudioRecorder_errorCallback_(AAudioStream *_Nonnull stream, void *_Nullable userData, aaudio_result_t error){
    //STNixAAudioRecorder* obj = (STNixAAudioRecorder*)userData;
    NIX_PRINTF_ERROR("nixAAudioRecorder_errorCallback_::error %d = '%s'.\n", error, AAudio_convertResultToText(error));
}

aaudio_data_callback_result_t nixAAudioRecorder_dataCallback_(AAudioStream *_Nonnull stream, void *_Nullable userData, void *_Nonnull audioData, int32_t numFrames){
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)userData;
    {
        NixAAudioRecorder_consumeInputBuffer(obj, audioData, numFrames);
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

NixBOOL NixAAudioRecorder_prepare(STNixAAudioRecorder* obj, STNixAAudioEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    NixBOOL r = NIX_FALSE;
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.conv == NULL && audioDesc->blockAlign > 0){
        AAudioStreamBuilder *bldr;
        aaudio_result_t rr = AAudio_createStreamBuilder(&bldr);
        if(rr != AAUDIO_OK || bldr == NULL){
            NIX_PRINTF_ERROR("NixAAudioRecorder_prepare::AAudio_createStreamBuilder failed.\n");
        } else {
            aaudio_result_t rr = 0;
            AAudioStream *stream = NULL;
            AAudioStreamBuilder_setDirection(bldr, AAUDIO_DIRECTION_INPUT);
            //AAudioStreamBuilder_setSharingMode(bldr, AAUDIO_SHARING_MODE_SHARED);
            AAudioStreamBuilder_setSampleRate(bldr, audioDesc->samplerate);
            AAudioStreamBuilder_setChannelCount(bldr, audioDesc->channels);
            AAudioStreamBuilder_setDataCallback(bldr, nixAAudioRecorder_dataCallback_, obj);
            AAudioStreamBuilder_setErrorCallback(bldr, nixAAudioRecorder_errorCallback_, obj);
            if(audioDesc->samplesFormat == ENNixSampleFmt_Int){
                if(audioDesc->bitsPerSample == 16){
                    AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_I16);
                } else if(audioDesc->bitsPerSample == 32){
                    AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_I32);
                }
            } else if(audioDesc->samplesFormat == ENNixSampleFmt_Float){
                if(audioDesc->bitsPerSample == 32){
                    AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_FLOAT);
                }
            }
            rr = AAudioStreamBuilder_openStream(bldr, &stream);
            if(AAUDIO_OK != rr){
                NIX_PRINTF_ERROR("NixAAudioRecorder_prepare::AAudioStreamBuilder_openStream failed.\n");
            } else {
                STNixAudioDesc inDesc;
                memset(&inDesc, 0, sizeof(inDesc));
                //read properties
                switch(AAudioStream_getFormat(stream)){
                    case AAUDIO_FORMAT_PCM_I16:
                        inDesc.bitsPerSample = 16;
                        inDesc.samplesFormat = ENNixSampleFmt_Int;
                        break;
                    case AAUDIO_FORMAT_PCM_I32:
                        inDesc.bitsPerSample = 32;
                        inDesc.samplesFormat = ENNixSampleFmt_Int;
                        break;
                    case AAUDIO_FORMAT_PCM_FLOAT:
                        inDesc.bitsPerSample = 32;
                        inDesc.samplesFormat = ENNixSampleFmt_Float;
                        break;
                    default:
                        inDesc.bitsPerSample = 0;
                        inDesc.samplesFormat = ENNixSampleFmt_Unknown;
                        break;
                }
                inDesc.channels   = AAudioStream_getChannelCount(stream);
                inDesc.samplerate = AAudioStream_getSampleRate(stream);
                if(inDesc.bitsPerSample <= 0 || inDesc.channels <= 0 || inDesc.samplerate <= 0){
                    NIX_PRINTF_ERROR("NixAAudioRecorder_prepare, unknown stream sample format.\n");
                } else {
                    inDesc.blockAlign = (inDesc.bitsPerSample / 8) * inDesc.channels;
                    void* conv = NixFmtConverter_alloc(obj->ctx);
                    if(!NixFmtConverter_prepare(conv, &inDesc, audioDesc)){
                        NIX_PRINTF_ERROR("NixAAudioRecorder_prepare::NixFmtConverter_prepare failed.\n");
                        NixFmtConverter_free(conv);
                        conv = NULL;
                    } else {
                        //allocate reusable buffers
                        while(obj->queues.reuse.use < buffersCount){
                            STNixAAudioQueuePair pair;
                            NixAAudioQueuePair_init(obj->ctx, &pair);
                            pair.org = (*eng->apiItf.buffer.alloc)(eng->ctx, audioDesc, NULL, audioDesc->blockAlign * blocksPerBuffer);
                            if(pair.org.ptr == NULL){
                                NIX_PRINTF_ERROR("NixAAudioRecorder_prepare::pair.org allocation failed.\n");
                                break;
                            } else {
                                NixAAudioQueue_pushOwning(&obj->queues.reuse, &pair);
                            }
                        }
                        //
                        if(obj->queues.reuse.use <= 0){
                            NIX_PRINTF_ERROR("NixAAudioRecorder_prepare::no reusable buffer could be allocated.\n");
                        } else {
                            //prepared
                            obj->queues.filling.iCurSample = 0;
                            obj->queues.conv = conv; conv = NULL; //consume
                            obj->rec = stream; stream = NULL; //consume
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
                if(stream != NULL){
                    AAudioStream_close(stream);
#                   ifdef NIX_ASSERTS_ACTIVATED
                    {
                        aaudio_stream_state_t state = AAudioStream_getState(stream);
                        NIX_ASSERT(state == AAUDIO_STREAM_STATE_CLOSED || state == AAUDIO_STREAM_STATE_UNINITIALIZED)
                    }
#                   endif
                    stream = NULL;
                }
            }
            AAudioStreamBuilder_delete(bldr);
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

NixBOOL NixAAudioRecorder_setCallback(STNixAAudioRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    {
        obj->callback.func = callback;
        obj->callback.data = callbackData;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixAAudioRecorder_start(STNixAAudioRecorder* obj){
    NixBOOL r = NIX_FALSE;
    if(obj->rec != NULL){
        r = NIX_TRUE;
        if(!obj->engStarted){
            if(AAUDIO_OK != AAudioStream_requestStart(obj->rec)){
                NIX_PRINTF_ERROR("NixAAudioRecorder_start::AAudioStream_requestStart failed.\n");
                r = NIX_FALSE;
            } else {
                obj->engStarted = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL NixAAudioRecorder_stop(STNixAAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(obj->rec != NULL){
        if(AAUDIO_OK != AAudioStream_requestStop(obj->rec)){
            NIX_PRINTF_ERROR("NixAAudioRecorder_stop::AAudioStream_requestStop failed.\n");
            r = NIX_FALSE;
        }
        obj->engStarted = NIX_FALSE;
    }
    NixAAudioRecorder_flush(obj);
    return r;
}

NixBOOL NixAAudioRecorder_flush(STNixAAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    //move filling buffer to notify (if data is available)
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.reuse.use > 0){
        STNixAAudioQueuePair* pair = &obj->queues.reuse.arr[0];
        if(!NixBuffer_isNull(pair->org) && ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr))->use > 0){
            obj->queues.filling.iCurSample = 0;
            if(!NixAAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                //program logic error
                r = NIX_FALSE;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

void NixAAudioRecorder_consumeInputBuffer(STNixAAudioRecorder* obj, void* audioData, const NixSI32 numFrames){
    if(obj->queues.conv != NULL && obj->rec != NULL && audioData != NULL && numFrames > 0){
        NixMutex_lock(obj->queues.mutex);
        {
            NixUI32 inIdx = 0;
            NixSI32 inSz = numFrames;
            //process
            while(inIdx < inSz){
                if(obj->queues.reuse.use <= 0){
                    //move oldest-notify buffer to reuse
                    if(!NixAAudioQueue_popMovingTo(&obj->queues.notify, &obj->queues.reuse)){
                        //program logic error
                        NIX_ASSERT(NIX_FALSE);
                        break;
                    }
                } else {
                    STNixAAudioQueuePair* pair = &obj->queues.reuse.arr[0];
                    if(NixBuffer_isNull(pair->org) || ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr))->desc.blockAlign <= 0){
                        //just remove
                        STNixAAudioQueuePair pair;
                        if(!NixAAudioQueue_popOrphaning(&obj->queues.reuse, &pair)){
                            NIX_ASSERT(NIX_FALSE);
                            //program logic error
                            break;
                        }
                        NixAAudioQueuePair_destroy(&pair);
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
                            NixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &obj->capFmt, audioData, inIdx);
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
                            if(!NixAAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
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

void NixAAudioRecorder_notifyBuffers(STNixAAudioRecorder* obj, const NixBOOL discardWithoutNotifying){
    NixMutex_lock(obj->queues.mutex);
    {
        const NixUI32 maxProcess = obj->queues.notify.use;
        NixUI32 ammProcessed = 0;
        while(ammProcessed < maxProcess && obj->queues.notify.use > 0){
            STNixAAudioQueuePair pair;
            if(!NixAAudioQueue_popOrphaning(&obj->queues.notify, &pair)){
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
                if(!NixAAudioQueue_pushOwning(&obj->queues.reuse, &pair)){
                    //program logic error
                    NIX_ASSERT(NIX_FALSE);
                    NixAAudioQueuePair_destroy(&pair);
                }
            }
            //processed
            ++ammProcessed;
        }
    }
    NixMutex_unlock(obj->queues.mutex);
}

//------
//Engine (API)
//------

STNixEngineRef nixAAudioEngine_alloc(STNixContextRef ctx){
    STNixEngineRef r = STNixEngineRef_Zero;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixContext_malloc(ctx, sizeof(STNixAAudioEngine), "STNixAAudioEngine");
    if(obj != NULL){
        NixAAudioEngine_init(ctx, obj);
        if(NULL == (r.ptr = NixSharedPtr_alloc(ctx.itf, obj, "nixAAudioEngine_alloc"))){
            NIX_PRINTF_ERROR("nixAAudioEngine_create::NixSharedPtr_alloc failed.\n");
        } else {
            r.itf = &obj->apiItf.engine;
            obj = NULL; //consume
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAAudioEngine_destroy(obj);
            NixContext_mfree(ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAAudioEngine_free(STNixEngineRef pObj){
    if(pObj.ptr != NULL){
        STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixAAudioEngine_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

void nixAAudioEngine_printCaps(STNixEngineRef pObj){
    //
}

NixBOOL nixAAudioEngine_ctxIsActive(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAAudioEngine_ctxActivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAAudioEngine_ctxDeactivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

void nixAAudioEngine_tick(STNixEngineRef pObj){
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        NixAAudioEngine_tick(obj, NIX_FALSE);
    }
}

//Factory

STNixSourceRef nixAAudioEngine_allocSource(STNixEngineRef ref){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.source.alloc != NULL){
        r = (*obj->apiItf.source.alloc)(ref);
    }
    return r;
}

STNixBufferRef nixAAudioEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixBufferRef r = STNixBufferRef_Zero;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.buffer.alloc != NULL){
        r = (*obj->apiItf.buffer.alloc)(obj->ctx, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

STNixRecorderRef nixAAudioEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixAAudioEngine* obj = (STNixAAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.recorder.alloc != NULL){
        r = (*obj->apiItf.recorder.alloc)(ref, audioDesc, buffersCount, blocksPerBuffer);
    }
    return r;
}

//------
//Source (API)
//------

STNixSourceRef nixAAudioSource_alloc(STNixEngineRef pEng){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixAAudioEngine* eng = (STNixAAudioEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng == NULL){
        NIX_PRINTF_ERROR("nixAAudioSource_alloc::eng is NULL.\n");
    } else {
        STNixAAudioSource* obj = (STNixAAudioSource*)NixContext_malloc(eng->ctx, sizeof(STNixAAudioSource), "STNixAAudioSource");
        if(obj == NULL){
            NIX_PRINTF_ERROR("nixAAudioSource_alloc::NixContext_malloc failed.\n");
        } else {
            NixAAudioSource_init(eng->ctx, obj);
            obj->eng = eng;
            //add to engine
            if(!NixAAudioEngine_srcsAdd(eng, obj)){
                NIX_PRINTF_ERROR("nixAAudioSource_create::NixAAudioEngine_srcsAdd failed.\n");
            } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "nixAAudioSource_alloc"))){
                NIX_PRINTF_ERROR("nixAAudioSource_create::NixSharedPtr_alloc failed.\n");
            } else {
                r.itf = &eng->apiItf.source;
                obj->self = r;
                obj = NULL; //consume
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAAudioSource_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAAudioSource_removeAllBuffersAndNotify_(STNixAAudioSource* obj){
    STNixNotifQueue notifs;
    NixNotifQueue_init(obj->ctx, &notifs);
    //move all pending buffers to notify
    NixMutex_lock(obj->queues.mutex);
    {
        NixAAudioSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(obj);
        NixAAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, obj);
    }
    NixMutex_unlock(obj->queues.mutex);
    //notify
    {
        NixUI32 i; for(i = 0; i < notifs.use; ++i){
            STNixSourceNotif* n = &notifs.arr[i];
            if(n->callback.func != NULL){
                (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
            }
        }
    }
    NixNotifQueue_destroy(&notifs);
}

void nixAAudioSource_free(STNixSourceRef pObj){ //orphans the source, will automatically be destroyed after internal cleanup
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            //set final state
            {
                //nullify self-reference before notifying
                //to avoid reviving this object during final notification.
                NixSource_null(&obj->self);
                //Flag as orphan, for cleanup inside 'tick'
                NixAAudioSource_setIsOrphan(obj); //source is waiting for close(), wait for the change of state and NixAAudioSource_release + free.
                NixAAudioSource_setIsPlaying(obj, NIX_FALSE);
                NixAAudioSource_setIsPaused(obj, NIX_FALSE);
                ++obj->eng->srcs.changingStateCountHint;
            }
            //flush all pending buffers
            {
                nixAAudioSource_removeAllBuffersAndNotify_(obj);
            }
        }
    }
}

void nixAAudioSource_setCallback(STNixSourceRef pObj, NixSourceCallbackFnc callback, void* callbackData){
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        obj->queues.callback.func = callback;
        obj->queues.callback.data = callbackData;
    }
}

NixBOOL nixAAudioSource_setVolume(STNixSourceRef pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        obj->volume = vol;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAAudioSource_setRepeat(STNixSourceRef pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixAAudioSource_setIsRepeat(obj, isRepeat);
        r = NIX_TRUE;
    }
    return r;
}

void nixAAudioSource_play(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->src != NULL && (!NixAAudioSource_isPlaying(obj) || NixAAudioSource_isPaused(obj))){
            //restart static buffer
            if(NixAAudioSource_isStatic(obj)){
                NixMutex_lock(obj->queues.mutex);
                {
                    if(obj->queues.pend.use == 1){
                        //last buffer to play
                        STNixAAudioQueuePair* pair = &obj->queues.pend.arr[0];
                        STNixPCMBuffer* buff = (pair->cnv != NULL ? pair->cnv : (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr));
                        if(buff != NULL && buff->desc.blockAlign > 0){
                            const NixUI32 blocks = buff->use / buff->desc.blockAlign;
                            if(obj->queues.pendBlockIdx >= blocks){
                                //restart this buffer
                                obj->queues.pendBlockIdx = 0;
                            }
                        }
                    }
                }
                NixMutex_unlock(obj->queues.mutex);
            }
            if(AAUDIO_OK != AAudioStream_requestStart(obj->src)){
                NIX_PRINTF_ERROR("nixAAudioSource_play::AAudioStream_requestStart failed.\n");
            } else {
                NixAAudioSource_setIsChanging(obj, NIX_TRUE);
                ++obj->eng->srcs.changingStateCountHint;
            }
        }
        NixAAudioSource_setIsPlaying(obj, NIX_TRUE);
        NixAAudioSource_setIsPaused(obj, NIX_FALSE);
    }
}

void nixAAudioSource_pause(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->src != NULL && NixAAudioSource_isPlaying(obj) && !NixAAudioSource_isPaused(obj)){
            if(AAUDIO_OK != AAudioStream_requestPause(obj->src)){
                NIX_PRINTF_ERROR("nixAAudioSource_pause::AAudioStream_requestPause failed.\n");
            } else {
                NixAAudioSource_setIsChanging(obj, NIX_TRUE);
                ++obj->eng->srcs.changingStateCountHint;
            }
        }
        NixAAudioSource_setIsPaused(obj, NIX_TRUE);
    }
}

void nixAAudioSource_stop(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(obj->src != NULL && NixAAudioSource_isPlaying(obj)){
            if(AAUDIO_OK != AAudioStream_requestStop(obj->src)){
                NIX_PRINTF_ERROR("nixAAudioSource_stop::AAudioStream_requestStop failed.\n");
            } else {
                NixAAudioSource_setIsChanging(obj, NIX_TRUE);
                ++obj->eng->srcs.changingStateCountHint;
            }
        }
        NixAAudioSource_setIsPlaying(obj, NIX_FALSE);
        NixAAudioSource_setIsPaused(obj, NIX_FALSE);
        //flush all pending buffers
        nixAAudioSource_removeAllBuffersAndNotify_(obj);
    }
}

NixBOOL nixAAudioSource_isPlaying(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        if(NixAAudioSource_isPlaying(obj) && !NixAAudioSource_isPaused(obj)){
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL nixAAudioSource_isPaused(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixAAudioSource_isPlaying(obj) && NixAAudioSource_isPaused(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixBOOL nixAAudioSource_isRepeat(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixAAudioSource_isRepeat(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixFLOAT nixAAudioSource_getVolume(STNixSourceRef pObj){
    NixFLOAT r = 0.f;
    if(pObj.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = obj->volume;
    }
    return r;
}

void nixAAudioSource_errorCallback_(AAudioStream *_Nonnull stream, void *_Nullable userData, aaudio_result_t error){
    //STNixAAudioSource* obj = (STNixAAudioSource*)userData;
    NIX_PRINTF_ERROR("nixAAudioSource_errorCallback_::error %d = '%s'.\n", error, AAudio_convertResultToText(error));
}

aaudio_data_callback_result_t nixAAudioSource_dataCallback_(AAudioStream *_Nonnull stream, void *_Nullable userData, void *_Nonnull audioData, int32_t numFrames){
    STNixAAudioSource* obj = (STNixAAudioSource*)userData;
    NixBOOL dstExplicitStop = NIX_FALSE;
    const NixUI32 numFed = NixAAudioSource_feedSamplesTo(obj, audioData, numFrames, &dstExplicitStop);
    //fille with zeroes the unpopulated area
    if(numFed < numFrames && obj->srcFmt.blockAlign > 0){
        void* data = &(((NixUI8*)audioData)[numFed * obj->srcFmt.blockAlign]);
        const NixUI32 dataSz = (numFrames - numFed) * obj->srcFmt.blockAlign;
        memset(data, 0, dataSz);
    }
    return (numFed < numFrames || dstExplicitStop ? AAUDIO_CALLBACK_RESULT_STOP : AAUDIO_CALLBACK_RESULT_CONTINUE);
}

NixBOOL nixAAudioSource_prepareSourceForFmt_(STNixAAudioSource* obj, const STNixAudioDesc* fmt){
    NixBOOL r = NIX_FALSE;
    AAudioStreamBuilder *bldr;
    aaudio_result_t rr = AAudio_createStreamBuilder(&bldr);
    if(rr != AAUDIO_OK || bldr == NULL){
        NIX_PRINTF_ERROR("nixAAudioSource_prepareSourceForFmt_::AAudio_createStreamBuilder failed.\n");
    } else {
        aaudio_result_t rr = 0;
        AAudioStream *stream = NULL;
        AAudioStreamBuilder_setDirection(bldr, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setSharingMode(bldr, AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setSampleRate(bldr, fmt->samplerate);
        AAudioStreamBuilder_setChannelCount(bldr, fmt->channels);
        AAudioStreamBuilder_setDataCallback(bldr, nixAAudioSource_dataCallback_, obj);
        AAudioStreamBuilder_setErrorCallback(bldr, nixAAudioSource_errorCallback_, obj);
        if(fmt->samplesFormat == ENNixSampleFmt_Int){
            if(fmt->bitsPerSample == 16){
                AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_I16);
            } else if(fmt->bitsPerSample == 32){
                AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_I32);
            }
        } else if(fmt->samplesFormat == ENNixSampleFmt_Float){
            if(fmt->bitsPerSample == 32){
                AAudioStreamBuilder_setFormat(bldr, AAUDIO_FORMAT_PCM_FLOAT);
            }
        }
        rr = AAudioStreamBuilder_openStream(bldr, &stream);
        if(AAUDIO_OK != rr){
            NIX_PRINTF_ERROR("nixAAudioSource_prepareSourceForFmt_::AAudioStreamBuilder_openStream failed.\n");
        } else {
            STNixAudioDesc desc;
            memset(&desc, 0, sizeof(desc));
            //read properties
            switch(AAudioStream_getFormat(stream)){
                case AAUDIO_FORMAT_PCM_I16:
                    desc.bitsPerSample = 16;
                    desc.samplesFormat = ENNixSampleFmt_Int;
                    break;
                case AAUDIO_FORMAT_PCM_I32:
                    desc.bitsPerSample = 32;
                    desc.samplesFormat = ENNixSampleFmt_Int;
                    break;
                case AAUDIO_FORMAT_PCM_FLOAT:
                    desc.bitsPerSample = 32;
                    desc.samplesFormat = ENNixSampleFmt_Float;
                    break;
                default:
                    desc.bitsPerSample = 0;
                    desc.samplesFormat = ENNixSampleFmt_Unknown;
                    break;
            }
            desc.channels   = AAudioStream_getChannelCount(stream);
            desc.samplerate = AAudioStream_getSampleRate(stream);
            if(desc.bitsPerSample <= 0 || desc.channels <= 0 || desc.samplerate <= 0){
                NIX_PRINTF_ERROR("nixAAudioSource_prepareSourceForFmt_, unknown stream sample format.\n");
            } else {
                desc.blockAlign = (desc.bitsPerSample / 8) * desc.channels;
                r = NIX_TRUE;
                //converter
                if(!STNixAudioDesc_isEqual(fmt, &desc)){
                    void* conv = NixFmtConverter_alloc(obj->ctx);
                    if(!NixFmtConverter_prepare(conv, fmt, &desc)){
                        NIX_PRINTF_ERROR("nixAAudioSource_prepareSourceForFmt_, NixFmtConverter_prepare failed.\n");
                        NixFmtConverter_free(conv);
                        r = NIX_FALSE;
                    } else {
                        obj->queues.conv = conv;
                    }
                }
                //set
                if(r){
                    obj->src = stream; stream = NULL; //consume
                    obj->srcFmt = desc;
                    obj->buffsFmt = *fmt;
                }
                
            }
            //release (if not consumed)
            if(stream != NULL){
                AAudioStream_close(stream);
#               ifdef NIX_ASSERTS_ACTIVATED
                {
                    aaudio_stream_state_t state = AAudioStream_getState(stream);
                    NIX_ASSERT(state == AAUDIO_STREAM_STATE_CLOSED || state == AAUDIO_STREAM_STATE_UNINITIALIZED)
                }
#               endif
                stream = NULL;
            }
        }
        AAudioStreamBuilder_delete(bldr);
    }
    return r;
}
    
NixBOOL nixAAudioSource_setBuffer(STNixSourceRef pObj, STNixBufferRef pBuff){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixAAudioSource* obj    = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(obj->src == NULL || obj->srcFmt.blockAlign <= 0){
            if(!nixAAudioSource_prepareSourceForFmt_(obj, &buff->desc)){
                NIX_PRINTF_ERROR("nixAAudioSource_setBuffer::nixAAudioSource_prepareSourceForFmt_ failed.\n");
            } else {
                NIX_ASSERT(obj->src != NULL) //program logic error
            }
        }
        //apply
        if(obj->src == NULL){
            NIX_PRINTF_ERROR("nixAAudioSource_setBuffer, no source available.\n");
        } else if(obj->queues.pend.use != 0){
            NIX_PRINTF_ERROR("nixAAudioSource_setBuffer, source already has buffer.\n");
        } else if(NixAAudioSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixAAudioSource_setBuffer, source is already static.\n");
        } else if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixAAudioSource_setBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            NixAAudioSource_setIsStatic(obj, NIX_TRUE);
            //schedule
            if(!NixAAudioSource_queueBufferForOutput(obj, pBuff)){
                NIX_PRINTF_ERROR("nixAAudioSource_setBuffer, NixAAudioSource_queueBufferForOutput failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixAAudioSource_queueBuffer(STNixSourceRef pObj, STNixBufferRef pBuff){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixAAudioSource* obj    = (STNixAAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(obj->src == NULL || obj->srcFmt.blockAlign <= 0){
            if(!nixAAudioSource_prepareSourceForFmt_(obj, &buff->desc)){
                NIX_PRINTF_ERROR("nixAAudioSource_queueBuffer, nixAAudioSource_prepareSourceForFmt_ failed.\n");
            } else {
                NIX_ASSERT(obj->src != NULL) //program logic error
            }
        }
        //
        if(obj->src == NULL){
            NIX_PRINTF_ERROR("nixAAudioSource_queueBuffer, no source available.\n");
        } else if(NixAAudioSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixAAudioSource_queueBuffer, source is static.\n");
        } else if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixAAudioSource_queueBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            //schedule
            if(!NixAAudioSource_queueBufferForOutput(obj, pBuff)){
                NIX_PRINTF_ERROR("nixAAudioSource_queueBuffer, NixAAudioSource_queueBufferForOutput failed.\n");
            } else {
                //restart stream if necesary
                if(obj->src != NULL && NixAAudioSource_isPlaying(obj) && !NixAAudioSource_isPaused(obj)){
                    aaudio_stream_state_t state = AAudioStream_getState(obj->src);
                    if(state != AAUDIO_STREAM_STATE_STARTING && state != AAUDIO_STREAM_STATE_STARTED){
                        if(AAUDIO_OK != AAudioStream_requestStart(obj->src)){
                            NIX_PRINTF_WARNING("nixAAudioSource_queueBuffer::AAudioStream_requestStart failed.\n");
                        } else {
                            NIX_PRINTF_INFO("nixAAudioSource_queueBuffer::AAudioStream_requestStart.\n");
                        }
                    }
                }
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixAAudioSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset){ //relative to first buffer in queue
    NixBOOL r = NIX_FALSE;
    if(ref.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->queues.pend.use > 0){
            STNixAAudioQueuePair* pair = &obj->queues.pend.arr[0];
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

NixUI32 nixAAudioSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){   //all buffer queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        {
            NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
                STNixAAudioQueuePair* pair = &obj->queues.pend.arr[0];
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

NixUI32 nixAAudioSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){  //relative to first buffer in queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixAAudioSource* obj = (STNixAAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->queues.pend.use > 0){
            STNixAAudioQueuePair* pair = &obj->queues.pend.arr[0];
            STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
            if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                blocksCount = obj->queues.pendBlockIdx;
                bytesCount  = obj->queues.pendBlockIdx * buff->desc.blockAlign;
                msecsCount  = blocksCount * 1000 / buff->desc.samplerate;
                r = blocksCount;
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
//Recorder (API)
//------

STNixRecorderRef nixAAudioRecorder_alloc(STNixEngineRef pEng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixAAudioEngine* eng = (STNixAAudioEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0 && audioDesc->blockAlign > 0 && eng->rec == NULL){
        STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixContext_malloc(eng->ctx, sizeof(STNixAAudioRecorder), "STNixAAudioRecorder");
        if(obj != NULL){
            NixAAudioRecorder_init(eng->ctx, obj);
            if(!NixAAudioRecorder_prepare(obj, eng, audioDesc, buffersCount, blocksPerBuffer)){
                NIX_PRINTF_ERROR("NixAAudioRecorder_create, NixAAudioRecorder_prepare failed.\n");
            } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "nixAAudioRecorder_alloc"))){
                NIX_PRINTF_ERROR("NixAAudioRecorder_create::NixSharedPtr_alloc failed.\n");
            } else {
                r.itf           = &eng->apiItf.recorder;
                obj->engRef     = pEng; NixEngine_retain(pEng);
                obj->selfRef    = r;
                eng->rec        = obj; obj = NULL; //consume
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAAudioRecorder_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAAudioRecorder_free(STNixRecorderRef pObj){
    if(pObj.ptr != NULL){
        STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixAAudioRecorder_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

NixBOOL nixAAudioRecorder_setCallback(STNixRecorderRef pObj, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAAudioRecorder_setCallback(obj, callback, callbackData);
    }
    return r;
}

NixBOOL nixAAudioRecorder_start(STNixRecorderRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAAudioRecorder_start(obj);
    }
    return r;
}

NixBOOL nixAAudioRecorder_stop(STNixRecorderRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAAudioRecorder_stop(obj);
    }
    return r;
}

NixBOOL nixAAudioRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying){
    NixBOOL r = NIX_FALSE;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        if(includeCurrentPartialBuff){
            NixAAudioRecorder_flush(obj);
        }
        NixAAudioRecorder_notifyBuffers(obj, discardWithoutNotifying);
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAAudioRecorder_isCapturing(STNixRecorderRef ref){
    NixBOOL r = NIX_FALSE;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = obj->engStarted;
    }
    return r;
}

NixUI32 nixAAudioRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    STNixAAudioRecorder* obj = (STNixAAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    //calculate filled buffers
    NixMutex_lock(obj->queues.mutex);
    {
        NixUI32 i; for(i = 0; i < obj->queues.notify.use; i++){
            STNixAAudioQueuePair* pair = &obj->queues.notify.arr[0];
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
