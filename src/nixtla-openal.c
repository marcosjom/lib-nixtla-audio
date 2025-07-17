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
#include "nixtla-audio.h"
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
STNixApiEngine  nixOpenALEngine_create(void);
void            nixOpenALEngine_destroy(STNixApiEngine obj);
void            nixOpenALEngine_printCaps(STNixApiEngine obj);
NixBOOL         nixOpenALEngine_ctxIsActive(STNixApiEngine obj);
NixBOOL         nixOpenALEngine_ctxActivate(STNixApiEngine obj);
NixBOOL         nixOpenALEngine_ctxDeactivate(STNixApiEngine obj);
void            nixOpenALEngine_tick(STNixApiEngine obj);
//PCMBuffer
STNixApiBuffer  nixOpenALPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
void            nixOpenALPCMBuffer_destroy(STNixApiBuffer obj);
NixBOOL         nixOpenALPCMBuffer_setData(STNixApiBuffer obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL         nixOpenALPCMBuffer_fillWithZeroes(STNixApiBuffer obj);
//Source
STNixApiSource  nixOpenALSource_create(STNixApiEngine eng);
void            nixOpenALSource_destroy(STNixApiSource obj);
void            nixOpenALSource_setCallback(STNixApiSource obj, void (*callback)(void* pEng, const NixUI32 sourceIndex, const NixUI32 ammBuffs), void* callbackEng, NixUI32 callbackSourceIndex);
NixBOOL         nixOpenALSource_setVolume(STNixApiSource obj, const float vol);
NixBOOL         nixOpenALSource_setRepeat(STNixApiSource obj, const NixBOOL isRepeat);
void            nixOpenALSource_play(STNixApiSource obj);
void            nixOpenALSource_pause(STNixApiSource obj);
void            nixOpenALSource_stop(STNixApiSource obj);
NixBOOL         nixOpenALSource_isPlaying(STNixApiSource obj);
NixBOOL         nixOpenALSource_isPaused(STNixApiSource obj);
NixBOOL         nixOpenALSource_setBuffer(STNixApiSource obj, STNixApiBuffer buff);  //static-source
NixBOOL         nixOpenALSource_queueBuffer(STNixApiSource obj, STNixApiBuffer buff); //stream-source
//Recorder
STNixApiRecorder nixOpenALRecorder_create(STNixApiEngine eng, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer);
void            nixOpenALRecorder_destroy(STNixApiRecorder obj);
NixBOOL         nixOpenALRecorder_setCallback(STNixApiRecorder obj, NixApiCaptureBufferFilledCallback callback, void* callbackData);
NixBOOL         nixOpenALRecorder_start(STNixApiRecorder obj);
NixBOOL         nixOpenALRecorder_stop(STNixApiRecorder obj);
void            nixOpenALRecorder_notifyBuffers(STNixApiRecorder obj, STNix_Engine* engAbs, PTRNIX_CaptureBufferFilledCallback bufferCaptureCallback, void* bufferCaptureCallbackUserData);

NixBOOL nixOpenALEngine_getApiItf(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->engine.create      = nixOpenALEngine_create;
        dst->engine.destroy     = nixOpenALEngine_destroy;
        dst->engine.printCaps   = nixOpenALEngine_printCaps;
        dst->engine.ctxIsActive = nixOpenALEngine_ctxIsActive;
        dst->engine.ctxActivate = nixOpenALEngine_ctxActivate;
        dst->engine.ctxDeactivate = nixOpenALEngine_ctxDeactivate;
        dst->engine.tick        = nixOpenALEngine_tick;
        //PCMBuffer
        dst->buffer.create      = nixOpenALPCMBuffer_create;
        dst->buffer.destroy     = nixOpenALPCMBuffer_destroy;
        dst->buffer.setData     = nixOpenALPCMBuffer_setData;
        dst->buffer.fillWithZeroes = nixOpenALPCMBuffer_fillWithZeroes;
        //Source
        dst->source.create      = nixOpenALSource_create;
        dst->source.destroy     = nixOpenALSource_destroy;
        dst->source.setCallback = nixOpenALSource_setCallback;
        dst->source.setVolume   = nixOpenALSource_setVolume;
        dst->source.setRepeat   = nixOpenALSource_setRepeat;
        dst->source.play        = nixOpenALSource_play;
        dst->source.pause       = nixOpenALSource_pause;
        dst->source.stop        = nixOpenALSource_stop;
        dst->source.isPlaying   = nixOpenALSource_isPlaying;
        dst->source.isPaused    = nixOpenALSource_isPaused;
        dst->source.setBuffer   = nixOpenALSource_setBuffer;  //static-source
        dst->source.queueBuffer = nixOpenALSource_queueBuffer; //stream-source
        //Recorder
        dst->recorder.create    = nixOpenALRecorder_create;
        dst->recorder.destroy   = nixOpenALRecorder_destroy;
        dst->recorder.setCallback   = nixOpenALRecorder_setCallback;
        dst->recorder.start     = nixOpenALRecorder_start;
        dst->recorder.stop      = nixOpenALRecorder_stop;
        //
        r = NIX_TRUE;
    }
    return r;
}

struct STNix_OpenALEngine_;
struct STNix_OpenALSource_;
struct STNix_OpenALSourceCallback_;
struct STNix_OpenALQueue_;
struct STNix_OpenALQueuePair_;
struct STNix_OpenALSrcNotif_;
struct STNix_OpenALNotifQueue_;
struct STNix_OpenALPCMBuffer_;
struct STNix_OpenALRecorder_;

//------
//Engine
//------

typedef struct STNix_OpenALEngine_ {
    NixUI32         maskCapabilities;
    NixBOOL         contextALIsCurrent;
    ALCcontext*     contextAL;
    ALCdevice*      deviceAL;
    ALCdevice*      idCaptureAL;                //OpenAL specific
    NixUI32         captureMainBufferBytesCount;    //OpenAL specific
    //srcs
    struct {
        NIX_MUTEX_T                 mutex;
        struct STNix_OpenALSource_** arr;
        NixUI32                     use;
        NixUI32                     sz;
    } srcs;
    struct STNix_OpenALRecorder_* rec;
} STNix_OpenALEngine;

void Nix_OpenALEngine_init(STNix_OpenALEngine* obj);
void Nix_OpenALEngine_destroy(STNix_OpenALEngine* obj);
NixBOOL Nix_OpenALEngine_srcsAdd(STNix_OpenALEngine* obj, struct STNix_OpenALSource_* src);
void Nix_OpenALEngine_tick(STNix_OpenALEngine* obj, const NixBOOL isFinalCleanup);

//------
//Notif
//------

typedef struct STNix_OpenALSourceCallback_ {
    void            (*func)(void* pEng, const NixUI32 sourceIndex, NixUI32 ammBuffs);
    void*           eng;
    NixUI32         sourceIndex;
} STNix_OpenALSourceCallback;

typedef struct STNix_OpenALSrcNotif_ {
    STNix_OpenALSourceCallback callback;
    NixUI32 ammBuffs;
} STNix_OpenALSrcNotif;

void Nix_OpenALSrcNotif_init(STNix_OpenALSrcNotif* obj);
void Nix_OpenALSrcNotif_destroy(STNix_OpenALSrcNotif* obj);

//------
//NotifQueue
//------

typedef struct STNix_OpenALNotifQueue_ {
    STNix_OpenALSrcNotif*  arr;
    NixUI32                use;
    NixUI32                sz;
    STNix_OpenALSrcNotif  arrEmbedded[32];
} STNix_OpenALNotifQueue;

void Nix_OpenALNotifQueue_init(STNix_OpenALNotifQueue* obj);
void Nix_OpenALNotifQueue_destroy(STNix_OpenALNotifQueue* obj);
//
NixBOOL Nix_OpenALNotifQueue_push(STNix_OpenALNotifQueue* obj, STNix_OpenALSrcNotif* pair);

//------
//PCMBuffer
//------

typedef struct STNix_OpenALPCMBuffer_ {
    NixUI8*         ptr;
    NixUI32         use;
    NixUI32         sz;
    STNix_audioDesc desc;
} STNix_OpenALPCMBuffer;

