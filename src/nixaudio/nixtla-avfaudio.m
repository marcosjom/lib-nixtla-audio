//
//  nixtla.m
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 06/07/25.
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
//This file adds support for AVFAudio for playing and recording in MacOS and iOS.
//

#include "nixtla-audio-private.h"
#include "nixaudio/nixtla-audio.h"
#include "nixtla-avfaudio.h"
//
#include <AVFAudio/AVAudioEngine.h>
#include <AVFAudio/AVAudioPlayerNode.h>
#include <AVFAudio/AVAudioMixerNode.h>
#include <AVFAudio/AVAudioFormat.h>
#include <AVFAudio/AVAudioChannelLayout.h>
#include <AVFAudio/AVAudioConverter.h>

//------
//API Itf
//------

//AVFAudio interface to objective-c (iOS/MacOS)

//Engine
STNixEngineRef  nixAVAudioEngine_alloc(STNixContextRef ctx);
void            nixAVAudioEngine_free(STNixEngineRef ref);
void            nixAVAudioEngine_printCaps(STNixEngineRef ref);
NixBOOL         nixAVAudioEngine_ctxIsActive(STNixEngineRef ref);
NixBOOL         nixAVAudioEngine_ctxActivate(STNixEngineRef ref);
NixBOOL         nixAVAudioEngine_ctxDeactivate(STNixEngineRef ref);
void            nixAVAudioEngine_tick(STNixEngineRef ref);
//Factory
STNixSourceRef  nixAVAudioEngine_allocSource(STNixEngineRef ref);
STNixBufferRef  nixAVAudioEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
STNixRecorderRef nixAVAudioEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
//Source
STNixSourceRef  nixAVAudioSource_alloc(STNixEngineRef eng);
void            nixAVAudioSource_free(STNixSourceRef ref);
void            nixAVAudioSource_setCallback(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData);
NixBOOL         nixAVAudioSource_setVolume(STNixSourceRef ref, const float vol);
NixBOOL         nixAVAudioSource_setRepeat(STNixSourceRef ref, const NixBOOL isRepeat);
void            nixAVAudioSource_play(STNixSourceRef ref);
void            nixAVAudioSource_pause(STNixSourceRef ref);
void            nixAVAudioSource_stop(STNixSourceRef ref);
NixBOOL         nixAVAudioSource_isPlaying(STNixSourceRef ref);
NixBOOL         nixAVAudioSource_isPaused(STNixSourceRef ref);
NixBOOL         nixAVAudioSource_isRepeat(STNixSourceRef ref);
NixFLOAT        nixAVAudioSource_getVolume(STNixSourceRef ref);
NixBOOL         nixAVAudioSource_setBuffer(STNixSourceRef ref, STNixBufferRef buff);  //static-source
NixBOOL         nixAVAudioSource_queueBuffer(STNixSourceRef ref, STNixBufferRef buff); //stream-source
NixBOOL         nixAVAudioSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset); //relative to first buffer in queue
NixUI32         nixAVAudioSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);   //all buffer queue
NixUI32         nixAVAudioSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //relative to first buffer in queue
//Recorder
STNixRecorderRef nixAVAudioRecorder_alloc(STNixEngineRef eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
void            nixAVAudioRecorder_free(STNixRecorderRef ref);
NixBOOL         nixAVAudioRecorder_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL         nixAVAudioRecorder_start(STNixRecorderRef ref);
NixBOOL         nixAVAudioRecorder_stop(STNixRecorderRef ref);
NixBOOL         nixAVAudioRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying);
NixBOOL         nixAVAudioRecorder_isCapturing(STNixRecorderRef ref);
NixUI32         nixAVAudioRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);

NixBOOL nixAVAudioEngine_getApiItf(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->engine.alloc       = nixAVAudioEngine_alloc;
        dst->engine.free        = nixAVAudioEngine_free;
        dst->engine.printCaps   = nixAVAudioEngine_printCaps;
        dst->engine.ctxIsActive = nixAVAudioEngine_ctxIsActive;
        dst->engine.ctxActivate = nixAVAudioEngine_ctxActivate;
        dst->engine.ctxDeactivate = nixAVAudioEngine_ctxDeactivate;
        dst->engine.tick        = nixAVAudioEngine_tick;
        //Factory
        dst->engine.allocSource = nixAVAudioEngine_allocSource;
        dst->engine.allocBuffer = nixAVAudioEngine_allocBuffer;
        dst->engine.allocRecorder = nixAVAudioEngine_allocRecorder;
        //PCMBuffer
        NixPCMBuffer_getApiItf(&dst->buffer);
        //Source
        dst->source.alloc       = nixAVAudioSource_alloc;
        dst->source.free        = nixAVAudioSource_free;
        dst->source.setCallback = nixAVAudioSource_setCallback;
        dst->source.setVolume   = nixAVAudioSource_setVolume;
        dst->source.setRepeat   = nixAVAudioSource_setRepeat;
        dst->source.play        = nixAVAudioSource_play;
        dst->source.pause       = nixAVAudioSource_pause;
        dst->source.stop        = nixAVAudioSource_stop;
        dst->source.isPlaying   = nixAVAudioSource_isPlaying;
        dst->source.isPaused    = nixAVAudioSource_isPaused;
        dst->source.isRepeat    = nixAVAudioSource_isRepeat;
        dst->source.getVolume   = nixAVAudioSource_getVolume;
        dst->source.setBuffer   = nixAVAudioSource_setBuffer;  //static-source
        dst->source.queueBuffer = nixAVAudioSource_queueBuffer; //stream-source
        dst->source.setBufferOffset = nixAVAudioSource_setBufferOffset; //relative to first buffer in queue
        dst->source.getBuffersCount = nixAVAudioSource_getBuffersCount; //all buffer queue
        dst->source.getBlocksOffset = nixAVAudioSource_getBlocksOffset; //relative to first buffer in queue
        //Recorder
        dst->recorder.alloc     = nixAVAudioRecorder_alloc;
        dst->recorder.free      = nixAVAudioRecorder_free;
        dst->recorder.setCallback = nixAVAudioRecorder_setCallback;
        dst->recorder.start     = nixAVAudioRecorder_start;
        dst->recorder.stop      = nixAVAudioRecorder_stop;
        dst->recorder.flush     = nixAVAudioRecorder_flush;
        dst->recorder.isCapturing = nixAVAudioRecorder_isCapturing;
        dst->recorder.getBuffersFilledCount = nixAVAudioRecorder_getBuffersFilledCount;
        //
        r = NIX_TRUE;
    }
    return r;
}

//

struct STNixAVAudioEngine_;
struct STNixAVAudioSource_;
struct STNixAVAudioQueue_;
struct STNixAVAudioQueuePair_;
struct STNixAVAudioRecorder_;

//------
//Engine
//------

typedef struct STNixAVAudioEngine_ {
    STNixContextRef ctx;
    STNixApiItf     apiItf;
    //srcs
    struct {
        STNixMutexRef   mutex;
        struct STNixAVAudioSource_** arr;
        NixUI32         use;
        NixUI32         sz;
        NixUI32         changingStateCountHint;
    } srcs;
    //
    struct STNixAVAudioRecorder_* rec;
} STNixAVAudioEngine;

void NixAVAudioEngine_init(STNixContextRef ctx, STNixAVAudioEngine* obj);
void NixAVAudioEngine_destroy(STNixAVAudioEngine* obj);
NixBOOL NixAVAudioEngine_srcsAdd(STNixAVAudioEngine* obj, struct STNixAVAudioSource_* src);
void NixAVAudioEngine_tick(STNixAVAudioEngine* obj, const NixBOOL isFinalCleanup);

//------
//QueuePair (Buffers)
//------

typedef struct STNixAVAudioQueuePair_ {
    STNixBufferRef      org;    //original buffer (owned by the user)
    AVAudioPCMBuffer*   cnv;    //converted buffer (owned by the source)
} STNixAVAudioQueuePair;

void NixAVAudioQueuePair_init(STNixAVAudioQueuePair* obj);
void NixAVAudioQueuePair_destroy(STNixAVAudioQueuePair* obj);
void NixAVAudioQueuePair_moveOrg(STNixAVAudioQueuePair* obj, STNixAVAudioQueuePair* to);
void NixAVAudioQueuePair_moveCnv(STNixAVAudioQueuePair* obj, STNixAVAudioQueuePair* to);