void Nix_OpenALPCMBuffer_init(STNix_OpenALPCMBuffer* obj);
void Nix_OpenALPCMBuffer_destroy(STNix_OpenALPCMBuffer* obj);
NixBOOL Nix_OpenALPCMBuffer_setData(STNix_OpenALPCMBuffer* obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL Nix_OpenALPCMBuffer_fillWithZeroes(STNix_OpenALPCMBuffer* obj);

//------
//QueuePair (Buffers)
//------

typedef struct STNix_OpenALQueuePair_ {
    STNix_OpenALPCMBuffer*  org;    //original buffer (owned by the user)
    ALuint                  idBufferAL; //converted buffer (owned by the source)
} STNix_OpenALQueuePair;

void Nix_OpenALQueuePair_init(STNix_OpenALQueuePair* obj);
void Nix_OpenALQueuePair_destroy(STNix_OpenALQueuePair* obj);

//------
//Queue (Buffers)
//------

typedef struct STNix_OpenALQueue_ {
    STNix_OpenALQueuePair*  arr;
    NixUI32                 use;
    NixUI32                 sz;
} STNix_OpenALQueue;

void Nix_OpenALQueue_init(STNix_OpenALQueue* obj);
void Nix_OpenALQueue_destroy(STNix_OpenALQueue* obj);
//
NixBOOL Nix_OpenALQueue_flush(STNix_OpenALQueue* obj, const NixBOOL nullifyOrgs);
NixBOOL Nix_OpenALQueue_prepareForSz(STNix_OpenALQueue* obj, const NixUI32 minSz);
NixBOOL Nix_OpenALQueue_pushOwning(STNix_OpenALQueue* obj, STNix_OpenALQueuePair* pair);
NixBOOL Nix_OpenALQueue_popOrphaning(STNix_OpenALQueue* obj, STNix_OpenALQueuePair* dst);
NixBOOL Nix_OpenALQueue_popMovingTo(STNix_OpenALQueue* obj, STNix_OpenALQueue* other);

//------
//Source
//------

#define NIX_OpenALSource_BIT_isStatic   (0x1 << 0)  //source expects only one buffer, repeats or pauses after playing it
#define NIX_OpenALSource_BIT_isChanging (0x1 << 1)  //source is changing state after a call to request*()
#define NIX_OpenALSource_BIT_isRepeat   (0x1 << 2)
#define NIX_OpenALSource_BIT_isPlaying  (0x1 << 3)
#define NIX_OpenALSource_BIT_isPaused   (0x1 << 4)
#define NIX_OpenALSource_BIT_isClosing  (0x1 << 5)
#define NIX_OpenALSource_BIT_isOrphan   (0x1 << 6)  //source is waiting for close(), wait for the change of state and Nix_OpenALSource_release + NIX_FREE.

typedef struct STNix_OpenALSource_ {
    struct STNix_OpenALEngine_* eng;    //parent engine
    STNix_audioDesc         buffsFmt;   //first attached buffers' format (defines the converter config)
    STNix_audioDesc         srcFmt;
    ALenum                  srcFmtAL;
    ALuint                  idSourceAL;
    //queues
    struct {
        NIX_MUTEX_T         mutex;
        //conv
        struct {
            void*           obj;   //nixFmtConverter
            //buff
            struct {
                void*       ptr;
                NixUI32     sz;
            } buff;
        } conv;
        STNix_OpenALSourceCallback callback;
        STNix_OpenALQueue   notify; //buffers (consumed, pending to notify)
        STNix_OpenALQueue   reuse;  //buffers (conversion buffers)
        STNix_OpenALQueue   pend;   //to be played/filled
        NixUI32             pendSampleIdx;  //current sample playing/filling
    } queues;
    //props
    float                   volume;
    NixUI8                  stateBits;  //packed bools to reduce padding, NIX_OpenALSource_BIT_
} STNix_OpenALSource;

void Nix_OpenALSource_init(STNix_OpenALSource* obj);
void Nix_OpenALSource_release(STNix_OpenALSource* obj);
NixBOOL Nix_OpenALSource_queueBufferForOutput(STNix_OpenALSource* obj, STNix_OpenALPCMBuffer* buff, const NixBOOL isStream);
NixBOOL Nix_OpenALSource_pendPopOldestBuffLocked_(STNix_OpenALSource* obj);
NixBOOL Nix_OpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNix_OpenALSource* obj);

//
#define Nix_OpenALSource_isStatic(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isStatic) != 0)
#define Nix_OpenALSource_isChanging(OBJ)        (((OBJ)->stateBits & NIX_OpenALSource_BIT_isChanging) != 0)
#define Nix_OpenALSource_isRepeat(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isRepeat) != 0)
#define Nix_OpenALSource_isPlaying(OBJ)         (((OBJ)->stateBits & NIX_OpenALSource_BIT_isPlaying) != 0)
#define Nix_OpenALSource_isPaused(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isPaused) != 0)
#define Nix_OpenALSource_isClosing(OBJ)         (((OBJ)->stateBits & NIX_OpenALSource_BIT_isClosing) != 0)
#define Nix_OpenALSource_isOrphan(OBJ)          (((OBJ)->stateBits & NIX_OpenALSource_BIT_isOrphan) != 0)
//
#define Nix_OpenALSource_setIsStatic(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isStatic : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isStatic)
#define Nix_OpenALSource_setIsChanging(OBJ, V)  (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isChanging : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isChanging)
#define Nix_OpenALSource_setIsRepeat(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isRepeat : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isRepeat)
#define Nix_OpenALSource_setIsPlaying(OBJ, V)   (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isPlaying : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isPlaying)
#define Nix_OpenALSource_setIsPaused(OBJ, V)    (OBJ)->stateBits = (V ? (OBJ)->stateBits | NIX_OpenALSource_BIT_isPaused : (OBJ)->stateBits & ~NIX_OpenALSource_BIT_isPaused)
#define Nix_OpenALSource_setIsClosing(OBJ)      (OBJ)->stateBits = ((OBJ)->stateBits | NIX_OpenALSource_BIT_isClosing)
#define Nix_OpenALSource_setIsOrphan(OBJ)       (OBJ)->stateBits = ((OBJ)->stateBits | NIX_OpenALSource_BIT_isOrphan)

//------
//Recorder
//------

typedef struct STNix_OpenALRecorder_ {
    NixBOOL                 engStarted;
    STNix_OpenALEngine*     engNx;
    ALuint                  idCaptureAL;
    STNix_audioDesc         capFmt;
    //callback
    struct {
        NixApiCaptureBufferFilledCallback func;
        void*               data;
    } callback;
    //cfg
    struct {
        STNix_audioDesc     fmt;
        NixUI16             samplesPerBuffer;
        NixUI16             maxBuffers;
    } cfg;
    //queues
    struct {
        NIX_MUTEX_T         mutex;
        void*               conv;   //nixFmtConverter
        STNix_OpenALQueue   notify;
        STNix_OpenALQueue   reuse;
        //filling
        struct {
            NixUI8*         tmp;
            NixUI32         tmpSz;
            NixSI32         iCurSample; //at first buffer in 'reuse'
        } filling;
    } queues;
} STNix_OpenALRecorder;

void Nix_OpenALRecorder_init(STNix_OpenALRecorder* obj);
void Nix_OpenALRecorder_destroy(STNix_OpenALRecorder* obj);
//
NixBOOL Nix_OpenALRecorder_prepare(STNix_OpenALRecorder* obj, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer);
NixBOOL Nix_OpenALRecorder_setCallback(STNix_OpenALRecorder* obj, NixApiCaptureBufferFilledCallback callback, void* callbackData);
NixBOOL Nix_OpenALRecorder_start(STNix_OpenALRecorder* obj);
NixBOOL Nix_OpenALRecorder_stop(STNix_OpenALRecorder* obj);
NixBOOL Nix_OpenALRecorder_flush(STNix_OpenALRecorder* obj);
void Nix_OpenALRecorder_consumeInputBuffer(STNix_OpenALRecorder* obj);
void Nix_OpenALRecorder_notifyBuffers(STNix_OpenALRecorder* obj);

//------
//Engine
//------

void Nix_OpenALEngine_init(STNix_OpenALEngine* obj){
    memset(obj, 0, sizeof(STNix_OpenALEngine));
    obj->deviceAL = NIX_OPENAL_NULL;
    obj->contextAL = NIX_OPENAL_NULL;
    //srcs
    {
        NIX_MUTEX_INIT(&obj->srcs.mutex);
    }
}

void Nix_OpenALEngine_destroy(STNix_OpenALEngine* obj){
    if(obj != NULL){
        //srcs
        {
            //cleanup
            while(obj->srcs.arr != NULL && obj->srcs.use > 0){
                Nix_OpenALEngine_tick(obj, NIX_TRUE);
            }
            //
            if(obj->srcs.arr != NULL){
                NIX_FREE(obj->srcs.arr);
                obj->srcs.arr = NULL;
            }
            NIX_MUTEX_DESTROY(&obj->srcs.mutex);
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
        obj = NULL;
    }
}

NixBOOL Nix_OpenALEngine_srcsAdd(STNix_OpenALEngine* obj, struct STNix_OpenALSource_* src){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        NIX_MUTEX_LOCK(&obj->srcs.mutex);
        {
            //resize array (if necesary)
            if(obj->srcs.use >= obj->srcs.sz){
                const NixUI32 szN = obj->srcs.use + 4;
                STNix_OpenALSource** arrN = NULL;
                NIX_MALLOC(arrN, STNix_OpenALSource*, sizeof(STNix_OpenALSource*) * szN, "STNix_OpenALEngine::srcsN");
                if(arrN != NULL){
                    if(obj->srcs.arr != NULL){
                        if(obj->srcs.use > 0){
                            memcpy(arrN, obj->srcs.arr, sizeof(arrN[0]) * obj->srcs.use);
                        }
                        NIX_FREE(obj->srcs.arr);
                        obj->srcs.arr = NULL;
                    }
                    obj->srcs.arr = arrN;
                    obj->srcs.sz = szN;
                }
            }
            //add
            if(obj->srcs.use >= obj->srcs.sz){
                NIX_PRINTF_ERROR("nixOpenALSource_create::STNix_OpenALEngine::srcs failed (no allocated space).\n");
            } else {
                //become the owner of the pair
                obj->srcs.arr[obj->srcs.use++] = src;
                r = NIX_TRUE;
            }
        }
        NIX_MUTEX_UNLOCK(&obj->srcs.mutex);
    }
    return r;
}