//------
//Queue (Buffers)
//------

typedef struct STNixAVAudioQueue_ {
    STNixContextRef         ctx;
    STNixAVAudioQueuePair*  arr;
    NixUI32                 use;
    NixUI32                 sz;
} STNixAVAudioQueue;

void NixAVAudioQueue_init(STNixContextRef ctx, STNixAVAudioQueue* obj);
void NixAVAudioQueue_destroy(STNixAVAudioQueue* obj);
//
NixBOOL NixAVAudioQueue_flush(STNixAVAudioQueue* obj);
NixBOOL NixAVAudioQueue_prepareForSz(STNixAVAudioQueue* obj, const NixUI32 minSz);
NixBOOL NixAVAudioQueue_pushOwning(STNixAVAudioQueue* obj, STNixAVAudioQueuePair* pair);
NixBOOL NixAVAudioQueue_popOrphaning(STNixAVAudioQueue* obj, STNixAVAudioQueuePair* dst);
NixBOOL NixAVAudioQueue_popMovingTo(STNixAVAudioQueue* obj, STNixAVAudioQueue* other);

//------
//Source
//------

typedef struct STNixAVAudioSource_ {
    STNixContextRef         ctx;
    STNixSourceRef          self;
    struct STNixAVAudioEngine_* engp; //parent engine
    STNixAudioDesc          buffsFmt; //first attached buffers' format (defines the converter config)
    AVAudioPlayerNode*      src;    //AVAudioPlayerNode
    AVAudioEngine*          eng;    //AVAudioEngine
    NixFLOAT                volume;
    //queues
    struct {
        STNixMutexRef       mutex;
        void*               conv;   //NixFmtConverter
        STNixSourceCallback callback;
        STNixAVAudioQueue   notify; //buffers (consumed, pending to notify)
        STNixAVAudioQueue   reuse;  //buffers (conversion buffers)
        STNixAVAudioQueue   pend;   //to be played/filled
        NixUI32             pendBlockIdx;  //current sample playing/filling
        NixUI32             pendScheduledCount;
    } queues;
    //packed bools to reduce padding
    NixBOOL                 engStarted;
    NixUI8                  stateBits;  //packed bools to reduce padding, NIX_AVAudioSource_BIT_
} STNixAVAudioSource;

void NixAVAudioSource_init(STNixContextRef ctx, STNixAVAudioSource* obj);
void NixAVAudioSource_destroy(STNixAVAudioSource* obj);
void NixAVAudioSource_scheduleEnqueuedBuffers(STNixAVAudioSource* obj);
NixBOOL NixAVAudioSource_queueBufferForOutput(STNixAVAudioSource* obj, STNixBufferRef buff);
NixBOOL NixAVAudioSource_pendPopOldestBuffLocked_(STNixAVAudioSource* obj);
NixBOOL NixAVAudioSource_pendPopAllBuffsLocked_(STNixAVAudioSource* obj);

#define NIX_AVAudioSource_BIT_isStatic   (0x1 << 0)  //source expects only one buffer, repeats or pauses after playing it
#define NIX_AVAudioSource_BIT_isChanging (0x1 << 1)  //source is changing state after a call to request*()
#define NIX_AVAudioSource_BIT_isRepeat   (0x1 << 2)
#define NIX_AVAudioSource_BIT_isPlaying  (0x1 << 3)
#define NIX_AVAudioSource_BIT_isPaused   (0x1 << 4)
#define NIX_AVAudioSource_BIT_isClosing  (0x1 << 5)
#define NIX_AVAudioSource_BIT_isOrphan   (0x1 << 6)  //source is waiting for close(), wait for the change of state and NixAVAudioSource_release + free.
//
#define NixAVAudioSource_isStatic(OBJ)          (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isStatic) != 0)
#define NixAVAudioSource_isChanging(OBJ)        (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isChanging) != 0)
#define NixAVAudioSource_isRepeat(OBJ)          (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isRepeat) != 0)
#define NixAVAudioSource_isPlaying(OBJ)         (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isPlaying) != 0)
#define NixAVAudioSource_isPaused(OBJ)          (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isPaused) != 0)
#define NixAVAudioSource_isClosing(OBJ)         (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isClosing) != 0)
#define NixAVAudioSource_isOrphan(OBJ)          (((OBJ)->stateBits & NIX_AVAudioSource_BIT_isOrphan) != 0)
//
#define NixAVAudioSource_setIsStatic(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AVAudioSource_BIT_isStatic : (OBJ)->stateBits & ~NIX_AVAudioSource_BIT_isStatic)
#define NixAVAudioSource_setIsChanging(OBJ, V)  (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AVAudioSource_BIT_isChanging : (OBJ)->stateBits & ~NIX_AVAudioSource_BIT_isChanging)
#define NixAVAudioSource_setIsRepeat(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AVAudioSource_BIT_isRepeat : (OBJ)->stateBits & ~NIX_AVAudioSource_BIT_isRepeat)
#define NixAVAudioSource_setIsPlaying(OBJ, V)   (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AVAudioSource_BIT_isPlaying : (OBJ)->stateBits & ~NIX_AVAudioSource_BIT_isPlaying)
#define NixAVAudioSource_setIsPaused(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_AVAudioSource_BIT_isPaused : (OBJ)->stateBits & ~NIX_AVAudioSource_BIT_isPaused)
#define NixAVAudioSource_setIsClosing(OBJ)      (OBJ)->stateBits = ((OBJ)->stateBits | NIX_AVAudioSource_BIT_isClosing)
#define NixAVAudioSource_setIsOrphan(OBJ)       (OBJ)->stateBits = ((OBJ)->stateBits | NIX_AVAudioSource_BIT_isOrphan)

//------
//Recorder
//------

typedef struct STNixAVAudioRecorder_ {
    STNixContextRef         ctx;
    NixBOOL                 engStarted;
    STNixEngineRef          engRef;
    STNixRecorderRef        selfRef;
    AVAudioEngine*          eng;    //AVAudioEngine
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
        STNixAVAudioQueue   notify;
        STNixAVAudioQueue   reuse;
        //filling
        struct {
            NixSI32         iCurSample; //at first buffer in 'reuse'
        } filling;
    } queues;
} STNixAVAudioRecorder;

void NixAVAudioRecorder_init(STNixContextRef ctx,STNixAVAudioRecorder* obj);
void NixAVAudioRecorder_destroy(STNixAVAudioRecorder* obj);
//
NixBOOL NixAVAudioRecorder_prepare(STNixAVAudioRecorder* obj, STNixAVAudioEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
NixBOOL NixAVAudioRecorder_setCallback(STNixAVAudioRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL NixAVAudioRecorder_start(STNixAVAudioRecorder* obj);
NixBOOL NixAVAudioRecorder_stop(STNixAVAudioRecorder* obj);
NixBOOL NixAVAudioRecorder_flush(STNixAVAudioRecorder* obj);
void NixAVAudioRecorder_notifyBuffers(STNixAVAudioRecorder* obj, const NixBOOL discardWithoutNotifying);

//------
//NixFmtConverter
//------

void NixFmtConverter_buffFmtToAudioDesc(AVAudioFormat* buffFmt, STNixAudioDesc* dst);

//------
//Engine
//------

void NixAVAudioEngine_init(STNixContextRef ctx, STNixAVAudioEngine* obj){
    memset(obj, 0, sizeof(STNixAVAudioEngine));
    //
    NixContext_set(&obj->ctx, ctx);
    nixAVAudioEngine_getApiItf(&obj->apiItf);
    //srcs
    {
        obj->srcs.mutex = NixContext_mutex_alloc(obj->ctx);
    }
}

void NixAVAudioEngine_destroy(STNixAVAudioEngine* obj){
    //srcs
    {
        //cleanup
        while(obj->srcs.arr != NULL && obj->srcs.use > 0){
            NixAVAudioEngine_tick(obj, NIX_TRUE);
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

NixBOOL NixAVAudioEngine_srcsAdd(STNixAVAudioEngine* obj, struct STNixAVAudioSource_* src){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        NixMutex_lock(obj->srcs.mutex);
        {
            //resize array (if necesary)
            if(obj->srcs.use >= obj->srcs.sz){
                const NixUI32 szN = obj->srcs.use + 4;
                STNixAVAudioSource** arrN = (STNixAVAudioSource**)NixContext_mrealloc(obj->ctx, obj->srcs.arr, sizeof(STNixAVAudioSource*) * szN, "STNixAVAudioEngine::srcsN");
                if(arrN != NULL){
                    obj->srcs.arr = arrN;
                    obj->srcs.sz = szN;
                }
            }
            //add
            if(obj->srcs.use >= obj->srcs.sz){
                NIX_PRINTF_ERROR("nixAVAudioSource_create::STNixAVAudioEngine::srcs failed (no allocated space).\n");
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

void NixAVAudioEngine_removeSrcRecordLocked_(STNixAVAudioEngine* obj, NixSI32* idx){
    STNixAVAudioSource* src = obj->srcs.arr[*idx];
    if(src != NULL){
        NixAVAudioSource_destroy(src);
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

void NixAVAudioEngine_tick_addQueueNotifSrcLocked_(STNixNotifQueue* notifs, STNixAVAudioSource* src){
    if(src->queues.notify.use > 0){
        NixSI32 i; for(i = 0; i < src->queues.notify.use; i++){
            STNixAVAudioQueuePair* pair = &src->queues.notify.arr[i];
            if(!NixNotifQueue_addBuff(notifs, src->self, src->queues.callback, pair->org)){
                NIX_ASSERT(NIX_FALSE); //program logic error
            }
        }
        if(!NixAVAudioQueue_flush(&src->queues.notify)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        }
    }
}
    
void NixAVAudioEngine_tick(STNixAVAudioEngine* obj, const NixBOOL isFinalCleanup){
    if(obj != NULL){
        //srcs
        {
            STNixNotifQueue notifs;
            NixNotifQueue_init(obj->ctx, &notifs);
            NixMutex_lock(obj->srcs.mutex);
            if(obj->srcs.arr != NULL && obj->srcs.use > 0){
                NixUI32 changingStateCount = 0;
                //NIX_PRINTF_INFO("NixAVAudioEngine_tick::%d sources.\n", obj->srcs.use);
                NixSI32 i; for(i = 0; i < (NixSI32)obj->srcs.use; ++i){
                    STNixAVAudioSource* src = obj->srcs.arr[i];
                    //NIX_PRINTF_INFO("NixAVAudioEngine_tick::source(#%d/%d).\n", i + 1, obj->srcs.use);
                    //release orphan
                    if(NixAVAudioSource_isOrphan(src) || isFinalCleanup){
                        if(src->eng != nil){
                            if([src->eng isRunning]){
                                [src->eng stop];
                            }
                            if(![src->eng isRunning]){
                                if(src->src != nil){
                                    [src->src release];
                                    src->src = nil;
                                }
                            }
                        }
                    }
                    //process
                    if(src->src == nil){
                        //remove
                        //NIX_PRINTF_INFO("NixAVAudioEngine_tick::source(#%d/%d); remove-NULL.\n", i + 1, obj->srcs.use);
                        NixAVAudioEngine_removeSrcRecordLocked_(obj, &i);
                        src = NULL;
                    } else {
                        //post-process
                        if(src != NULL){
                            //add to notify queue
                            {
                                NixMutex_lock(src->queues.mutex);
                                {
                                    NixAVAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
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
                //NIX_PRINTF_INFO("NixAVAudioEngine_tick::notify %d.\n", notifs.use);
                NixUI32 i;
                for(i = 0; i < notifs.use; ++i){
                    STNixSourceNotif* n = &notifs.arr[i];
                    //NIX_PRINTF_INFO("NixAVAudioEngine_tick::notify(#%d/%d).\n", i + 1, notifs.use);
                    if(n->callback.func != NULL){
                        (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
                    }
                }
            }
            //NIX_PRINTF_INFO("NixAVAudioEngine_tick::NixNotifQueue_destroy.\n");
            NixNotifQueue_destroy(&notifs);
        }
        //recorder
        if(obj->rec != NULL){
            NixAVAudioRecorder_notifyBuffers(obj->rec, NIX_FALSE);
        }
    }
}

//------
//QueuePair (Buffers)
//------

void NixAVAudioQueuePair_init(STNixAVAudioQueuePair* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixAVAudioQueuePair_destroy(STNixAVAudioQueuePair* obj){
    if(!NixBuffer_isNull(obj->org)){
        NixBuffer_release(&obj->org);
        NixBuffer_null(&obj->org);
    }
    if(obj->cnv != nil){
        [obj->cnv release];
        obj->cnv = nil;
    }
}

void NixAVAudioQueuePair_moveOrg(STNixAVAudioQueuePair* obj, STNixAVAudioQueuePair* to){
    NixBuffer_set(&to->org, obj->org);
    NixBuffer_release(&obj->org);
    NixBuffer_null(&obj->org);
}

void NixAVAudioQueuePair_moveCnv(STNixAVAudioQueuePair* obj, STNixAVAudioQueuePair* to){
    if(to->cnv != nil){
        [to->cnv release];
        to->cnv = nil;
    }
    to->cnv = obj->cnv;
    obj->cnv = nil;
}

//------
//Queue (Buffers)
//------

void NixAVAudioQueue_init(STNixContextRef ctx, STNixAVAudioQueue* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
}

void NixAVAudioQueue_destroy(STNixAVAudioQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNixAVAudioQueuePair* b = &obj->arr[i];
            NixAVAudioQueuePair_destroy(b);
        }
        NixContext_mfree(obj->ctx, obj->arr);
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

NixBOOL NixAVAudioQueue_flush(STNixAVAudioQueue* obj){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        if(obj->arr != NULL){
            NixUI32 i; for(i = 0; i < obj->use; i++){
                STNixAVAudioQueuePair* b = &obj->arr[i];
                NixAVAudioQueuePair_destroy(b);
            }
            obj->use = 0;
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixAVAudioQueue_prepareForSz(STNixAVAudioQueue* obj, const NixUI32 minSz){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        //resize array (if necesary)
        if(minSz > obj->sz){
            const NixUI32 szN = minSz;
            STNixAVAudioQueuePair* arrN = (STNixAVAudioQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixAVAudioQueuePair) * szN, "NixAVAudioQueue_prepareForSz::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //analyze
        if(minSz > obj->sz){
            NIX_PRINTF_ERROR("NixAVAudioQueue_prepareForSz failed (no allocated space).\n");
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAVAudioQueue_pushOwning(STNixAVAudioQueue* obj, STNixAVAudioQueuePair* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNixAVAudioQueuePair* arrN = (STNixAVAudioQueuePair*)NixContext_mrealloc(obj->ctx, obj->arr, sizeof(STNixAVAudioQueuePair) * szN, "NixAVAudioQueue_pushOwning::arrN");
            if(arrN != NULL){
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //add
        if(obj->use >= obj->sz){
            NIX_PRINTF_ERROR("NixAVAudioQueue_pushOwning failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAVAudioQueue_popOrphaning(STNixAVAudioQueue* obj, STNixAVAudioQueuePair* dst){
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

NixBOOL NixAVAudioQueue_popMovingTo(STNixAVAudioQueue* obj, STNixAVAudioQueue* other){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioQueuePair pair;
    if(!NixAVAudioQueue_popOrphaning(obj, &pair)){
        //
    } else {
        if(!NixAVAudioQueue_pushOwning(other, &pair)){
            //program logic error
            NIX_ASSERT(NIX_FALSE);
            NixAVAudioQueuePair_destroy(&pair);
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//Source
//------

void NixAVAudioSource_init(STNixContextRef ctx, STNixAVAudioSource* obj){
    memset(obj, 0, sizeof(STNixAVAudioSource));
    NixContext_set(&obj->ctx, ctx);
    obj->volume = 1.f;
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixAVAudioQueue_init(ctx, &obj->queues.notify);
        NixAVAudioQueue_init(ctx, &obj->queues.pend);
        NixAVAudioQueue_init(ctx, &obj->queues.reuse);
    }
}
   
void NixAVAudioSource_destroy(STNixAVAudioSource* obj){
    //src
    if(obj->src != nil){ [obj->src release]; obj->src = nil; }
    if(obj->eng != nil){ [obj->eng release]; obj->eng = nil; }
    //queues
    {
        if(obj->queues.conv != NULL){
            NixFmtConverter_free(obj->queues.conv);
            obj->queues.conv = NULL;
        }
        NixAVAudioQueue_destroy(&obj->queues.pend);
        NixAVAudioQueue_destroy(&obj->queues.reuse);
        NixAVAudioQueue_destroy(&obj->queues.notify);
        NixMutex_free(&obj->queues.mutex);
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

void NixAVAudioSource_queueBufferScheduleCallback_(STNixAVAudioSource* obj, AVAudioPCMBuffer* cnvBuff){
    NixMutex_lock(obj->queues.mutex);
    {
        NIX_ASSERT(obj->queues.pendScheduledCount > 0)
        if(obj->queues.pendScheduledCount > 0){
            //Note: AVFAudio calls callbacks not in the same order of scheduling.
/*#         ifdef NIX_ASSERTS_ACTIVATED
            if(obj->queues.pend.arr[0].cnv != cnvBuff){
                NIX_PRINTF_WARNING("Unqueued buffer does not match oldest buffer.\n");
                NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
                    const STNixAVAudioQueuePair* pair = &obj->queues.pend.arr[i];
                    NIX_PRINTF_WARNING("Buffer #%d/%d: %lld vs %lld%s.\n", (i + 1), obj->queues.pend.use, (long long)pair->cnv, (long long)cnvBuff, pair->cnv == cnvBuff ? " MATCH": "");
                }
            }
            NIX_ASSERT(obj->queues.pend.arr[0].cnv == cnvBuff)
#           endif*/
            --obj->queues.pendScheduledCount;
        }
        if(NixAVAudioSource_isStatic(obj)){
            //schedule again
            if(NixAVAudioSource_isPlaying(obj) && !NixAVAudioSource_isPaused(obj) && NixAVAudioSource_isRepeat(obj) && !NixAVAudioSource_isOrphan(obj) && obj->queues.pend.use == 1 && obj->queues.pendScheduledCount == 0){
                ++obj->queues.pendScheduledCount;
                NixMutex_unlock(obj->queues.mutex);
                {
                    [obj->src scheduleBuffer:cnvBuff completionCallbackType:AVAudioPlayerNodeCompletionDataRendered completionHandler:^(AVAudioPlayerNodeCompletionCallbackType cbType) {
                        //printf("completionCallbackType: %s.\n", cbType == AVAudioPlayerNodeCompletionDataConsumed ? "DataConsumed" : cbType == AVAudioPlayerNodeCompletionDataRendered? "DataRendered" : cbType == AVAudioPlayerNodeCompletionDataPlayedBack ? "DataPlayedBack" : "unknown");
                        if(cbType == AVAudioPlayerNodeCompletionDataRendered){
                            NixAVAudioSource_queueBufferScheduleCallback_(obj, cnvBuff);
                        }
                    }];
                }
                NixMutex_lock(obj->queues.mutex);
            } else {
                //pause
                NixAVAudioSource_setIsPaused(obj, NIX_TRUE);
            }
        } else {
            if(!NixAVAudioSource_pendPopOldestBuffLocked_(obj)){
                //remove from queue, to pend
            } else {
                //buff moved from pend to reuse and notif
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
}

void NixAVAudioSource_scheduleEnqueuedBuffers(STNixAVAudioSource* obj){
    NixMutex_lock(obj->queues.mutex);
    while(obj->queues.pendScheduledCount < obj->queues.pend.use){
        STNixAVAudioQueuePair* pair = &obj->queues.pend.arr[obj->queues.pendScheduledCount];
        NIX_ASSERT(pair->cnv != nil)
        if(pair->cnv == nil){
            //program logic error
            break;
        } else {
            AVAudioPCMBuffer* cnv = pair->cnv;
            ++obj->queues.pendScheduledCount;
            NixMutex_unlock(obj->queues.mutex);
            {
                [obj->src scheduleBuffer:cnv completionCallbackType:AVAudioPlayerNodeCompletionDataRendered completionHandler:^(AVAudioPlayerNodeCompletionCallbackType cbType) {
                    //printf("completionCallbackType: %s.\n", cbType == AVAudioPlayerNodeCompletionDataConsumed ? "DataConsumed" : cbType == AVAudioPlayerNodeCompletionDataRendered? "DataRendered" : cbType == AVAudioPlayerNodeCompletionDataPlayedBack ? "DataPlayedBack" : "unknown");
                    if(cbType == AVAudioPlayerNodeCompletionDataRendered){
                        NixAVAudioSource_queueBufferScheduleCallback_(obj, cnv);
                    }
                }];
            }
            NixMutex_lock(obj->queues.mutex);
        }
    }
    NixMutex_unlock(obj->queues.mutex);
}

NixBOOL NixAVAudioSource_queueBufferForOutput(STNixAVAudioSource* obj, STNixBufferRef pBuff){
    NixBOOL r = NIX_FALSE;
    STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
    if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
        //error
    } else if(buff->desc.blockAlign <= 0){
        //error
    } else {
        STNixAVAudioQueuePair pair;
        NixAVAudioQueuePair_init(&pair);
        //prepare conversion buffer (if necesary)
        if(obj->eng != NULL && obj->queues.conv != NULL){
            AVAudioFormat* outFmt = [[obj->eng outputNode] outputFormatForBus:0];
            //create copy buffer
            const NixUI32 buffBlocksMax    = (buff->sz / buff->desc.blockAlign);
            const NixUI32 blocksReq        = NixFmtConverter_blocksForNewFrequency(buffBlocksMax, obj->buffsFmt.samplerate, [outFmt sampleRate]);
            //try to reuse buffer
            {
                STNixAVAudioQueuePair reuse;
                if(NixAVAudioQueue_popOrphaning(&obj->queues.reuse, &reuse)){
                    //reusable buffer
                    NIX_ASSERT(reuse.org.ptr == NULL) //program logic error
                    NIX_ASSERT(reuse.cnv != NULL) //program logic error
                    if([reuse.cnv frameCapacity] < blocksReq){
                        [reuse.cnv release];
                        reuse.cnv = nil;
                    } else {
                        pair.cnv = reuse.cnv;
                        reuse.cnv = nil;
                    }
                    NixAVAudioQueuePair_destroy(&reuse);
                }
            }
            //create new buffer
            if(pair.cnv == nil){
                pair.cnv = [[AVAudioPCMBuffer alloc] initWithPCMFormat:outFmt frameCapacity:blocksReq];
                if(pair.cnv == NULL){
                    NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::pair.cnv could be allocated.\n");
                }
            }
            //convert
            if(pair.cnv == NULL){
                r = NIX_FALSE;
            } else if(!NixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &buff->desc, buff->ptr, 0)){
                NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                r = NIX_FALSE;
            } else {
                r = NIX_TRUE;
                STNixAudioDesc srcFmt;
                srcFmt.channels = [outFmt channelCount];
                srcFmt.samplerate = [outFmt sampleRate];
                NixBOOL isInterleavedDst = [outFmt isInterleaved];
                NixUI32 iCh, maxChannels = NixFmtConverter_maxChannels();
                switch([outFmt commonFormat]) {
                    case AVAudioPCMFormatFloat32:
                        srcFmt.samplesFormat = ENNixSampleFmt_Float;
                        srcFmt.bitsPerSample = 32;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!NixFmtConverter_setPtrAtDst(obj->queues.conv, iCh, pair.cnv.floatChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtDst failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    case AVAudioPCMFormatInt16:
                        srcFmt.samplesFormat = ENNixSampleFmt_Int;
                        srcFmt.bitsPerSample = 16;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!NixFmtConverter_setPtrAtDst(obj->queues.conv, iCh, pair.cnv.int16ChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtDst failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    case AVAudioPCMFormatInt32:
                        srcFmt.samplesFormat = ENNixSampleFmt_Int;
                        srcFmt.bitsPerSample = 32;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!NixFmtConverter_setPtrAtDst(obj->queues.conv, iCh, pair.cnv.int32ChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_setPtrAtDst failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    default:
                        NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::unexpected 'commonFormat'.\n");
                        r = NIX_FALSE;
                        break;
                }
                //conver
                if(r){
                    const NixUI32 srcBlocks = (buff->use / buff->desc.blockAlign);
                    const NixUI32 dstBlocks = [pair.cnv frameCapacity];
                    NixUI32 ammBlocksRead = 0;
                    NixUI32 ammBlocksWritten = 0;
                    if(!NixFmtConverter_convert(obj->queues.conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                        NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_convert failed from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s).\n"
                                         , obj->buffsFmt.samplerate
                                         , obj->buffsFmt.channels
                                         , obj->buffsFmt.bitsPerSample
                                         , obj->buffsFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->buffsFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                         , srcFmt.samplerate
                                         , srcFmt.channels
                                         , srcFmt.bitsPerSample
                                         , srcFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : srcFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                         );
                        r = NIX_FALSE;
                    } else {
                        /*
#                       ifdef NIX_ASSERTS_ACTIVATED
                        if((ammBlocksRead * 100 / srcBlocks) < 90 || (ammBlocksWritten * 100 / dstBlocks) < 90){
                            NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixFmtConverter_convert under-consumption from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s) = %d%% of %u src-frames consumed, %d%% of %u dst-frames populated.\n"
                                             , obj->buffsFmt.samplerate
                                             , obj->buffsFmt.channels
                                             , obj->buffsFmt.bitsPerSample
                                             , obj->buffsFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : obj->buffsFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                             , srcFmt.samplerate
                                             , srcFmt.channels
                                             , srcFmt.bitsPerSample
                                             , srcFmt.samplesFormat == ENNixSampleFmt_Int ? "int" : srcFmt.samplesFormat == ENNixSampleFmt_Float ? "float" : "unknown"
                                             //
                                             , ammBlocksRead * 100 / srcBlocks, srcBlocks, ammBlocksWritten * 100 / dstBlocks, dstBlocks
                                             );
                            NIX_ASSERT(FALSE) //program logic error
                        }
#                       endif
                        */
                        [pair.cnv setFrameLength:ammBlocksWritten];
                    }
                }
            }
        }
        //add to queue
        if(r){
            NixBuffer_set(&pair.org, pBuff);
            NixMutex_lock(obj->queues.mutex);
            {
                if(!NixAVAudioQueue_pushOwning(&obj->queues.pend, &pair)){
                    NIX_PRINTF_ERROR("NixAVAudioSource_queueBufferForOutput::NixAVAudioQueue_pushOwning failed.\n");
                    r = NIX_FALSE;
                } else {
                    //added to queue
                    NixAVAudioQueue_prepareForSz(&obj->queues.reuse, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    NixAVAudioQueue_prepareForSz(&obj->queues.notify, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    //this is the first buffer i the queue
                    if(obj->queues.pend.use == 1){
                        obj->queues.pendBlockIdx = 0;
                    }
                }
            }
            NixMutex_unlock(obj->queues.mutex);
        }
        if(!r){
            NixAVAudioQueuePair_destroy(&pair);
        }
    }
    return r;
}

NixBOOL NixAVAudioSource_pendPopOldestBuffLocked_(STNixAVAudioSource* obj){
    NixBOOL r = NIX_FALSE;
    if(obj->queues.pend.use > 0){
        STNixAVAudioQueuePair pair;
        if(!NixAVAudioQueue_popOrphaning(&obj->queues.pend, &pair)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        } else {
            //move "cnv" to reusable queue
            if(pair.cnv != NULL){
                STNixAVAudioQueuePair reuse;
                NixAVAudioQueuePair_init(&reuse);
                NixAVAudioQueuePair_moveCnv(&pair, &reuse);
                if(!NixAVAudioQueue_pushOwning(&obj->queues.reuse, &reuse)){
                    NIX_PRINTF_ERROR("NixAVAudioSource_pendPopOldestBuffLocked_::NixAVAudioQueue_pushOwning(reuse) failed.\n");
                    NixAVAudioQueuePair_destroy(&reuse);
                }
            }
            //move "org" to notify queue
            if(!NixBuffer_isNull(pair.org)){
                STNixAVAudioQueuePair notif;
                NixAVAudioQueuePair_init(&notif);
                NixAVAudioQueuePair_moveOrg(&pair, &notif);
                if(!NixAVAudioQueue_pushOwning(&obj->queues.notify, &notif)){
                    NIX_PRINTF_ERROR("NixAVAudioSource_pendPopOldestBuffLocked_::NixAVAudioQueue_pushOwning(notify) failed.\n");
                    NixAVAudioQueuePair_destroy(&notif);
                }
            }
            NIX_ASSERT(pair.org.ptr == NULL); //program logic error
            NIX_ASSERT(pair.cnv == NULL); //program logic error
            NixAVAudioQueuePair_destroy(&pair);
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAVAudioSource_pendPopAllBuffsLocked_(STNixAVAudioSource* obj){
    NixBOOL r = NIX_TRUE;
    while(obj->queues.pend.use > 0){
        if(!NixAVAudioSource_pendPopOldestBuffLocked_(obj)){
            r = NIX_FALSE;
            break;
        }
    }
    return r;
}

//------
//Recorder
//------

void NixAVAudioRecorder_init(STNixContextRef ctx, STNixAVAudioRecorder* obj){
    memset(obj, 0, sizeof(*obj));
    NixContext_set(&obj->ctx, ctx);
    //cfg
    {
        //
    }
    //queues
    {
        obj->queues.mutex = NixContext_mutex_alloc(obj->ctx);
        NixAVAudioQueue_init(ctx, &obj->queues.notify);
        NixAVAudioQueue_init(ctx, &obj->queues.reuse);
    }
}

void NixAVAudioRecorder_destroy(STNixAVAudioRecorder* obj){
    //queues
    {
        NixMutex_lock(obj->queues.mutex);
        {
            NixAVAudioQueue_destroy(&obj->queues.notify);
            NixAVAudioQueue_destroy(&obj->queues.reuse);
            if(obj->queues.conv != NULL){
                NixFmtConverter_free(obj->queues.conv);
                obj->queues.conv = NULL;
            }
        }
        NixMutex_unlock(obj->queues.mutex);
        NixMutex_free(&obj->queues.mutex);
    }
    //
    if(obj->eng != nil){
        AVAudioInputNode* input = [obj->eng inputNode];
        [input removeTapOnBus:0];
        [obj->eng stop];
        [obj->eng release];
        obj->eng = nil;
    }
    //
    if(obj->engRef.ptr != NULL){
        STNixAVAudioEngine* eng = (STNixAVAudioEngine*)NixSharedPtr_getOpq(obj->engRef.ptr);
        if(eng != NULL && eng->rec == obj){
            eng->rec = NULL;
        }
        NixEngine_release(&obj->engRef);
        obj->engRef.ptr = NULL;
    }
    NixContext_release(&obj->ctx);
    NixContext_null(&obj->ctx);
}

void NixAVAudioRecorder_consumeInputBuffer_(STNixAVAudioRecorder* obj, AVAudioPCMBuffer* buff){
    if(obj->queues.conv != NULL){
        NixMutex_lock(obj->queues.mutex);
        {
            NixUI32 inIdx = 0;
            const NixUI32 inSz = [buff frameLength];
            while(inIdx < inSz){
                if(obj->queues.reuse.use <= 0){
                    //move oldest-notify buffer to reuse
                    if(!NixAVAudioQueue_popMovingTo(&obj->queues.notify, &obj->queues.reuse)){
                        //program logic error
                        NIX_ASSERT(NIX_FALSE);
                        break;
                    }
                } else {
                    STNixAVAudioQueuePair* pair = &obj->queues.reuse.arr[0];
                    STNixPCMBuffer* org = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
                    if(org == NULL || org->desc.blockAlign <= 0){
                        //just remove
                        STNixAVAudioQueuePair pair;
                        if(!NixAVAudioQueue_popOrphaning(&obj->queues.reuse, &pair)){
                            NIX_ASSERT(NIX_FALSE);
                            //program logic error
                            break;
                        }
                        NixAVAudioQueuePair_destroy(&pair);
                    } else {
                        const NixUI32 outSz = (org->sz / org->desc.blockAlign);
                        const NixUI32 outAvail = (obj->queues.filling.iCurSample >= outSz ? 0 : outSz - obj->queues.filling.iCurSample);
                        const NixUI32 inAvail = inSz - inIdx;
                        NixUI32 ammBlocksRead = 0, ammBlocksWritten = 0;
                        if(outAvail > 0 && inAvail > 0){
                            //convert
                            NixUI32 iCh;
                            const NixUI32 maxChannels = NixFmtConverter_maxChannels();
                            //dst
                            NixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv, &org->desc, org->ptr, obj->queues.filling.iCurSample);
                            //src
                            AVAudioFormat* orgFmt = [ buff format];
                            NixBOOL isInterleavedSrc = [orgFmt isInterleaved];
                            NixUI32 chCountSrc = [orgFmt channelCount];
                            switch([orgFmt commonFormat]) {
                                case AVAudioPCMFormatFloat32:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 32;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        NixFmtConverter_setPtrAtSrc(obj->queues.conv, iCh,  buff.floatChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                case AVAudioPCMFormatInt16:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 16;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        NixFmtConverter_setPtrAtSrc(obj->queues.conv, iCh,  buff.int16ChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                case AVAudioPCMFormatInt32:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 32;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        NixFmtConverter_setPtrAtSrc(obj->queues.conv, iCh,  buff.int32ChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                default:
                                    break;
                            }
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
                            if(!NixAVAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
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


NixBOOL NixAVAudioRecorder_prepare(STNixAVAudioRecorder* obj, STNixAVAudioEngine* eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    NixBOOL r = NIX_FALSE;
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.conv == NULL && audioDesc->blockAlign > 0){
        obj->eng = [[AVAudioEngine alloc] init];
        {
            void* conv = NixFmtConverter_alloc(eng->ctx);
            AVAudioInputNode* input = [obj->eng inputNode];
            AVAudioFormat* inFmt = [input inputFormatForBus:0];
            const NixUI32 inSampleRate = [inFmt sampleRate];
            STNixAudioDesc inDesc;
            NixFmtConverter_buffFmtToAudioDesc(inFmt, &inDesc);
            if(!NixFmtConverter_prepare(conv, &inDesc, audioDesc)){
                NIX_PRINTF_ERROR("NixAVAudioRecorder_prepare::NixFmtConverter_prepare failed.\n");
                NixFmtConverter_free(conv);
                conv = NULL;
            } else {
                //allocate reusable buffers
                while(obj->queues.reuse.use < buffersCount){
                    STNixAVAudioQueuePair pair;
                    NixAVAudioQueuePair_init(&pair);
                    pair.org = (*eng->apiItf.buffer.alloc)(eng->ctx, audioDesc, NULL, audioDesc->blockAlign * blocksPerBuffer);
                    if(pair.org.ptr == NULL){
                        NIX_PRINTF_ERROR("NixAVAudioRecorder_prepare::pair.org allocation failed.\n");
                        break;
                    } else {
                        NixAVAudioQueue_pushOwning(&obj->queues.reuse, &pair);
                    }
                }
                //
                if(obj->queues.reuse.use <= 0){
                    NIX_PRINTF_ERROR("NixAVAudioRecorder_prepare::no reusable buffer could be allocated.\n");
                } else {
                    r = NIX_TRUE;
                    //prepared
                    obj->queues.filling.iCurSample = 0;
                    obj->queues.conv = conv; conv = NULL; //consume
                    //cfg
                    obj->cfg.fmt = *audioDesc;
                    obj->cfg.maxBuffers = buffersCount;
                    obj->cfg.blocksPerBuffer = blocksPerBuffer;
                    //
                    NixMutex_unlock(obj->queues.mutex);
                    //install tap (unlocked)
                    {
                        [input installTapOnBus:0 bufferSize:(inSampleRate / 30) format:inFmt block:^(AVAudioPCMBuffer * _Nonnull buffer, AVAudioTime * _Nonnull when) {
                            NixAVAudioRecorder_consumeInputBuffer_(obj, buffer);
                            //printf("AVFAudio recorder buffer with %d samples (%d samples in memory).\n", [buffer frameLength], obj->in.samples.cur);
                        }];
                    }
                    NixMutex_lock(obj->queues.mutex);
                    [obj->eng prepare];
                }
            }
            //release (if not consumed)
            if(conv != NULL){
                NixFmtConverter_free(conv);
                conv = NULL;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

NixBOOL NixAVAudioRecorder_setCallback(STNixAVAudioRecorder* obj, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    {
        obj->callback.func = callback;
        obj->callback.data = callbackData;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL NixAVAudioRecorder_start(STNixAVAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(!obj->engStarted){
        NSError* err = nil;
        if(![obj->eng startAndReturnError:&err]){
            NIX_PRINTF_ERROR("nixAVAudioRecorder_create, AVAudioEngine::startAndReturnError failed: '%s'.\n", err == nil ? "unknown" : [[err description] UTF8String]);
            r = NIX_FALSE;
        } else {
            obj->engStarted = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL NixAVAudioRecorder_stop(STNixAVAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(obj->eng != nil){
        [obj->eng stop];
        obj->engStarted = NIX_FALSE;
    }
    NixAVAudioRecorder_flush(obj);
    return r;
}

NixBOOL NixAVAudioRecorder_flush(STNixAVAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    //move filling buffer to notify (if data is available)
    NixMutex_lock(obj->queues.mutex);
    if(obj->queues.reuse.use > 0){
        STNixAVAudioQueuePair* pair = &obj->queues.reuse.arr[0];
        if(!NixBuffer_isNull(pair->org) && ((STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr))->use > 0){
            obj->queues.filling.iCurSample = 0;
            if(!NixAVAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                //program logic error
                r = NIX_FALSE;
            }
        }
    }
    NixMutex_unlock(obj->queues.mutex);
    return r;
}

void NixAVAudioRecorder_notifyBuffers(STNixAVAudioRecorder* obj, const NixBOOL discardWithoutNotifying){
    NixMutex_lock(obj->queues.mutex);
    {
        const NixUI32 maxProcess = obj->queues.notify.use;
        NixUI32 ammProcessed = 0;
        while(ammProcessed < maxProcess && obj->queues.notify.use > 0){
            STNixAVAudioQueuePair pair;
            if(!NixAVAudioQueue_popOrphaning(&obj->queues.notify, &pair)){
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
                if(!NixAVAudioQueue_pushOwning(&obj->queues.reuse, &pair)){
                    //program logic error
                    NIX_ASSERT(NIX_FALSE);
                    NixAVAudioQueuePair_destroy(&pair);
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

STNixEngineRef nixAVAudioEngine_alloc(STNixContextRef ctx){
    STNixEngineRef r = STNixEngineRef_Zero;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixContext_malloc(ctx, sizeof(STNixAVAudioEngine), "STNixAVAudioEngine");
    if(obj != NULL){
        NixAVAudioEngine_init(ctx, obj);
        if(NULL == (r.ptr = NixSharedPtr_alloc(ctx.itf, obj, "nixAVAudioEngine_alloc"))){
            NIX_PRINTF_ERROR("nixAVAudioEngine_create::NixSharedPtr_alloc failed.\n");
        } else {
            r.itf = &obj->apiItf.engine;
            obj = NULL; //consume
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAVAudioEngine_destroy(obj);
            NixContext_mfree(ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAVAudioEngine_free(STNixEngineRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixAVAudioEngine_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

void nixAVAudioEngine_printCaps(STNixEngineRef pObj){
}

NixBOOL nixAVAudioEngine_ctxIsActive(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioEngine_ctxActivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioEngine_ctxDeactivate(STNixEngineRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

void nixAVAudioEngine_tick(STNixEngineRef pObj){
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        NixAVAudioEngine_tick(obj, NIX_FALSE);
    }
}

//Factory

STNixSourceRef nixAVAudioEngine_allocSource(STNixEngineRef ref){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.source.alloc != NULL){
        r = (*obj->apiItf.source.alloc)(ref);
    }
    return r;
}

STNixBufferRef nixAVAudioEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixBufferRef r = STNixBufferRef_Zero;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.buffer.alloc != NULL){
        r = (*obj->apiItf.buffer.alloc)(obj->ctx, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

STNixRecorderRef nixAVAudioEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixAVAudioEngine* obj = (STNixAVAudioEngine*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL && obj->apiItf.recorder.alloc != NULL){
        r = (*obj->apiItf.recorder.alloc)(ref, audioDesc, buffersCount, blocksPerBuffer);
    }
    return r;
}

//------
//Source API
//------
  
void nixAVAudioSource_removeAllBuffersAndNotify_(STNixAVAudioSource* obj);

STNixSourceRef nixAVAudioSource_alloc(STNixEngineRef pEng){
    STNixSourceRef r = STNixSourceRef_Zero;
    STNixAVAudioEngine* eng = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixContext_malloc(eng->ctx, sizeof(STNixAVAudioSource), "STNixAVAudioSource");
        if(obj != NULL){
            NixAVAudioSource_init(eng->ctx, obj);
            //
            obj->engp = eng;
            //
            obj->eng = [[AVAudioEngine alloc] init];
            obj->src = [[AVAudioPlayerNode alloc] init];
            {
                //attach
                [obj->eng attachNode:obj->src];
                //connect
                [obj->eng connect:obj->src to:[obj->eng outputNode] format:nil];
                //
                if(!obj->engStarted){
                    NSError* err = nil;
                    [obj->eng prepare];
                    if(![obj->eng startAndReturnError:&err]){
                        NIX_PRINTF_ERROR("nixAVAudioSource_create, AVAudioEngine::startAndReturnError failed: '%s'.\n", err == nil ? "unknown" : [[err description] UTF8String]);
                        [obj->src release]; obj->src = nil;
                        [obj->eng release]; obj->eng = nil;
                        NixContext_mfree(eng->ctx, obj);
                        obj = NULL;
                    } else {
                        obj->engStarted = NIX_TRUE;
                    }
                }
            }
        }
        //add to engine
        if(!NixAVAudioEngine_srcsAdd(eng, obj)){
            NIX_PRINTF_ERROR("nixAVAudioSource_create::NixAVAudioEngine_srcsAdd failed.\n");
        } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "NixAVAudioEngine_srcsAdd"))){
            NIX_PRINTF_ERROR("nixAVAudioEngine_create::NixSharedPtr_alloc failed.\n");
        } else {
            r.itf = &eng->apiItf.source;
            obj->self = r;
            obj = NULL; //consume
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAVAudioSource_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAVAudioSource_free(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            //set final state
            {
                //nullify self-reference before notifying
                //to avoid reviving this object during final notification.
                NixSource_null(&obj->self);
                //Flag as orphan, for cleanup inside 'tick'
                NixAVAudioSource_setIsOrphan(obj);
            }
            //stop engine
            {
                if(obj->eng != nil && [obj->eng isRunning]){
                    [obj->eng stop];
                }
            }
            //flush all pending buffers
            {
                nixAVAudioSource_removeAllBuffersAndNotify_(obj);
            }
        }
    }
}

void nixAVAudioSource_removeAllBuffersAndNotify_(STNixAVAudioSource* obj){
    STNixNotifQueue notifs;
    NixNotifQueue_init(obj->ctx, &notifs);
    //move all pending buffers to notify
    NixMutex_lock(obj->queues.mutex);
    {
        NixAVAudioSource_pendPopAllBuffsLocked_(obj);
        NixAVAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, obj);
    }
    NixMutex_unlock(obj->queues.mutex);
    //notify
    {
        NixUI32 i;
        for(i = 0; i < notifs.use; ++i){
            STNixSourceNotif* n = &notifs.arr[i];
            NIX_PRINTF_INFO("nixAVAudioSource_removeAllBuffersAndNotify_::notify(#%d/%d).\n", i + 1, notifs.use);
            if(n->callback.func != NULL){
                (*n->callback.func)(&n->source, n->buffs, n->buffsUse, n->callback.data);
            }
        }
    }
    NixNotifQueue_destroy(&notifs);
}

void nixAVAudioSource_setCallback(STNixSourceRef pObj, NixSourceCallbackFnc callback, void* callbackData){
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj     = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        obj->queues.callback.func   = callback;
        obj->queues.callback.data   = callbackData;
    }
}

NixBOOL nixAVAudioSource_setVolume(STNixSourceRef pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        [obj->src setVolume:(vol < 0.f ? 0.f : vol > 1.f ? 1.f : vol)];
        obj->volume = vol;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioSource_setRepeat(STNixSourceRef pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixAVAudioSource_setIsRepeat(obj, isRepeat);
        r = NIX_TRUE;
    }
    return r;
}

   
void nixAVAudioSource_play(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        //
        NixAVAudioSource_setIsPlaying(obj, NIX_TRUE);
        NixAVAudioSource_setIsPaused(obj, NIX_FALSE);
        NixAVAudioSource_scheduleEnqueuedBuffers(obj);
        [obj->src play];
    }
}

void nixAVAudioSource_pause(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        [obj->src pause];
        NixAVAudioSource_setIsPaused(obj, NIX_TRUE);
    }
}

void nixAVAudioSource_stop(STNixSourceRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        NixAVAudioSource_setIsPlaying(obj, NIX_FALSE);
        NixAVAudioSource_setIsPaused(obj, NIX_FALSE);
        [obj->src stop];
        //flush all pending buffers
        nixAVAudioSource_removeAllBuffersAndNotify_(obj);
    }
}

NixBOOL nixAVAudioSource_isPlaying(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = (obj->eng != nil && obj->src != nil && [obj->eng isRunning] && [obj->src isPlaying] && obj->queues.pendScheduledCount > 0) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixBOOL nixAVAudioSource_isPaused(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixAVAudioSource_isPlaying(obj) && NixAVAudioSource_isPaused(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixBOOL nixAVAudioSource_isRepeat(STNixSourceRef pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = NixAVAudioSource_isRepeat(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixFLOAT nixAVAudioSource_getVolume(STNixSourceRef pObj){
    NixFLOAT r = 0.f;
    if(pObj.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        r = obj->volume;
    }
    return r;
}


void* nixAVAudioSource_createConverter(STNixContextRef ctx, const STNixAudioDesc* srcFmt, AVAudioFormat* outFmt){
    void* r = NULL;
    STNixAudioDesc outDesc;
    memset(&outDesc, 0, sizeof(outDesc));
    outDesc.samplerate  = [outFmt sampleRate];
    outDesc.channels    = [outFmt channelCount];
    switch([outFmt commonFormat]) {
        case AVAudioPCMFormatFloat32:
            outDesc.bitsPerSample   = 32;
            outDesc.samplesFormat   = ENNixSampleFmt_Float;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        case AVAudioPCMFormatInt16:
            outDesc.bitsPerSample   = 16;
            outDesc.samplesFormat   = ENNixSampleFmt_Int;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        case AVAudioPCMFormatInt32:
            outDesc.bitsPerSample   = 32;
            outDesc.samplesFormat   = ENNixSampleFmt_Int;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        default:
            break;
    }
    if(outDesc.bitsPerSample > 0){
        r = NixFmtConverter_alloc(ctx);
        if(!NixFmtConverter_prepare(r, srcFmt, &outDesc)){
            NixFmtConverter_free(r);
            r = NULL;
        }
    }
    return r;
}

NixBOOL nixAVAudioSource_setBuffer(STNixSourceRef pObj, STNixBufferRef pBuff){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixAVAudioSource* obj    = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        if(obj->queues.conv != NULL || obj->buffsFmt.blockAlign > 0){
            //error, buffer already set
        } else {
            AVAudioOutputNode* outNode  = [obj->eng outputNode];
            AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
            obj->queues.conv = nixAVAudioSource_createConverter(obj->ctx, &buff->desc, outFmt);
            if(obj->queues.conv == NULL){
                NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, nixAVAudioSource_createConverter failed.\n");
            } else {
                //set format
                obj->buffsFmt = buff->desc;
                NixAVAudioSource_setIsStatic(obj, NIX_TRUE);
                if(!NixAVAudioSource_queueBufferForOutput(obj, pBuff)){
                    NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, NixAVAudioSource_queueBufferForOutput failed.\n");
                } else {
                    //enqueue
                    if(NixAVAudioSource_isPlaying(obj) && !NixAVAudioSource_isPaused(obj)){
                        NixAVAudioSource_scheduleEnqueuedBuffers(obj);
                    }
                    r = NIX_TRUE;
                }
            }
        }
    }
    return r;
}

//2025-07-08
//Note: AVAudioMixerNode and all attempts to play 22050Hz 16-bits mono audio produces Operating-System assertions.
//Implementing manual-samples conversion as result.
//
NixBOOL nixAVAudioSource_queueBuffer(STNixSourceRef pObj, STNixBufferRef pBuff) {
    NixBOOL r = NIX_FALSE;
    if(pObj.ptr != NULL && pBuff.ptr != NULL){
        STNixAVAudioSource* obj    = (STNixAVAudioSource*)NixSharedPtr_getOpq(pObj.ptr);
        STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pBuff.ptr);
        //define format
        if(obj->queues.conv == NULL && obj->buffsFmt.blockAlign <= 0){
            //first buffer, define as format
            AVAudioOutputNode* outNode  = [obj->eng outputNode];
            AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
            obj->queues.conv = nixAVAudioSource_createConverter(obj->ctx, &buff->desc, outFmt);
            if(obj->queues.conv == NULL){
                //error, converter creation failed
            } else {
                //set format
                obj->buffsFmt = buff->desc;
            }
        }
        //queue buffer
        if(!STNixAudioDesc_isEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, new buffer doesnt match first buffer's format.\n");
        } else if(!NixAVAudioSource_queueBufferForOutput(obj, pBuff)){
            NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, NixAVAudioSource_queueBufferForOutput failed.\n");
        } else {
            //enqueue
            if(NixAVAudioSource_isPlaying(obj) && !NixAVAudioSource_isPaused(obj)){
                NixAVAudioSource_scheduleEnqueuedBuffers(obj);
            }
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL nixAVAudioSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset){ //relative to first buffer in queue
    NixBOOL r = NIX_FALSE;
    if(ref.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->queues.pend.use > 0){
            STNixAVAudioQueuePair* pair = &obj->queues.pend.arr[0];
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

NixUI32 nixAVAudioSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){   //all buffer queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        {
            NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
                STNixAVAudioQueuePair* pair = &obj->queues.pend.arr[0];
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

NixUI32 nixAVAudioSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){  //relative to first buffer in queue
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    if(ref.ptr != NULL){
        STNixAVAudioSource* obj = (STNixAVAudioSource*)NixSharedPtr_getOpq(ref.ptr);
        NixMutex_lock(obj->queues.mutex);
        if(obj->src != nil && obj->queues.pend.use > 0){
            STNixAVAudioQueuePair* pair = &obj->queues.pend.arr[0];
            STNixPCMBuffer* buff = (STNixPCMBuffer*)NixSharedPtr_getOpq(pair->org.ptr);
            if(buff != NULL && buff->desc.blockAlign > 0 && buff->desc.samplerate > 0){
                AVAudioTime* atime = obj->src.lastRenderTime;
                if(atime != nil){
                    AVAudioTime* playerTime = [obj->src playerTimeForNodeTime:atime];
                    if(playerTime != NULL){
                        blocksCount = playerTime.sampleTime * buff->desc.samplerate / playerTime.sampleRate;
                        bytesCount  = blocksCount * buff->desc.blockAlign;
                        msecsCount  = (blocksCount * 1000 / buff->desc.samplerate);
                        r = blocksCount;
                    }
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
//NixFmtConverter API
//------

void NixFmtConverter_buffFmtToAudioDesc(AVAudioFormat* buffFmt, STNixAudioDesc* dst){
    if(buffFmt != nil && dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->samplerate  = [buffFmt sampleRate];
        dst->channels    = [buffFmt channelCount];
        switch([buffFmt commonFormat]) {
            case AVAudioPCMFormatFloat32:
                dst->bitsPerSample   = 32;
                dst->samplesFormat   = ENNixSampleFmt_Float;
                dst->blockAlign      = (dst->bitsPerSample / 8) * ([buffFmt isInterleaved] ? dst->channels : 1);
                break;
            case AVAudioPCMFormatInt16:
                dst->bitsPerSample   = 16;
                dst->samplesFormat   = ENNixSampleFmt_Int;
                dst->blockAlign      = (dst->bitsPerSample / 8) * ([buffFmt isInterleaved] ? dst->channels : 1);
                break;
            case AVAudioPCMFormatInt32:
                dst->bitsPerSample   = 32;
                dst->samplesFormat   = ENNixSampleFmt_Int;
                dst->blockAlign      = (dst->bitsPerSample / 8) * ([buffFmt isInterleaved] ? dst->channels : 1);
                break;
            default:
                break;
        }
    }
}



//------
//Recorder API
//------

STNixRecorderRef nixAVAudioRecorder_alloc(STNixEngineRef pEng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer){
    STNixRecorderRef r = STNixRecorderRef_Zero;
    STNixAVAudioEngine* eng = (STNixAVAudioEngine*)NixSharedPtr_getOpq(pEng.ptr);
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0 && audioDesc->blockAlign > 0 && eng->rec == NULL){
        STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixContext_malloc(eng->ctx, sizeof(STNixAVAudioRecorder), "STNixAVAudioRecorder");
        if(obj != NULL){
            NixAVAudioRecorder_init(eng->ctx, obj);
            if(!NixAVAudioRecorder_prepare(obj, eng, audioDesc, buffersCount, blocksPerBuffer)){
                NIX_PRINTF_ERROR("nixAVAudioRecorder_create, NixAVAudioRecorder_prepare failed.\n");
            } else if(NULL == (r.ptr = NixSharedPtr_alloc(eng->ctx.itf, obj, "nixAVAudioRecorder_alloc"))){
                NIX_PRINTF_ERROR("nixAVAudioRecorder_create::NixSharedPtr_alloc failed.\n");
            } else {
                r.itf           = &eng->apiItf.recorder;
                obj->engRef     = pEng; NixEngine_retain(pEng);
                obj->selfRef    = r;
                eng->rec        = obj; obj = NULL; //consume
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            NixAVAudioRecorder_destroy(obj);
            NixContext_mfree(eng->ctx, obj);
            obj = NULL;
        }
    }
    return r;
}

void nixAVAudioRecorder_free(STNixRecorderRef pObj){
    if(pObj.ptr != NULL){
        STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
        NixSharedPtr_free(pObj.ptr);
        if(obj != NULL){
            STNixMemoryItf memItf = obj->ctx.itf->mem; //use a copy, in case the Context get destroyed
            {
                NixAVAudioRecorder_destroy(obj);
            }
            if(memItf.free != NULL){
                (*memItf.free)(obj);
            }
            obj = NULL;
        }
    }
}

NixBOOL nixAVAudioRecorder_setCallback(STNixRecorderRef pObj, NixRecorderCallbackFnc callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAVAudioRecorder_setCallback(obj, callback, callbackData);
    }
    return r;
}

NixBOOL nixAVAudioRecorder_start(STNixRecorderRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAVAudioRecorder_start(obj);
    }
    return r;
}

NixBOOL nixAVAudioRecorder_stop(STNixRecorderRef pObj){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(pObj.ptr);
    if(obj != NULL){
        r = NixAVAudioRecorder_stop(obj);
    }
    return r;
}

NixBOOL nixAVAudioRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        if(includeCurrentPartialBuff){
            NixAVAudioRecorder_flush(obj);
        }
        NixAVAudioRecorder_notifyBuffers(obj, discardWithoutNotifying);
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioRecorder_isCapturing(STNixRecorderRef ref){
    NixBOOL r = NIX_FALSE;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    if(obj != NULL){
        r = obj->engStarted;
    }
    return r;
}

NixUI32 nixAVAudioRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount){
    NixUI32 r = 0, bytesCount = 0, blocksCount = 0, msecsCount = 0;
    STNixAVAudioRecorder* obj = (STNixAVAudioRecorder*)NixSharedPtr_getOpq(ref.ptr);
    //calculate filled buffers
    NixMutex_lock(obj->queues.mutex);
    {
        NixUI32 i; for(i = 0; i < obj->queues.notify.use; i++){
            STNixAVAudioQueuePair* pair = &obj->queues.notify.arr[0];
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