void Nix_OpenALEngine_removeSrcRecordLocked_(STNix_OpenALEngine* obj, NixSI32* idx){
    STNix_OpenALSource* src = obj->srcs.arr[*idx];
    if(src != NULL){
        Nix_OpenALSource_release(src);
        NIX_FREE(src);
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

void Nix_OpenALEngine_tick_addQueueNotifSrcLocked_(STNix_OpenALNotifQueue* notifs, STNix_OpenALSource* srcLocked){
    if(srcLocked->queues.notify.use > 0){
        const NixBOOL nullifyOrgs = NIX_TRUE;
        STNix_OpenALSrcNotif n;
        Nix_OpenALSrcNotif_init(&n);
        n.callback = srcLocked->queues.callback;
        n.ammBuffs = srcLocked->queues.notify.use;
        if(!Nix_OpenALQueue_flush(&srcLocked->queues.notify, nullifyOrgs)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        }
        if(!Nix_OpenALNotifQueue_push(notifs, &n)){
            NIX_ASSERT(NIX_FALSE); //program logic error
            Nix_OpenALSrcNotif_destroy(&n);
        }
    }
}

void Nix_OpenALEngine_tick(STNix_OpenALEngine* obj, const NixBOOL isFinalCleanup){
    if(obj != NULL){
        //srcs
        {
            STNix_OpenALNotifQueue notifs;
            Nix_OpenALNotifQueue_init(&notifs);
            NIX_MUTEX_LOCK(&obj->srcs.mutex);
            if(obj->srcs.arr != NULL && obj->srcs.use > 0){
                //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::%d sources.\n", obj->srcs.use);
                NixSI32 i; for(i = 0; i < (NixSI32)obj->srcs.use; ++i){
                    STNix_OpenALSource* src = obj->srcs.arr[i];
                    //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::source(#%d/%d).\n", i + 1, obj->srcs.use);
                    if(src->idSourceAL == NIX_OPENAL_NULL){
                        //remove
                        //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::source(#%d/%d); remove-NULL.\n", i + 1, obj->srcs.use);
                        Nix_OpenALEngine_removeSrcRecordLocked_(obj, &i);
                        src = NULL;
                    } else {
                        //remove processed buffers
                        if(src != NULL && !Nix_OpenALSource_isStatic(src)){
                            ALint csmdAmm = 0;
                            alGetSourceiv(src->idSourceAL, AL_BUFFERS_PROCESSED, &csmdAmm); NIX_OPENAL_ERR_VERIFY("alGetSourceiv(AL_BUFFERS_PROCESSED)");
                            if(csmdAmm > 0){
                                NIX_MUTEX_LOCK(&src->queues.mutex);
                                {
                                    NIX_ASSERT(csmdAmm <= src->queues.pend.use) //Just checking, this should be always be true
                                    while(csmdAmm > 0){
                                        if(!Nix_OpenALSource_pendPopOldestBuffLocked_(src)){
                                            NIX_ASSERT(NIX_FALSE) //program logic error
                                            break;
                                        }
                                        --csmdAmm;
                                    }
                                }
                                NIX_MUTEX_UNLOCK(&src->queues.mutex);
                            }
                        }
                        //post-process
                        if(src != NULL){
                            //add to notify queue
                            {
                                NIX_MUTEX_LOCK(&src->queues.mutex);
                                {
                                    Nix_OpenALEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
                                }
                                NIX_MUTEX_UNLOCK(&src->queues.mutex);
                            }
                        }
                    }
                }
            }
            NIX_MUTEX_UNLOCK(&obj->srcs.mutex);
            //notify (unloked)
            if(notifs.use > 0){
                //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::notify %d.\n", notifs.use);
                NixUI32 i;
                for(i = 0; i < notifs.use; ++i){
                    STNix_OpenALSrcNotif* n = &notifs.arr[i];
                    //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::notify(#%d/%d).\n", i + 1, notifs.use);
                    if(n->callback.func != NULL){
                        (*n->callback.func)(n->callback.eng, n->callback.sourceIndex, n->ammBuffs);
                    }
                }
            }
            //NIX_PRINTF_INFO("Nix_OpenALEngine_tick::Nix_OpenALNotifQueue_destroy.\n");
            Nix_OpenALNotifQueue_destroy(&notifs);
        }
        //recorder
        if(obj->rec != NULL){
            //Note: when the capture is stopped, ALC_CAPTURE_SAMPLES returns the maximun size instead of the captured-only size.
            if(obj->rec->engStarted){
                Nix_OpenALRecorder_consumeInputBuffer(obj->rec);
            }
            Nix_OpenALRecorder_notifyBuffers(obj->rec);
        }
    }
}

//------
//Notif
//------

void Nix_OpenALSrcNotif_init(STNix_OpenALSrcNotif* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_OpenALSrcNotif_destroy(STNix_OpenALSrcNotif* obj){
    //
}

//------
//NotifQueue
//------

void Nix_OpenALNotifQueue_init(STNix_OpenALNotifQueue* obj){
    memset(obj, 0, sizeof(*obj));
    obj->arr = obj->arrEmbedded;
    obj->sz = (sizeof(obj->arrEmbedded) / sizeof(obj->arrEmbedded[0]));
}

void Nix_OpenALNotifQueue_destroy(STNix_OpenALNotifQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNix_OpenALSrcNotif* b = &obj->arr[i];
            Nix_OpenALSrcNotif_destroy(b);
        }
        if(obj->arr != obj->arrEmbedded){
            NIX_FREE(obj->arr);
        }
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
}

NixBOOL Nix_OpenALNotifQueue_push(STNix_OpenALNotifQueue* obj, STNix_OpenALSrcNotif* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNix_OpenALSrcNotif* arrN = NULL;
            NIX_MALLOC(arrN, STNix_OpenALSrcNotif, sizeof(STNix_OpenALSrcNotif) * szN, "Nix_OpenALNotifQueue_push::arrN");
            if(arrN != NULL){
                if(obj->arr != NULL){
                    if(obj->use > 0){
                        memcpy(arrN, obj->arr, sizeof(arrN[0]) * obj->use);
                    }
                    if(obj->arr != obj->arrEmbedded){
                        NIX_FREE(obj->arr);
                    }
                    obj->arr = NULL;
                }
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //add
        if(obj->use >= obj->sz){
            NIX_PRINTF_ERROR("Nix_OpenALNotifQueue_push failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//PCMBuffer
//------

void Nix_OpenALPCMBuffer_init(STNix_OpenALPCMBuffer* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_OpenALPCMBuffer_destroy(STNix_OpenALPCMBuffer* obj){
    if(obj->ptr != NULL){
        NIX_FREE(obj->ptr);
        obj->ptr = NULL;
    }
    obj->use = obj->sz = 0;
    memset(obj, 0, sizeof(STNix_OpenALPCMBuffer));
}

NixBOOL Nix_OpenALPCMBuffer_setData(STNix_OpenALPCMBuffer* obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        const NixUI32 reqBytes = (audioDataPCMBytes / audioDesc->blockAlign * audioDesc->blockAlign);
        //destroy current buffer (if necesary)
        if(!STNix_audioDesc_IsEqual(&obj->desc, audioDesc) || obj->sz < reqBytes){
            if(obj->ptr != NULL){
                NIX_FREE(obj->ptr);
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
                NIX_MALLOC(obj->ptr, NixUI8, reqBytes, "nixOpenALPCMBuffer_create::ptr");
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

NixBOOL Nix_OpenALPCMBuffer_fillWithZeroes(STNix_OpenALPCMBuffer* obj){
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
//QueuePair (Buffers)
//------

void Nix_OpenALQueuePair_init(STNix_OpenALQueuePair* obj){
    memset(obj, 0, sizeof(*obj));
    obj->idBufferAL = NIX_OPENAL_NULL;
}

void Nix_OpenALQueuePair_destroy(STNix_OpenALQueuePair* obj){
    NIX_ASSERT(obj->org == NULL) //program-logic error; should be always NULLyfied before the pair si destroyed
    if(obj->org != NULL){
        //Note: org is owned by the user, do not destroy
        obj->org = NULL;
    }
    if(obj->idBufferAL != NIX_OPENAL_NULL){
        alDeleteBuffers(1, &obj->idBufferAL); NIX_OPENAL_ERR_VERIFY("alDeleteBuffers");
        obj->idBufferAL = NIX_OPENAL_NULL;
    }
}

//------
//Queue (Buffers)
//------

void Nix_OpenALQueue_init(STNix_OpenALQueue* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_OpenALQueue_destroy(STNix_OpenALQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNix_OpenALQueuePair* b = &obj->arr[i];
            Nix_OpenALQueuePair_destroy(b);
        }
        NIX_FREE(obj->arr);
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
}

NixBOOL Nix_OpenALQueue_flush(STNix_OpenALQueue* obj, const NixBOOL nullifyOrgs){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        if(obj->arr != NULL){
            NixUI32 i; for(i = 0; i < obj->use; i++){
                STNix_OpenALQueuePair* b = &obj->arr[i];
                if(nullifyOrgs){
                    b->org = NULL;
                }
                Nix_OpenALQueuePair_destroy(b);
            }
            obj->use = 0;
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_OpenALQueue_prepareForSz(STNix_OpenALQueue* obj, const NixUI32 minSz){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        //resize array (if necesary)
        if(minSz > obj->sz){
            const NixUI32 szN = minSz;
            STNix_OpenALQueuePair* arrN = NULL;
            NIX_MALLOC(arrN, STNix_OpenALQueuePair, sizeof(STNix_OpenALQueuePair) * szN, "Nix_OpenALQueue_prepareForSz::arrN");
            if(arrN != NULL){
                if(obj->arr != NULL){
                    if(obj->use > 0){
                        memcpy(arrN, obj->arr, sizeof(arrN[0]) * obj->use);
                    }
                    NIX_FREE(obj->arr);
                    obj->arr = NULL;
                }
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //analyze
        if(minSz > obj->sz){
            NIX_PRINTF_ERROR("Nix_OpenALQueue_prepareForSz failed (no allocated space).\n");
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL Nix_OpenALQueue_pushOwning(STNix_OpenALQueue* obj, STNix_OpenALQueuePair* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNix_OpenALQueuePair* arrN = NULL;
            NIX_MALLOC(arrN, STNix_OpenALQueuePair, sizeof(STNix_OpenALQueuePair) * szN, "Nix_OpenALQueue_pushOwning::arrN");
            if(arrN != NULL){
                if(obj->arr != NULL){
                    if(obj->use > 0){
                        memcpy(arrN, obj->arr, sizeof(arrN[0]) * obj->use);
                    }
                    NIX_FREE(obj->arr);
                    obj->arr = NULL;
                }
                obj->arr = arrN;
                obj->sz = szN;
            }
        }
        //add
        if(obj->use >= obj->sz){
            NIX_PRINTF_ERROR("Nix_OpenALQueue_pushOwning failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL Nix_OpenALQueue_popOrphaning(STNix_OpenALQueue* obj, STNix_OpenALQueuePair* dst){
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

NixBOOL Nix_OpenALQueue_popMovingTo(STNix_OpenALQueue* obj, STNix_OpenALQueue* other){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALQueuePair pair;
    if(!Nix_OpenALQueue_popOrphaning(obj, &pair)){
        //
    } else {
        if(!Nix_OpenALQueue_pushOwning(other, &pair)){
            //program logic error
            NIX_ASSERT(NIX_FALSE);
            if(pair.org != NULL){
                Nix_OpenALPCMBuffer_destroy(pair.org);
                NIX_FREE(pair.org);
                pair.org = NULL;
            }
            Nix_OpenALQueuePair_destroy(&pair);
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//Source
//------

void Nix_OpenALSource_init(STNix_OpenALSource* obj){
    memset(obj, 0, sizeof(STNix_OpenALSource));
    obj->idSourceAL = NIX_OPENAL_NULL;
    //queues
    {
        NIX_MUTEX_INIT(&obj->queues.mutex);
        Nix_OpenALQueue_init(&obj->queues.notify);
        Nix_OpenALQueue_init(&obj->queues.pend);
        Nix_OpenALQueue_init(&obj->queues.reuse);
    }
}

void Nix_OpenALSource_release(STNix_OpenALSource* obj){
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
                NIX_FREE(obj->queues.conv.buff.ptr);
                obj->queues.conv.buff.ptr = NULL;
            }
            obj->queues.conv.buff.sz = 0;
        }
        if(obj->queues.conv.obj != NULL){
            nixFmtConverter_destroy(obj->queues.conv.obj);
            obj->queues.conv.obj = NULL;
        }
        Nix_OpenALQueue_destroy(&obj->queues.pend);
        Nix_OpenALQueue_destroy(&obj->queues.reuse);
        Nix_OpenALQueue_destroy(&obj->queues.notify);
        NIX_MUTEX_DESTROY(&obj->queues.mutex);
    }
}

NixBOOL Nix_OpenALSource_queueBufferForOutput(STNix_OpenALSource* obj, STNix_OpenALPCMBuffer* buff, const NixBOOL isStream){
    NixBOOL r = NIX_FALSE;
    if(!STNix_audioDesc_IsEqual(&obj->buffsFmt, &buff->desc)){
        //error
    } else {
        STNix_audioDesc dataFmt;
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
                void* cnvBuffN = NULL;
                NIX_MALLOC(cnvBuffN, void, buffConvSz, "cnvBuffSzN");
                if(cnvBuffN != NULL){
                    if(obj->queues.conv.buff.ptr != NULL){
                        NIX_FREE(obj->queues.conv.buff.ptr);
                        obj->queues.conv.buff.ptr = NULL;
                    }
                    obj->queues.conv.buff.ptr = cnvBuffN;
                    obj->queues.conv.buff.sz = buffConvSz;
                }
            }
            //convert
            if(buffConvSz > obj->queues.conv.buff.sz){
                NIX_PRINTF_ERROR("Nix_OpenALSource_queueBufferForOutput, could not allocate conversion buffer.\n");
            } else if(!nixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv.obj, &buff->desc, buff->ptr, 0)){
                NIX_PRINTF_ERROR("nixFmtConverter_setPtrAtSrcInterlaced, failed.\n");
            } else if(!nixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv.obj, &obj->srcFmt, obj->queues.conv.buff.ptr, 0)){
                NIX_PRINTF_ERROR("nixFmtConverter_setPtrAtDstInterlaced, failed.\n");
            } else {
                const NixUI32 srcBlocks = (buff->use / buff->desc.blockAlign);
                const NixUI32 dstBlocks = (obj->queues.conv.buff.sz / obj->srcFmt.blockAlign);
                NixUI32 ammBlocksRead = 0;
                NixUI32 ammBlocksWritten = 0;
                if(!nixFmtConverter_convert(obj->queues.conv.obj, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                    NIX_PRINTF_ERROR("Nix_OpenALSource_queueBufferForOutput::nixFmtConverter_convert failed from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s).\n"
                                     , obj->buffsFmt.samplerate
                                     , obj->buffsFmt.channels
                                     , obj->buffsFmt.bitsPerSample
                                     , obj->buffsFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : obj->buffsFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
                                     , obj->srcFmt.samplerate
                                     , obj->srcFmt.channels
                                     , obj->srcFmt.bitsPerSample
                                     , obj->srcFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : obj->srcFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
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
            STNix_OpenALQueuePair pair;
            Nix_OpenALQueuePair_init(&pair);
            //reuse or create bufferAL
            {
                STNix_OpenALQueuePair reuse;
                if(!Nix_OpenALQueue_popOrphaning(&obj->queues.reuse, &reuse)){
                    //no reusable buffer available, create new
                    ALenum errorAL;
                    alGenBuffers(1, &pair.idBufferAL);
                    if(AL_NONE != (errorAL = alGetError())){
                        NIX_PRINTF_ERROR("alGenBuffers failed: #%d '%s' idBufferAL(%d)\n", errorAL, alGetString(errorAL), pair.idBufferAL);
                        pair.idBufferAL = NIX_OPENAL_NULL;
                    }
                } else {
                    //reuse buffer
                    NIX_ASSERT(reuse.org == NULL) //program logic error
                    NIX_ASSERT(reuse.idBufferAL != NIX_OPENAL_NULL) //program logic error
                    if(reuse.idBufferAL == NIX_OPENAL_NULL){
                        NIX_PRINTF_ERROR("Nix_OpenALSource_queueBufferForOutput::reuse.cnv should not be NULL.\n");
                    } else {
                        pair.idBufferAL = reuse.idBufferAL; reuse.idBufferAL = NIX_OPENAL_NULL; //consume
                    }
                    Nix_OpenALQueuePair_destroy(&reuse);
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
                        } else if(Nix_OpenALSource_isPlaying(obj) && !Nix_OpenALSource_isPaused(obj)){
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
                    pair.org = buff;
                    NIX_MUTEX_LOCK(&obj->queues.mutex);
                    {
                        if(!Nix_OpenALQueue_pushOwning(&obj->queues.pend, &pair)){
                            NIX_PRINTF_ERROR("Nix_OpenALSource_queueBufferForOutput::Nix_OpenALQueue_pushOwning failed.\n");
                            pair.org = NULL;
                            r = NIX_FALSE;
                        } else {
                            //added to queue
                            Nix_OpenALQueue_prepareForSz(&obj->queues.reuse, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                            Nix_OpenALQueue_prepareForSz(&obj->queues.notify, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                            //this is the first buffer i the queue
                            if(obj->queues.pend.use == 1){
                                obj->queues.pendSampleIdx = 0;
                            }
                            r = NIX_TRUE;
                        }
                    }
                    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
                }
            }
            if(!r){
                Nix_OpenALQueuePair_destroy(&pair);
            }
        }
    }
    return r;
}

NixBOOL Nix_OpenALSource_pendPopOldestBuffLocked_(STNix_OpenALSource* obj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALQueuePair pair;
    if(!Nix_OpenALQueue_popOrphaning(&obj->queues.pend, &pair)){
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
                STNix_OpenALQueuePair reuse;
                Nix_OpenALQueuePair_init(&reuse);
                reuse.idBufferAL = pair.idBufferAL;
                if(!Nix_OpenALQueue_pushOwning(&obj->queues.reuse, &reuse)){
                    NIX_PRINTF_ERROR("Nix_OpenALSource_pendPopOldestBuffLocked_::Nix_OpenALQueue_pushOwning(reuse) failed.\n");
                    reuse.idBufferAL = NIX_OPENAL_NULL;
                    Nix_OpenALQueuePair_destroy(&reuse);
                } else {
                    //now owned by reuse
                    pair.idBufferAL = NIX_OPENAL_NULL;
                }
            }
        }
        //move "org" to notify queue
        if(pair.org != NULL){
            STNix_OpenALQueuePair notif;
            Nix_OpenALQueuePair_init(&notif);
            notif.org = pair.org;
            if(!Nix_OpenALQueue_pushOwning(&obj->queues.notify, &notif)){
                NIX_PRINTF_ERROR("Nix_OpenALSource_pendPopOldestBuffLocked_::Nix_OpenALQueue_pushOwning(notify) failed.\n");
                notif.org = NULL;
                Nix_OpenALQueuePair_destroy(&notif);
            } else {
                //now owned by reuse
                pair.org = NULL;
            }
        }
        NIX_ASSERT(pair.org == NULL); //program logic error
        NIX_ASSERT(pair.idBufferAL == NIX_OPENAL_NULL); //program logic error
        Nix_OpenALQueuePair_destroy(&pair);
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_OpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(STNix_OpenALSource* obj){
    NixBOOL r = NIX_TRUE;
    NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
        STNix_OpenALQueuePair* pair = &obj->queues.pend.arr[i];
        //move "org" to notify queue
        if(pair->org != NULL){
            STNix_OpenALQueuePair notif;
            Nix_OpenALQueuePair_init(&notif);
            notif.org = pair->org;
            if(!Nix_OpenALQueue_pushOwning(&obj->queues.notify, &notif)){
                NIX_PRINTF_ERROR("Nix_OpenALSource_pendPopOldestBuffLocked_::Nix_OpenALQueue_pushOwning(notify) failed.\n");
                notif.org = NULL;
                Nix_OpenALQueuePair_destroy(&notif);
            } else {
                //now owned by reuse
                pair->org = NULL;
            }
        }
        NIX_ASSERT(pair->org == NULL); //program logic error
    }
    return r;
}

//------
//Recorder
//------

void Nix_OpenALRecorder_init(STNix_OpenALRecorder* obj){
    memset(obj, 0, sizeof(*obj));
    obj->idCaptureAL = NIX_OPENAL_NULL;
    //cfg
    {
        //
    }
    //queues
    {
        NIX_MUTEX_INIT(&obj->queues.mutex);
        Nix_OpenALQueue_init(&obj->queues.notify);
        Nix_OpenALQueue_init(&obj->queues.reuse);
    }
}

void Nix_OpenALRecorder_destroy(STNix_OpenALRecorder* obj){
    //queues
    {
        NIX_MUTEX_LOCK(&obj->queues.mutex);
        {
            //ToDo: remove all 'org' buffers manually
            Nix_OpenALQueue_destroy(&obj->queues.notify);
            Nix_OpenALQueue_destroy(&obj->queues.reuse);
            //tmp
            {
                if(obj->queues.filling.tmp != NULL){
                    NIX_FREE(obj->queues.filling.tmp);
                    obj->queues.filling.tmp = NULL;
                }
                obj->queues.filling.tmpSz = 0;
            }
        }
        NIX_MUTEX_UNLOCK(&obj->queues.mutex);
        NIX_MUTEX_DESTROY(&obj->queues.mutex);
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
}

NixBOOL Nix_OpenALRecorder_prepare(STNix_OpenALRecorder* obj, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer){
    NixBOOL r = NIX_FALSE;
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    if(obj->queues.conv == NULL && audioDesc->blockAlign > 0){
        STNix_audioDesc inDesc = STNix_audioDesc_Zero;
        ALenum apiFmt = AL_UNDETERMINED;
        switch(audioDesc->channels){
            case 1:
                switch(audioDesc->bitsPerSample){
                    case 8:
                        apiFmt = AL_FORMAT_MONO8;
                        inDesc.samplesFormat   = ENNix_sampleFormat_int;
                        inDesc.bitsPerSample   = 8;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                    default:
                        apiFmt = AL_FORMAT_MONO16;
                        inDesc.samplesFormat   = ENNix_sampleFormat_int;
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
                        inDesc.samplesFormat   = ENNix_sampleFormat_int;
                        inDesc.bitsPerSample   = 8;
                        inDesc.channels        = audioDesc->channels;
                        inDesc.samplerate      = audioDesc->samplerate;
                        break;
                    default:
                        apiFmt = AL_FORMAT_STEREO16;
                        inDesc.samplesFormat   = ENNix_sampleFormat_int;
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
            const NixUI32 capPerBuffSz = inDesc.blockAlign * samplesPerBuffer;
            const NixUI32 capMainBuffSz = capPerBuffSz * 2;
            ALenum errAL;
            ALuint capDev = alcCaptureOpenDevice(NULL/*devName*/, inDesc.samplerate, apiFmt, capMainBuffSz);
            if (AL_NO_ERROR != (errAL = alGetError())) {
                NIX_PRINTF_ERROR("alcCaptureOpenDevice failed\n");
            } else {
                //tmp
                {
                    if(obj->queues.filling.tmp != NULL){
                        NIX_FREE(obj->queues.filling.tmp);
                        obj->queues.filling.tmp = NULL;
                    }
                    obj->queues.filling.tmpSz = 0;
                }
                NixUI8* tmpBuff = NULL; NIX_MALLOC(tmpBuff, NixUI8, capMainBuffSz, "tmpBuff");
                if(tmpBuff == NULL){
                    NIX_PRINTF_ERROR("Nix_OpenALRecorder_prepare::allocation of temporary buffer failed.\n");
                } else {
                    void* conv = nixFmtConverter_create();
                    if(!nixFmtConverter_prepare(conv, &inDesc, audioDesc)){
                        NIX_PRINTF_ERROR("Nix_OpenALRecorder_prepare::nixFmtConverter_prepare failed.\n");
                        nixFmtConverter_destroy(conv);
                        conv = NULL;
                    } else {
                        //allocate reusable buffers
                        while(obj->queues.reuse.use < buffersCount){
                            STNix_OpenALQueuePair pair;
                            Nix_OpenALQueuePair_init(&pair);
                            NIX_MALLOC(pair.org, STNix_OpenALPCMBuffer, sizeof(STNix_OpenALPCMBuffer), "Nix_OpenALRecorder_prepare.pair.org");
                            if(pair.org == NULL){
                                NIX_PRINTF_ERROR("Nix_OpenALRecorder_prepare::pair.org allocation failed.\n");
                                break;
                            } else {
                                Nix_OpenALPCMBuffer_init(pair.org);
                                if(!Nix_OpenALPCMBuffer_setData(pair.org, audioDesc, NULL, audioDesc->blockAlign * samplesPerBuffer)){
                                    NIX_PRINTF_ERROR("Nix_OpenALRecorder_prepare::Nix_OpenALPCMBuffer_setData failed.\n");
                                    Nix_OpenALPCMBuffer_destroy(pair.org);
                                    NIX_FREE(pair.org);
                                    pair.org = NULL;
                                    break;
                                } else {
                                    Nix_OpenALQueue_pushOwning(&obj->queues.reuse, &pair);
                                }
                            }
                        }
                        //
                        if(obj->queues.reuse.use <= 0){
                            NIX_PRINTF_ERROR("Nix_OpenALRecorder_prepare::no reusable buffer could be allocated.\n");
                        } else {
                            //prepared
                            obj->queues.filling.iCurSample = 0;
                            obj->queues.conv = conv; conv = NULL; //consume
                            obj->queues.filling.tmp = tmpBuff; tmpBuff = NULL; //consume
                            obj->queues.filling.tmpSz = capMainBuffSz;
                            //cfg
                            obj->cfg.fmt = *audioDesc;
                            obj->cfg.maxBuffers = buffersCount;
                            obj->cfg.samplesPerBuffer = samplesPerBuffer;
                            //
                            obj->capFmt = inDesc;
                            r = NIX_TRUE;
                        }
                    }
                    //release (if not consumed)
                    if(conv != NULL){
                        nixFmtConverter_destroy(conv);
                        conv = NULL;
                    }
                }
                //release (if not consumed)
                if(tmpBuff != NULL){
                    NIX_FREE(tmpBuff);
                    tmpBuff = NULL;
                }
                obj->idCaptureAL = capDev;
            }
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    return r;
}

NixBOOL Nix_OpenALRecorder_setCallback(STNix_OpenALRecorder* obj, NixApiCaptureBufferFilledCallback callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    {
        obj->callback.func = callback;
        obj->callback.data = callbackData;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_OpenALRecorder_start(STNix_OpenALRecorder* obj){
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

NixBOOL Nix_OpenALRecorder_stop(STNix_OpenALRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(obj->idCaptureAL != NIX_OPENAL_NULL){
        ALenum errorAL;
        alcCaptureStop(obj->idCaptureAL);
        if(AL_NO_ERROR != (errorAL = alGetError())){
            NIX_PRINTF_ERROR("alcCaptureStop failed: #%d '%s'.\n", errorAL, alGetString(errorAL));
        }
        obj->engStarted = NIX_FALSE;
    }
    Nix_OpenALRecorder_flush(obj);
    return r;
}

NixBOOL Nix_OpenALRecorder_flush(STNix_OpenALRecorder* obj){
    NixBOOL r = NIX_TRUE;
    //move filling buffer to notify (if data is available)
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    if(obj->queues.reuse.use > 0){
        STNix_OpenALQueuePair* pair = &obj->queues.reuse.arr[0];
        if(pair->org != NULL && pair->org->use > 0){
            obj->queues.filling.iCurSample = 0;
            if(!Nix_OpenALQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                //program logic error
                r = NIX_FALSE;
            }
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    return r;
}

void Nix_OpenALRecorder_consumeInputBuffer(STNix_OpenALRecorder* obj){
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
        NIX_MUTEX_LOCK(&obj->queues.mutex);
        {
            //process
            while(inIdx < inSz){
                if(obj->queues.reuse.use <= 0){
                    //move oldest-notify buffer to reuse
                    if(!Nix_OpenALQueue_popMovingTo(&obj->queues.notify, &obj->queues.reuse)){
                        //program logic error
                        NIX_ASSERT(NIX_FALSE);
                        break;
                    }
                } else {
                    STNix_OpenALQueuePair* pair = &obj->queues.reuse.arr[0];
                    if(pair->org == NULL || pair->org->desc.blockAlign <= 0){
                        //just remove
                        STNix_OpenALQueuePair pair;
                        if(!Nix_OpenALQueue_popOrphaning(&obj->queues.reuse, &pair)){
                            NIX_ASSERT(NIX_FALSE);
                            //program logic error
                            break;
                        }
                        if(pair.org != NULL){
                            Nix_OpenALPCMBuffer_destroy(pair.org);
                            NIX_FREE(pair.org);
                            pair.org = NULL;
                        }
                        Nix_OpenALQueuePair_destroy(&pair);
                    } else {
                        const NixUI32 outSz = (pair->org->sz / pair->org->desc.blockAlign);
                        const NixUI32 outAvail = (obj->queues.filling.iCurSample >= outSz ? 0 : outSz - obj->queues.filling.iCurSample);
                        const NixUI32 inAvail = inSz - inIdx;
                        NixUI32 ammBlocksRead = 0, ammBlocksWritten = 0;
                        if(outAvail > 0 && inAvail > 0){
                            //dst
                            nixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv, &pair->org->desc, pair->org->ptr, obj->queues.filling.iCurSample);
                            //src
                            nixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &obj->capFmt, obj->queues.filling.tmp, inIdx);
                            //convert
                            if(!nixFmtConverter_convert(obj->queues.conv, inAvail, outAvail, &ammBlocksRead, &ammBlocksWritten)){
                                //error
                                break;
                            } else if(ammBlocksRead == 0 && ammBlocksWritten == 0){
                                //converter did nothing, avoid infinite cycle
                                break;
                            } else {
                                inIdx += ammBlocksRead;
                                obj->queues.filling.iCurSample += ammBlocksWritten;
                                pair->org->use = (obj->queues.filling.iCurSample * pair->org->desc.blockAlign); NIX_ASSERT(pair->org->use <= pair->org->sz)
                            }
                        }
                        //move reusable buffer to notify
                        if(ammBlocksWritten == outAvail){
                            obj->queues.filling.iCurSample = 0;
                            if(!Nix_OpenALQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                                //program logic error
                                NIX_ASSERT(NIX_FALSE);
                                break;
                            }
                        }
                    }
                }
            }
        }
        NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    }
}

void Nix_OpenALRecorder_notifyBuffers(STNix_OpenALRecorder* obj){
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    {
        const NixUI32 maxProcess = obj->queues.notify.use;
        NixUI32 ammProcessed = 0;
        while(ammProcessed < maxProcess && obj->queues.notify.use > 0){
            STNix_OpenALQueuePair pair;
            if(!Nix_OpenALQueue_popOrphaning(&obj->queues.notify, &pair)){
                NIX_ASSERT(NIX_FALSE);
                //program logic error
                break;
            } else {
                //notify (unlocked)
                if(pair.org != NULL && pair.org->desc.blockAlign > 0 && obj->callback.func != NULL){
                    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
                    {
                        (*obj->callback.func)((STNixApiEngine){ obj->engNx }, (STNixApiRecorder){ obj }, pair.org->desc, pair.org->ptr, pair.org->use, (pair.org->use / pair.org->desc.blockAlign), obj->callback.data);
                    }
                    NIX_MUTEX_LOCK(&obj->queues.mutex);
                }
                //move to reuse
                if(!Nix_OpenALQueue_pushOwning(&obj->queues.reuse, &pair)){
                    //program logic error
                    NIX_ASSERT(NIX_FALSE);
                    if(pair.org != NULL){
                        Nix_OpenALPCMBuffer_destroy(pair.org);
                        NIX_FREE(pair.org);
                        pair.org = NULL;
                    }
                    Nix_OpenALQueuePair_destroy(&pair);
                }
            }
            //processed
            ++ammProcessed;
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
}

//------
//Engine API
//------

STNixApiEngine nixOpenALEngine_create(void){
    STNixApiEngine r = STNixApiEngine_Zero;
    STNix_OpenALEngine* obj = NULL;
    NIX_MALLOC(obj, STNix_OpenALEngine, sizeof(STNix_OpenALEngine), "STNix_OpenALEngine");
    if(obj != NULL){
        Nix_OpenALEngine_init(obj);
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
                } else {
                    obj->contextALIsCurrent = NIX_TRUE;
                    //Masc of capabilities
                    obj->maskCapabilities   |= (alcIsExtensionPresent(obj->deviceAL, "ALC_EXT_CAPTURE") != ALC_FALSE || alcIsExtensionPresent(obj->deviceAL, "ALC_EXT_capture") != ALC_FALSE) ? NIX_CAP_AUDIO_CAPTURE : 0;
                    obj->maskCapabilities   |= (alIsExtensionPresent("AL_EXT_STATIC_BUFFER") != AL_FALSE) ? NIX_CAP_AUDIO_STATIC_BUFFERS : 0;
                    obj->maskCapabilities   |= (alIsExtensionPresent("AL_EXT_OFFSET") != AL_FALSE) ? NIX_CAP_AUDIO_SOURCE_OFFSETS : 0;
                    //
                    r.opq = obj; obj = NULL; //consume
                }
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            Nix_OpenALEngine_destroy(obj);
            obj = NULL;
        }
    }
    return r;
}

void nixOpenALEngine_destroy(STNixApiEngine pObj){
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
    if(obj != NULL){
        Nix_OpenALEngine_destroy(obj);
        obj = NULL;
    }
}

void nixOpenALEngine_printCaps(STNixApiEngine pObj){
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
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

NixBOOL nixOpenALEngine_ctxIsActive(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
    if(obj != NULL){
        r = obj->contextALIsCurrent;
    }
    return r;
}

NixBOOL nixOpenALEngine_ctxActivate(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
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

NixBOOL nixOpenALEngine_ctxDeactivate(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
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

void nixOpenALEngine_tick(STNixApiEngine pObj){
    STNix_OpenALEngine* obj = (STNix_OpenALEngine*)pObj.opq;
    if(obj != NULL){
        Nix_OpenALEngine_tick(obj, NIX_FALSE);
    }
}

//------
//PCMBuffer API
//------

STNixApiBuffer nixOpenALPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixApiBuffer r = STNixApiBuffer_Zero;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        STNix_OpenALPCMBuffer* obj = NULL;
        NIX_MALLOC(obj, STNix_OpenALPCMBuffer, sizeof(STNix_OpenALPCMBuffer), "STNix_OpenALPCMBuffer");
        if(obj != NULL){
            Nix_OpenALPCMBuffer_init(obj);
            if(!Nix_OpenALPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes)){
                NIX_PRINTF_ERROR("nixOpenALPCMBuffer_create::Nix_OpenALPCMBuffer_setData failed.\n");
                Nix_OpenALPCMBuffer_destroy(obj);
                NIX_FREE(obj);
                obj = NULL;
            }
            r.opq = obj;
        }
    }
    return r;
}

void nixOpenALPCMBuffer_destroy(STNixApiBuffer pObj){
    if(pObj.opq != NULL){
        STNix_OpenALPCMBuffer* obj = (STNix_OpenALPCMBuffer*)pObj.opq;
        Nix_OpenALPCMBuffer_destroy(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}
   
NixBOOL nixOpenALPCMBuffer_setData(STNixApiBuffer pObj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && audioDesc != NULL && audioDesc->blockAlign > 0){
        STNix_OpenALPCMBuffer* obj = (STNix_OpenALPCMBuffer*)pObj.opq;
        r = Nix_OpenALPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

NixBOOL nixOpenALPCMBuffer_fillWithZeroes(STNixApiBuffer pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_OpenALPCMBuffer* obj = (STNix_OpenALPCMBuffer*)pObj.opq;
        r = Nix_OpenALPCMBuffer_fillWithZeroes(obj);
    }
    return r;
}

//------
//Source API
//------

STNixApiSource nixOpenALSource_create(STNixApiEngine pEng){
    STNixApiSource r = STNixApiSource_Zero;
    STNix_OpenALEngine* eng = (STNix_OpenALEngine*)pEng.opq;
    if(eng != NULL){
        STNix_OpenALSource* obj = NULL;
        NIX_MALLOC(obj, STNix_OpenALSource, sizeof(STNix_OpenALSource), "STNix_OpenALSource");
        if(obj != NULL){
            Nix_OpenALSource_init(obj);
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
                if(!Nix_OpenALEngine_srcsAdd(eng, obj)){
                    NIX_PRINTF_ERROR("nixOpenALSource_create::Nix_OpenALEngine_srcsAdd failed.\n");
                } else {
                    r.opq = obj; obj = NULL; //consume
                }
            }
        }
        //release (if not consumed)
        if(obj != NULL){
            Nix_OpenALSource_release(obj);
            NIX_FREE(obj);
            obj = NULL;
        }
    }
    return r;
}

void nixOpenALSource_removeAllBuffersAndNotify_(STNix_OpenALSource* obj){
    STNix_OpenALNotifQueue notifs;
    Nix_OpenALNotifQueue_init(&notifs);
    //move all pending buffers to notify
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    {
        Nix_OpenALSource_pendMoveAllBuffsToNotifyWithoutPoppingLocked_(obj);
        Nix_OpenALEngine_tick_addQueueNotifSrcLocked_(&notifs, obj);
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    //notify
    {
        NixUI32 i;
        for(i = 0; i < notifs.use; ++i){
            STNix_OpenALSrcNotif* n = &notifs.arr[i];
            NIX_PRINTF_INFO("nixOpenALSource_removeAllBuffersAndNotify_::notify(#%d/%d).\n", i + 1, notifs.use);
            if(n->callback.func != NULL){
                (*n->callback.func)(n->callback.eng, n->callback.sourceIndex, n->ammBuffs);
            }
        }
    }
}

void nixOpenALSource_destroy(STNixApiSource pObj){ //orphans the source, will automatically be destroyed after internal cleanup
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        //close
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourceStop(obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
        }
        Nix_OpenALSource_setIsOrphan(obj); //source is waiting for close(), wait for the change of state and Nix_OpenALSource_release + NIX_FREE.
        Nix_OpenALSource_setIsPlaying(obj, NIX_FALSE);
        Nix_OpenALSource_setIsPaused(obj, NIX_FALSE);
        //flush all pending buffers
        nixOpenALSource_removeAllBuffersAndNotify_(obj);
    }
}

void nixOpenALSource_setCallback(STNixApiSource pObj, void (*callback)(void* eng, const NixUI32 sourceIndex, const NixUI32 ammBuffs), void* callbackEng, NixUI32 callbackSourceIndex){
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        obj->queues.callback.func = callback;
        obj->queues.callback.eng = callbackEng;
        obj->queues.callback.sourceIndex = callbackSourceIndex;
    }
}

NixBOOL nixOpenALSource_setVolume(STNixApiSource pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        obj->volume = vol;
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcef(obj->idSourceAL, AL_GAIN, vol);
            NIX_OPENAL_ERR_VERIFY("alSourcef(AL_GAIN)");
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixOpenALSource_setRepeat(STNixApiSource pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        Nix_OpenALSource_setIsRepeat(obj, isRepeat);
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcei(obj->idSourceAL, AL_LOOPING, isRepeat ? AL_TRUE : AL_FALSE); NIX_OPENAL_ERR_VERIFY("alSourcei(AL_LOOPING)");
        }
        r = NIX_TRUE;
    }
    return r;
}

void nixOpenALSource_play(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourcePlay(obj->idSourceAL);    NIX_OPENAL_ERR_VERIFY("alSourcePlay");
        }
        Nix_OpenALSource_setIsPlaying(obj, NIX_TRUE);
        Nix_OpenALSource_setIsPaused(obj, NIX_FALSE);
    }
}

void nixOpenALSource_pause(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        if(obj->idSourceAL != NULL){
            alSourcePause(obj->idSourceAL);    NIX_OPENAL_ERR_VERIFY("alSourcePause");
        }
        Nix_OpenALSource_setIsPaused(obj, NIX_TRUE);
    }
}

void nixOpenALSource_stop(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            alSourceStop(obj->idSourceAL); NIX_OPENAL_ERR_VERIFY("alSourceStop");
        }
        Nix_OpenALSource_setIsPlaying(obj, NIX_FALSE);
        Nix_OpenALSource_setIsPaused(obj, NIX_FALSE);
        //flush all pending buffers
        nixOpenALSource_removeAllBuffersAndNotify_(obj);
    }
}

NixBOOL nixOpenALSource_isPlaying(STNixApiSource pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        if(obj->idSourceAL != NIX_OPENAL_NULL){
            ALint sourceState;
            alGetSourcei(obj->idSourceAL, AL_SOURCE_STATE, &sourceState);    NIX_OPENAL_ERR_VERIFY("alGetSourcei(AL_SOURCE_STATE)");
            r = sourceState == AL_PLAYING ? NIX_TRUE : NIX_FALSE;
        }
    }
    return r;
}

NixBOOL nixOpenALSource_isPaused(STNixApiSource pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_OpenALSource* obj = (STNix_OpenALSource*)pObj.opq;
        r = Nix_OpenALSource_isPlaying(obj) && Nix_OpenALSource_isPaused(obj) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

ALenum nixOpenALSource_alFormat(const STNix_audioDesc* fmt){
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

NixBOOL nixOpenALSource_prepareSourceForFmtAndSz_(STNix_OpenALSource* obj, const STNix_audioDesc* fmt, const NixUI32 buffSz){
    NixBOOL r = NIX_FALSE;
    if(fmt != NULL && fmt->blockAlign > 0 && fmt->bitsPerSample > 0 && fmt->channels > 0 && fmt->samplerate > 0 && fmt->samplesFormat > ENNix_sampleFormat_none && fmt->samplesFormat <= ENNix_sampleFormat_count){
        ALenum fmtAL = nixOpenALSource_alFormat(fmt);
        if(fmtAL != AL_UNDETERMINED){
            //buffer format cmpatible qith OpenAL
            obj->buffsFmt   = *fmt;
            obj->srcFmtAL   = fmtAL;
            obj->srcFmt     = *fmt;
            r = NIX_TRUE;
        } else {
            //prepare converter
            void* conv = nixFmtConverter_create();
            STNix_audioDesc convFmt = STNix_audioDesc_Zero;
            convFmt.channels        = fmt->channels;
            convFmt.bitsPerSample   = 16;
            convFmt.samplerate      = fmt->samplerate;
            convFmt.samplesFormat   = ENNix_sampleFormat_int;
            convFmt.blockAlign      = (convFmt.bitsPerSample / 8) * convFmt.channels;
            if(!nixFmtConverter_prepare(conv, fmt, &convFmt)){
                NIX_PRINTF_ERROR("nixOpenALSource_prepareSourceForFmtAndSz_, nixFmtConverter_prepare failed.\n");
            } else {
                fmtAL = nixOpenALSource_alFormat(&convFmt);
                if(fmtAL == AL_UNDETERMINED){
                    NIX_PRINTF_ERROR("nixOpenALSource_prepareSourceForFmtAndSz_, nixFmtConverter_prepare sucess but OpenAL unsupported format.\n");
                } else {
                    //allocate conversion buffer
                    const NixUI32 srcSamples = buffSz / fmt->blockAlign * fmt->blockAlign;
                    const NixUI32 cnvBytes = srcSamples * convFmt.blockAlign;
                    NIX_ASSERT(obj->queues.conv.buff.ptr == NULL && obj->queues.conv.buff.sz == 0)
                    NIX_MALLOC(obj->queues.conv.buff.ptr, void, cnvBytes, "obj->queues.conv.buff.ptr");
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
                nixFmtConverter_destroy(conv);
                conv = NULL;
            }
        }
    }
    return r;
}
    
NixBOOL nixOpenALSource_setBuffer(STNixApiSource pObj, STNixApiBuffer pBuff){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && pBuff.opq != NULL){
        STNix_OpenALSource* obj    = (STNix_OpenALSource*)pObj.opq;
        STNix_OpenALPCMBuffer* buff = (STNix_OpenALPCMBuffer*)pBuff.opq;
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
        } else if(Nix_OpenALSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, source is already static.\n");
        } else if(!STNix_audioDesc_IsEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            Nix_OpenALSource_setIsStatic(obj, NIX_TRUE);
            //schedule
            NixBOOL isStream = NIX_FALSE;
            if(!Nix_OpenALSource_queueBufferForOutput(obj, buff, isStream)){
                NIX_PRINTF_ERROR("nixOpenALSource_setBuffer, Nix_OpenALSource_queueBufferForOutput failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixOpenALSource_queueBuffer(STNixApiSource pObj, STNixApiBuffer pBuff){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && pBuff.opq != NULL){
        STNix_OpenALSource* obj    = (STNix_OpenALSource*)pObj.opq;
        STNix_OpenALPCMBuffer* buff = (STNix_OpenALPCMBuffer*)pBuff.opq;
        if(obj->buffsFmt.blockAlign == 0){
            if(!nixOpenALSource_prepareSourceForFmtAndSz_(obj, &buff->desc, buff->sz)){
                //error
            }
        }
        //
        if(obj->idSourceAL == NIX_OPENAL_NULL){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, no source available.\n");
        } else if(Nix_OpenALSource_isStatic(obj)){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, source is static.\n");
        } else if(!STNix_audioDesc_IsEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, new buffer doesnt match first buffer's format.\n");
        } else {
            //schedule
            NixBOOL isStream = NIX_TRUE;
            if(!Nix_OpenALSource_queueBufferForOutput(obj, buff, isStream)){
                NIX_PRINTF_ERROR("nixOpenALSource_queueBuffer, Nix_OpenALSource_queueBufferForOutput failed.\n");
            } else {
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

//------
//Recorder API
//------

STNixApiRecorder nixOpenALRecorder_create(STNixApiEngine pEng, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer){
    STNixApiRecorder r = STNixApiRecorder_Zero;
    STNix_OpenALEngine* eng = (STNix_OpenALEngine*)pEng.opq;
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0 && audioDesc->blockAlign > 0 && eng->rec == NULL){
        STNix_OpenALRecorder* obj = NULL;
        NIX_MALLOC(obj, STNix_OpenALRecorder, sizeof(STNix_OpenALRecorder), "STNix_OpenALRecorder");
        if(obj != NULL){
            Nix_OpenALRecorder_init(obj);
            if(!Nix_OpenALRecorder_prepare(obj, audioDesc, buffersCount, samplesPerBuffer)){
                NIX_PRINTF_ERROR("Nix_OpenALRecorder_create, Nix_OpenALRecorder_prepare failed.\n");
                Nix_OpenALRecorder_destroy(obj);
                NIX_FREE(obj);
                obj = NULL;
            } else {
                obj->engNx = eng;
                eng->rec = obj;
            }
        }
        r.opq = obj;
    }
    return r;
}

void nixOpenALRecorder_destroy(STNixApiRecorder pObj){
    STNix_OpenALRecorder* obj = (STNix_OpenALRecorder*)pObj.opq;
    if(obj != NULL){
        if(obj->engNx != NULL && obj->engNx->rec == obj){
            obj->engNx->rec = NULL;
        }
        Nix_OpenALRecorder_destroy(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}

NixBOOL nixOpenALRecorder_setCallback(STNixApiRecorder pObj, NixApiCaptureBufferFilledCallback callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALRecorder* obj = (STNix_OpenALRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_OpenALRecorder_setCallback(obj, callback, callbackData);
    }
    return r;
}

NixBOOL nixOpenALRecorder_start(STNixApiRecorder pObj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALRecorder* obj = (STNix_OpenALRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_OpenALRecorder_start(obj);
    }
    return r;
}

NixBOOL nixOpenALRecorder_stop(STNixApiRecorder pObj){
    NixBOOL r = NIX_FALSE;
    STNix_OpenALRecorder* obj = (STNix_OpenALRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_OpenALRecorder_stop(obj);
    }
    return r;
}
