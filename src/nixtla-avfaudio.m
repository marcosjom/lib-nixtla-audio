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
#include "nixtla-audio.h"
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
STNixApiEngine  nixAVAudioEngine_create(void);
void            nixAVAudioEngine_destroy(STNixApiEngine obj);
void            nixAVAudioEngine_printCaps(STNixApiEngine obj);
NixBOOL         nixAVAudioEngine_ctxIsActive(STNixApiEngine obj);
NixBOOL         nixAVAudioEngine_ctxActivate(STNixApiEngine obj);
NixBOOL         nixAVAudioEngine_ctxDeactivate(STNixApiEngine obj);
void            nixAVAudioEngine_tick(STNixApiEngine obj);
//PCMBuffer
STNixApiBuffer  nixAVAudioPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
void            nixAVAudioPCMBuffer_destroy(STNixApiBuffer obj);
NixBOOL         nixAVAudioPCMBuffer_setData(STNixApiBuffer obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL         nixAVAudioPCMBuffer_fillWithZeroes(STNixApiBuffer obj);
//Source
STNixApiSource  nixAVAudioSource_create(STNixApiEngine eng);
void            nixAVAudioSource_destroy(STNixApiSource obj);
void            nixAVAudioSource_setCallback(STNixApiSource obj, void (*callback)(void* eng, const NixUI32 sourceIndex, const NixUI32 ammBuffs), void* callbackEng, NixUI32 callbackSourceIndex);
NixBOOL         nixAVAudioSource_setVolume(STNixApiSource obj, const float vol);
NixBOOL         nixAVAudioSource_setRepeat(STNixApiSource obj, const NixBOOL isRepeat);
void            nixAVAudioSource_play(STNixApiSource obj);
void            nixAVAudioSource_pause(STNixApiSource obj);
void            nixAVAudioSource_stop(STNixApiSource obj);
NixBOOL         nixAVAudioSource_isPlaying(STNixApiSource obj);
NixBOOL         nixAVAudioSource_isPaused(STNixApiSource obj);
NixBOOL         nixAVAudioSource_setBuffer(STNixApiSource obj, STNixApiBuffer buff);  //static-source
NixBOOL         nixAVAudioSource_queueBuffer(STNixApiSource obj, STNixApiBuffer buff); //stream-source
//Recorder
STNixApiRecorder nixAVAudioRecorder_create(STNixApiEngine eng, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer);
void            nixAVAudioRecorder_destroy(STNixApiRecorder obj);
NixBOOL         nixAVAudioRecorder_setCallback(STNixApiRecorder obj, NixApiCaptureBufferFilledCallback callback, void* callbackData);
NixBOOL         nixAVAudioRecorder_start(STNixApiRecorder obj);
NixBOOL         nixAVAudioRecorder_stop(STNixApiRecorder obj);

NixBOOL nixAVAudioEngine_getApiItf(STNixApiItf* dst){
    NixBOOL r = NIX_FALSE;
    if(dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->engine.create      = nixAVAudioEngine_create;
        dst->engine.destroy     = nixAVAudioEngine_destroy;
        dst->engine.printCaps   = nixAVAudioEngine_printCaps;
        dst->engine.ctxIsActive = nixAVAudioEngine_ctxIsActive;
        dst->engine.ctxActivate = nixAVAudioEngine_ctxActivate;
        dst->engine.ctxDeactivate = nixAVAudioEngine_ctxDeactivate;
        dst->engine.tick        = nixAVAudioEngine_tick;
        //PCMBuffer
        dst->buffer.create      = nixAVAudioPCMBuffer_create;
        dst->buffer.destroy     = nixAVAudioPCMBuffer_destroy;
        dst->buffer.setData     = nixAVAudioPCMBuffer_setData;
        dst->buffer.fillWithZeroes = nixAVAudioPCMBuffer_fillWithZeroes;
        //Source
        dst->source.create      = nixAVAudioSource_create;
        dst->source.destroy     = nixAVAudioSource_destroy;
        dst->source.setCallback = nixAVAudioSource_setCallback;
        dst->source.setVolume   = nixAVAudioSource_setVolume;
        dst->source.setRepeat   = nixAVAudioSource_setRepeat;
        dst->source.play        = nixAVAudioSource_play;
        dst->source.pause       = nixAVAudioSource_pause;
        dst->source.stop        = nixAVAudioSource_stop;
        dst->source.isPlaying   = nixAVAudioSource_isPlaying;
        dst->source.isPaused    = nixAVAudioSource_isPaused;
        dst->source.setBuffer   = nixAVAudioSource_setBuffer;  //static-source
        dst->source.queueBuffer = nixAVAudioSource_queueBuffer; //stream-source
        //Recorder
        dst->recorder.create    = nixAVAudioRecorder_create;
        dst->recorder.destroy   = nixAVAudioRecorder_destroy;
        dst->recorder.setCallback = nixAVAudioRecorder_setCallback;
        dst->recorder.start     = nixAVAudioRecorder_start;
        dst->recorder.stop      = nixAVAudioRecorder_stop;
        //
        r = NIX_TRUE;
    }
    return r;
}

//

struct STNix_AVAudioEngine_;
struct STNix_AVAudioSource_;
struct STNix_AVAudioSourceCallback_;
struct STNix_AVAudioQueue_;
struct STNix_AVAudioQueuePair_;
struct STNix_AVAudioSrcNotif_;
struct STNix_AVAudioNotifQueue_;
struct STNix_AVAudioPCMBuffer_;
struct STNix_AVAudioRecorder_;

//------
//Engine
//------

typedef struct STNix_AVAudioEngine_ {
    //srcs
    struct {
        NIX_MUTEX_T                 mutex;
        struct STNix_AVAudioSource_** arr;
        NixUI32                     use;
        NixUI32                     sz;
        NixUI32 changingStateCountHint;
    } srcs;
    //
    struct STNix_AVAudioRecorder_* rec;
} STNix_AVAudioEngine;

void Nix_AVAudioEngine_init(STNix_AVAudioEngine* obj);
void Nix_AVAudioEngine_destroy(STNix_AVAudioEngine* obj);
NixBOOL Nix_AVAudioEngine_srcsAdd(STNix_AVAudioEngine* obj, struct STNix_AVAudioSource_* src);
void Nix_AVAudioEngine_tick(STNix_AVAudioEngine* obj, const NixBOOL isFinalCleanup);

//------
//Notif
//------

typedef struct STNix_AVAudioSourceCallback_ {
    void            (*func)(void* pEng, const NixUI32 sourceIndex, const NixUI32 ammBuffs);
    void*           eng;
    NixUI32         sourceIndex;
} STNix_AVAudioSourceCallback;

typedef struct STNix_AVAudioSrcNotif_ {
    STNix_AVAudioSourceCallback callback;
    NixUI32 ammBuffs;
} STNix_AVAudioSrcNotif;

void Nix_AVAudioSrcNotif_init(STNix_AVAudioSrcNotif* obj);
void Nix_AVAudioSrcNotif_destroy(STNix_AVAudioSrcNotif* obj);

//------
//NotifQueue
//------

typedef struct STNix_AVAudioNotifQueue_ {
    STNix_AVAudioSrcNotif*  arr;
    NixUI32                use;
    NixUI32                sz;
    STNix_AVAudioSrcNotif  arrEmbedded[32];
} STNix_AVAudioNotifQueue;

void Nix_AVAudioNotifQueue_init(STNix_AVAudioNotifQueue* obj);
void Nix_AVAudioNotifQueue_destroy(STNix_AVAudioNotifQueue* obj);
//
NixBOOL Nix_AVAudioNotifQueue_push(STNix_AVAudioNotifQueue* obj, STNix_AVAudioSrcNotif* pair);

//------
//PCMBuffer
//------

typedef struct STNix_AVAudioPCMBuffer_ {
    STNix_audioDesc desc;
    NixUI8*         ptr;
    NixUI32         use;
    NixUI32         sz;
} STNix_AVAudioPCMBuffer;

void Nix_AVAudioPCMBuffer_init(STNix_AVAudioPCMBuffer* obj);
void Nix_AVAudioPCMBuffer_destroy(STNix_AVAudioPCMBuffer* obj);
NixBOOL Nix_AVAudioPCMBuffer_setData(STNix_AVAudioPCMBuffer* obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL Nix_AVAudioPCMBuffer_fillWithZeroes(STNix_AVAudioPCMBuffer* obj);

//------
//QueuePair (Buffers)
//------

typedef struct STNix_AVAudioQueuePair_ {
    STNix_AVAudioPCMBuffer*  org;    //original buffer (owned by the user)
    AVAudioPCMBuffer*        cnv;    //converted buffer (owned by the source)
} STNix_AVAudioQueuePair;

void Nix_AVAudioQueuePair_init(STNix_AVAudioQueuePair* obj);
void Nix_AVAudioQueuePair_destroy(STNix_AVAudioQueuePair* obj);

//------
//Queue (Buffers)
//------

typedef struct STNix_AVAudioQueue_ {
    STNix_AVAudioQueuePair* arr;
    NixUI32                 use;
    NixUI32                 sz;
} STNix_AVAudioQueue;

void Nix_AVAudioQueue_init(STNix_AVAudioQueue* obj);
void Nix_AVAudioQueue_destroy(STNix_AVAudioQueue* obj);
//
NixBOOL Nix_AVAudioQueue_flush(STNix_AVAudioQueue* obj, const NixBOOL nullifyOrgs);
NixBOOL Nix_AVAudioQueue_prepareForSz(STNix_AVAudioQueue* obj, const NixUI32 minSz);
NixBOOL Nix_AVAudioQueue_pushOwning(STNix_AVAudioQueue* obj, STNix_AVAudioQueuePair* pair);
NixBOOL Nix_AVAudioQueue_popOrphaning(STNix_AVAudioQueue* obj, STNix_AVAudioQueuePair* dst);
NixBOOL Nix_AVAudioQueue_popMovingTo(STNix_AVAudioQueue* obj, STNix_AVAudioQueue* other);

//------
//Source
//------

typedef struct STNix_AVAudioSource_ {
    STNix_audioDesc         buffsFmt;   //first attached buffers' format (defines the converter config)
    AVAudioPlayerNode*      src;    //AVAudioPlayerNode
    AVAudioEngine*          eng;    //AVAudioEngine
    //queues
    struct {
        NIX_MUTEX_T         mutex;
        void*               conv;   //nixFmtConverter
        STNix_AVAudioSourceCallback callback;
        STNix_AVAudioQueue  notify; //buffers (consumed, pending to notify)
        STNix_AVAudioQueue  reuse;  //buffers (conversion buffers)
        STNix_AVAudioQueue  pend;   //to be played/filled
        NixUI32             pendSampleIdx;  //current sample playing/filling
        NixUI32             pendScheduledCount;
    } queues;
    //packed bools to reduce padding
    NixBOOL                 engStarted;
    NixBOOL                 isRepeat;
    NixBOOL                 isPlaying;
    NixBOOL                 isPaused;
    NixBOOL                 isStatic;
} STNix_AVAudioSource;

void Nix_AVAudioSource_init(STNix_AVAudioSource* obj);
void Nix_AVAudioSource_release(STNix_AVAudioSource* obj);
void Nix_AVAudioSource_scheduleEnqueuedBuffers(STNix_AVAudioSource* obj);
NixBOOL Nix_AVAudioSource_queueBufferForOutput(STNix_AVAudioSource* obj, STNix_AVAudioPCMBuffer* buff);
NixBOOL Nix_AVAudioSource_pendPopOldestBuffLocked_(STNix_AVAudioSource* obj);
NixBOOL Nix_AVAudioSource_pendPopAllBuffsLocked_(STNix_AVAudioSource* obj);

//------
//Recorder
//------

typedef struct STNix_AVAudioRecorder_ {
    NixBOOL                 engStarted;
    STNix_AVAudioEngine*    engNx;
    AVAudioEngine*          eng;    //AVAudioEngine
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
        STNix_AVAudioQueue  notify;
        STNix_AVAudioQueue  reuse;
        //filling
        struct {
            NixSI32         iCurSample; //at first buffer in 'reuse'
        } filling;
    } queues;
} STNix_AVAudioRecorder;

void Nix_AVAudioRecorder_init(STNix_AVAudioRecorder* obj);
void Nix_AVAudioRecorder_destroy(STNix_AVAudioRecorder* obj);
//
NixBOOL Nix_AVAudioRecorder_prepare(STNix_AVAudioRecorder* obj, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer);
NixBOOL Nix_AVAudioRecorder_setCallback(STNix_AVAudioRecorder* obj, NixApiCaptureBufferFilledCallback callback, void* callbackData);
NixBOOL Nix_AVAudioRecorder_start(STNix_AVAudioRecorder* obj);
NixBOOL Nix_AVAudioRecorder_stop(STNix_AVAudioRecorder* obj);
NixBOOL Nix_AVAudioRecorder_flush(STNix_AVAudioRecorder* obj);
void Nix_AVAudioRecorder_notifyBuffers(STNix_AVAudioRecorder* obj);

//------
//nixFmtConverter
//------

void nixFmtConverter_buffFmtToAudioDesc(AVAudioFormat* buffFmt, STNix_audioDesc* dst);

//------
//Engine
//------

void Nix_AVAudioEngine_init(STNix_AVAudioEngine* obj){
    memset(obj, 0, sizeof(STNix_AVAudioEngine));
    //srcs
    {
        NIX_MUTEX_INIT(&obj->srcs.mutex);
    }
}

void Nix_AVAudioEngine_destroy(STNix_AVAudioEngine* obj){
    if(obj != NULL){
        //srcs
        {
            //cleanup
            while(obj->srcs.arr != NULL && obj->srcs.use > 0){
                Nix_AVAudioEngine_tick(obj, NIX_TRUE);
            }
            //
            if(obj->srcs.arr != NULL){
                NIX_FREE(obj->srcs.arr);
                obj->srcs.arr = NULL;
            }
            NIX_MUTEX_DESTROY(&obj->srcs.mutex);
        }
        //rec (recorder)
        if(obj->rec != NULL){
            obj->rec = NULL;
        }
        obj = NULL;
    }
}

NixBOOL Nix_AVAudioEngine_srcsAdd(STNix_AVAudioEngine* obj, struct STNix_AVAudioSource_* src){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        NIX_MUTEX_LOCK(&obj->srcs.mutex);
        {
            //resize array (if necesary)
            if(obj->srcs.use >= obj->srcs.sz){
                const NixUI32 szN = obj->srcs.use + 4;
                STNix_AVAudioSource** arrN = NULL;
                NIX_MALLOC(arrN, STNix_AVAudioSource*, sizeof(STNix_AVAudioSource*) * szN, "STNix_AVAudioEngine::srcsN");
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
                NIX_PRINTF_ERROR("nixAAudioSource_create::STNix_AVAudioEngine::srcs failed (no allocated space).\n");
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

void Nix_AVAudioEngine_removeSrcRecordLocked_(STNix_AVAudioEngine* obj, NixSI32* idx){
    STNix_AVAudioSource* src = obj->srcs.arr[*idx];
    if(src != NULL){
        Nix_AVAudioSource_release(src);
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

void Nix_AVAudioEngine_tick_addQueueNotifSrcLocked_(STNix_AVAudioNotifQueue* notifs, STNix_AVAudioSource* srcLocked){
    if(srcLocked->queues.notify.use > 0){
        const NixBOOL nullifyOrgs = NIX_TRUE;
        STNix_AVAudioSrcNotif n;
        Nix_AVAudioSrcNotif_init(&n);
        n.callback = srcLocked->queues.callback;
        n.ammBuffs = srcLocked->queues.notify.use;
        if(!Nix_AVAudioQueue_flush(&srcLocked->queues.notify, nullifyOrgs)){
            NIX_ASSERT(NIX_FALSE); //program logic error
        }
        if(!Nix_AVAudioNotifQueue_push(notifs, &n)){
            NIX_ASSERT(NIX_FALSE); //program logic error
            Nix_AVAudioSrcNotif_destroy(&n);
        }
    }
}
    
void Nix_AVAudioEngine_tick(STNix_AVAudioEngine* obj, const NixBOOL isFinalCleanup){
    if(obj != NULL){
        //srcs
        {
            STNix_AVAudioNotifQueue notifs;
            Nix_AVAudioNotifQueue_init(&notifs);
            NIX_MUTEX_LOCK(&obj->srcs.mutex);
            if(obj->srcs.arr != NULL && obj->srcs.use > 0){
                NixUI32 changingStateCount = 0;
                //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::%d sources.\n", obj->srcs.use);
                NixSI32 i; for(i = 0; i < (NixSI32)obj->srcs.use; ++i){
                    STNix_AVAudioSource* src = obj->srcs.arr[i];
                    //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::source(#%d/%d).\n", i + 1, obj->srcs.use);
                    if(src->src == NULL){
                        //remove
                        //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::source(#%d/%d); remove-NULL.\n", i + 1, obj->srcs.use);
                        Nix_AVAudioEngine_removeSrcRecordLocked_(obj, &i);
                        src = NULL;
                    } else {
                        //post-process
                        if(src != NULL){
                            //add to notify queue
                            {
                                NIX_MUTEX_LOCK(&src->queues.mutex);
                                {
                                    Nix_AVAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, src);
                                }
                                NIX_MUTEX_UNLOCK(&src->queues.mutex);
                            }
                        }
                    }
                }
                obj->srcs.changingStateCountHint = changingStateCount;
            }
            NIX_MUTEX_UNLOCK(&obj->srcs.mutex);
            //notify (unloked)
            if(notifs.use > 0){
                //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::notify %d.\n", notifs.use);
                NixUI32 i;
                for(i = 0; i < notifs.use; ++i){
                    STNix_AVAudioSrcNotif* n = &notifs.arr[i];
                    //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::notify(#%d/%d).\n", i + 1, notifs.use);
                    if(n->callback.func != NULL){
                        (*n->callback.func)(n->callback.eng, n->callback.sourceIndex, n->ammBuffs);
                    }
                }
            }
            //NIX_PRINTF_INFO("Nix_AVAudioEngine_tick::Nix_AVAudioNotifQueue_destroy.\n");
            Nix_AVAudioNotifQueue_destroy(&notifs);
        }
        //recorder
        if(obj->rec != NULL){
            Nix_AVAudioRecorder_notifyBuffers(obj->rec);
        }
    }
}

//------
//Notif
//------

void Nix_AVAudioSrcNotif_init(STNix_AVAudioSrcNotif* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_AVAudioSrcNotif_destroy(STNix_AVAudioSrcNotif* obj){
    //
}

//------
//NotifQueue
//------

void Nix_AVAudioNotifQueue_init(STNix_AVAudioNotifQueue* obj){
    memset(obj, 0, sizeof(*obj));
    obj->arr = obj->arrEmbedded;
    obj->sz = (sizeof(obj->arrEmbedded) / sizeof(obj->arrEmbedded[0]));
}

void Nix_AVAudioNotifQueue_destroy(STNix_AVAudioNotifQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNix_AVAudioSrcNotif* b = &obj->arr[i];
            Nix_AVAudioSrcNotif_destroy(b);
        }
        if(obj->arr != obj->arrEmbedded){
            NIX_FREE(obj->arr);
        }
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
}

NixBOOL Nix_AVAudioNotifQueue_push(STNix_AVAudioNotifQueue* obj, STNix_AVAudioSrcNotif* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNix_AVAudioSrcNotif* arrN = NULL;
            NIX_MALLOC(arrN, STNix_AVAudioSrcNotif, sizeof(STNix_AVAudioSrcNotif) * szN, "Nix_AVAudioNotifQueue_push::arrN");
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
            NIX_PRINTF_ERROR("Nix_AVAudioNotifQueue_push failed (no allocated space).\n");
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

void Nix_AVAudioPCMBuffer_init(STNix_AVAudioPCMBuffer* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_AVAudioPCMBuffer_destroy(STNix_AVAudioPCMBuffer* obj){
    if(obj->ptr != NULL){
        NIX_FREE(obj->ptr);
        obj->ptr = NULL;
    }
    obj->use = obj->sz = 0;
}

NixBOOL Nix_AVAudioPCMBuffer_setData(STNix_AVAudioPCMBuffer* obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        //resize buffer (if necesary)
        const NixUI32 reqBytes = (audioDataPCMBytes / audioDesc->blockAlign * audioDesc->blockAlign);
        if(obj->sz < reqBytes){
            NixUI8* dataN = NULL;
            NIX_MALLOC(dataN, NixUI8, reqBytes, "Nix_AVAudioPCMBuffer::dataN");
            if(dataN != NULL){
                if(obj->ptr != NULL){
                    NIX_FREE(obj->ptr);
                    obj->ptr = NULL;
                }
                obj->ptr = dataN;
                obj->sz = reqBytes;
                obj->use = 0;
            }
        }
        //
        if(obj->sz >= reqBytes){
            if(reqBytes <= 0){
                obj->desc   = *audioDesc;
                obj->use    = 0;
                r = NIX_TRUE;
            } else if(obj->ptr != NULL){
                if(audioDataPCM != NULL){
                    memcpy(obj->ptr, audioDataPCM, reqBytes);
                }
                obj->desc   = *audioDesc;
                obj->use    = reqBytes;
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL Nix_AVAudioPCMBuffer_fillWithZeroes(STNix_AVAudioPCMBuffer* obj){
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

void Nix_AVAudioQueuePair_init(STNix_AVAudioQueuePair* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_AVAudioQueuePair_destroy(STNix_AVAudioQueuePair* obj){
    NIX_ASSERT(obj->org == NULL) //program-logic error; should be always NULLyfied before the pair si destroyed
    if(obj->org != NULL){
        //Note: org is owned by the user, do not destroy
        obj->org = NULL;
    }
    if(obj->cnv != nil){
        [obj->cnv release];
        obj->cnv = nil;
    }
}

//------
//Queue (Buffers)
//------

void Nix_AVAudioQueue_init(STNix_AVAudioQueue* obj){
    memset(obj, 0, sizeof(*obj));
}

void Nix_AVAudioQueue_destroy(STNix_AVAudioQueue* obj){
    if(obj->arr != NULL){
        NixUI32 i; for(i = 0; i < obj->use; i++){
            STNix_AVAudioQueuePair* b = &obj->arr[i];
            Nix_AVAudioQueuePair_destroy(b);
        }
        NIX_FREE(obj->arr);
        obj->arr = NULL;
    }
    obj->use = obj->sz = 0;
}

NixBOOL Nix_AVAudioQueue_flush(STNix_AVAudioQueue* obj, const NixBOOL nullifyOrgs){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        if(obj->arr != NULL){
            NixUI32 i; for(i = 0; i < obj->use; i++){
                STNix_AVAudioQueuePair* b = &obj->arr[i];
                if(nullifyOrgs){
                    b->org = NULL;
                }
                Nix_AVAudioQueuePair_destroy(b);
            }
            obj->use = 0;
        }
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_AVAudioQueue_prepareForSz(STNix_AVAudioQueue* obj, const NixUI32 minSz){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL){
        //resize array (if necesary)
        if(minSz > obj->sz){
            const NixUI32 szN = minSz;
            STNix_AVAudioQueuePair* arrN = NULL;
            NIX_MALLOC(arrN, STNix_AVAudioQueuePair, sizeof(STNix_AVAudioQueuePair) * szN, "Nix_AVAudioQueue_prepareForSz::arrN");
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
            NIX_PRINTF_ERROR("Nix_AVAudioQueue_prepareForSz failed (no allocated space).\n");
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL Nix_AVAudioQueue_pushOwning(STNix_AVAudioQueue* obj, STNix_AVAudioQueuePair* pair){
    NixBOOL r = NIX_FALSE;
    if(obj != NULL && pair != NULL){
        //resize array (if necesary)
        if(obj->use >= obj->sz){
            const NixUI32 szN = obj->use + 4;
            STNix_AVAudioQueuePair* arrN = NULL;
            NIX_MALLOC(arrN, STNix_AVAudioQueuePair, sizeof(STNix_AVAudioQueuePair) * szN, "Nix_AVAudioQueue_pushOwning::arrN");
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
            NIX_PRINTF_ERROR("Nix_AVAudioQueue_pushOwning failed (no allocated space).\n");
        } else {
            //become the owner of the pair
            obj->arr[obj->use++] = *pair;
            r = NIX_TRUE;
        }
    }
    return r;
}

NixBOOL Nix_AVAudioQueue_popOrphaning(STNix_AVAudioQueue* obj, STNix_AVAudioQueuePair* dst){
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

NixBOOL Nix_AVAudioQueue_popMovingTo(STNix_AVAudioQueue* obj, STNix_AVAudioQueue* other){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioQueuePair pair;
    if(!Nix_AVAudioQueue_popOrphaning(obj, &pair)){
        //
    } else {
        if(!Nix_AVAudioQueue_pushOwning(other, &pair)){
            //program logic error
            NIX_ASSERT(NIX_FALSE);
            if(pair.org != NULL){
                Nix_AVAudioPCMBuffer_destroy(pair.org);
                NIX_FREE(pair.org);
                pair.org = NULL;
            }
            Nix_AVAudioQueuePair_destroy(&pair);
        } else {
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//Source
//------

void Nix_AVAudioSource_init(STNix_AVAudioSource* obj){
    memset(obj, 0, sizeof(STNix_AVAudioSource));
    //queues
    {
        NIX_MUTEX_INIT(&obj->queues.mutex);
        Nix_AVAudioQueue_init(&obj->queues.notify);
        Nix_AVAudioQueue_init(&obj->queues.pend);
        Nix_AVAudioQueue_init(&obj->queues.reuse);
    }
}

void Nix_AVAudioSource_release(STNix_AVAudioSource* obj){
    //src
    if(obj->src != nil){ [obj->src release]; obj->src = nil; }
    if(obj->eng != nil){ [obj->eng release]; obj->eng = nil; }
    //queues
    {
        if(obj->queues.conv != NULL){
            nixFmtConverter_destroy(obj->queues.conv);
            obj->queues.conv = NULL;
        }
        Nix_AVAudioQueue_destroy(&obj->queues.pend);
        Nix_AVAudioQueue_destroy(&obj->queues.reuse);
        Nix_AVAudioQueue_destroy(&obj->queues.notify);
        NIX_MUTEX_DESTROY(&obj->queues.mutex);
    }
}

void Nix_AVAudioSource_queueBufferScheduleCallback_(STNix_AVAudioSource* obj, AVAudioPCMBuffer* cnvBuff){
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    {
        NIX_ASSERT(obj->queues.pendScheduledCount > 0)
        if(obj->queues.pendScheduledCount > 0){
            //Note: AVFAudio calls callbacks not in the same order of scheduling.
/*#         ifdef NIX_ASSERTS_ACTIVATED
            if(obj->queues.pend.arr[0].cnv != cnvBuff){
                NIX_PRINTF_WARNING("Unqueued buffer does not match oldest buffer.\n");
                NixUI32 i; for(i = 0; i < obj->queues.pend.use; i++){
                    const STNix_AVAudioQueuePair* pair = &obj->queues.pend.arr[i];
                    NIX_PRINTF_WARNING("Buffer #%d/%d: %lld vs %lld%s.\n", (i + 1), obj->queues.pend.use, (long long)pair->cnv, (long long)cnvBuff, pair->cnv == cnvBuff ? " MATCH": "");
                }
            }
            NIX_ASSERT(obj->queues.pend.arr[0].cnv == cnvBuff)
#           endif*/
            --obj->queues.pendScheduledCount;
        }
        if(obj->isStatic){
            //schedule again
            if(obj->isPlaying && !obj->isPaused && obj->isRepeat && obj->queues.pend.use == 1 && obj->queues.pendScheduledCount == 0){
                ++obj->queues.pendScheduledCount;
                NIX_MUTEX_UNLOCK(&obj->queues.mutex);
                {
                    [obj->src scheduleBuffer:cnvBuff completionCallbackType:AVAudioPlayerNodeCompletionDataRendered completionHandler:^(AVAudioPlayerNodeCompletionCallbackType cbType) {
                        //printf("completionCallbackType: %s.\n", cbType == AVAudioPlayerNodeCompletionDataConsumed ? "DataConsumed" : cbType == AVAudioPlayerNodeCompletionDataRendered? "DataRendered" : cbType == AVAudioPlayerNodeCompletionDataPlayedBack ? "DataPlayedBack" : "unknown");
                        if(cbType == AVAudioPlayerNodeCompletionDataRendered){
                            Nix_AVAudioSource_queueBufferScheduleCallback_(obj, cnvBuff);
                        }
                    }];
                }
                NIX_MUTEX_LOCK(&obj->queues.mutex);
            } else {
                //pause
                obj->isPaused = NIX_TRUE;
            }
        } else {
            if(!Nix_AVAudioSource_pendPopOldestBuffLocked_(obj)){
                //remove from queue, to pend
            } else {
                //buff moved from pend to reuse and notif
            }
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
}

void Nix_AVAudioSource_scheduleEnqueuedBuffers(STNix_AVAudioSource* obj){
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    while(obj->queues.pendScheduledCount < obj->queues.pend.use){
        STNix_AVAudioQueuePair* pair = &obj->queues.pend.arr[obj->queues.pendScheduledCount];
        NIX_ASSERT(pair->cnv != nil)
        if(pair->cnv == nil){
            //program logic error
            break;
        } else {
            AVAudioPCMBuffer* cnv = pair->cnv;
            ++obj->queues.pendScheduledCount;
            NIX_MUTEX_UNLOCK(&obj->queues.mutex);
            {
                [obj->src scheduleBuffer:cnv completionCallbackType:AVAudioPlayerNodeCompletionDataRendered completionHandler:^(AVAudioPlayerNodeCompletionCallbackType cbType) {
                    //printf("completionCallbackType: %s.\n", cbType == AVAudioPlayerNodeCompletionDataConsumed ? "DataConsumed" : cbType == AVAudioPlayerNodeCompletionDataRendered? "DataRendered" : cbType == AVAudioPlayerNodeCompletionDataPlayedBack ? "DataPlayedBack" : "unknown");
                    if(cbType == AVAudioPlayerNodeCompletionDataRendered){
                        Nix_AVAudioSource_queueBufferScheduleCallback_(obj, cnv);
                    }
                }];
            }
            NIX_MUTEX_LOCK(&obj->queues.mutex);
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
}

NixBOOL Nix_AVAudioSource_queueBufferForOutput(STNix_AVAudioSource* obj, STNix_AVAudioPCMBuffer* buff){
    NixBOOL r = NIX_FALSE;
    if(!STNix_audioDesc_IsEqual(&obj->buffsFmt, &buff->desc)){
        //error
    } else if(buff->desc.blockAlign <= 0){
        //error
    } else {
        STNix_AVAudioQueuePair pair;
        Nix_AVAudioQueuePair_init(&pair);
        //prepare conversion buffer (if necesary)
        if(obj->eng != NULL && obj->queues.conv != NULL){
            AVAudioFormat* outFmt = [[obj->eng outputNode] outputFormatForBus:0];
            //create copy buffer
            const NixUI32 buffSamplesMax    = (buff->sz / buff->desc.blockAlign);
            const NixUI32 samplesReq        = nixFmtConverter_samplesForNewFrequency(buffSamplesMax, obj->buffsFmt.samplerate, [outFmt sampleRate]);
            //try to reuse buffer
            {
                STNix_AVAudioQueuePair reuse;
                if(Nix_AVAudioQueue_popOrphaning(&obj->queues.reuse, &reuse)){
                    //reusable buffer
                    NIX_ASSERT(reuse.org == NULL) //program logic error
                    NIX_ASSERT(reuse.cnv != NULL) //program logic error
                    if([reuse.cnv frameCapacity] < samplesReq){
                        [reuse.cnv release];
                        reuse.cnv = nil;
                    } else {
                        pair.cnv = reuse.cnv;
                        reuse.cnv = nil;
                    }
                    Nix_AVAudioQueuePair_destroy(&reuse);
                }
            }
            //create new buffer
            if(pair.cnv == nil){
                pair.cnv = [[AVAudioPCMBuffer alloc] initWithPCMFormat:outFmt frameCapacity:samplesReq];
                if(pair.cnv == NULL){
                    NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::pair.cnv could be allocated.\n");
                }
            }
            //convert
            if(pair.cnv == NULL){
                r = NIX_FALSE;
            } else if(!nixFmtConverter_setPtrAtSrcInterlaced(obj->queues.conv, &buff->desc, buff->ptr, 0)){
                NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                r = NIX_FALSE;
            } else {
                r = NIX_TRUE;
                STNix_audioDesc srcFmt;
                srcFmt.channels = [outFmt channelCount];
                srcFmt.samplerate = [outFmt sampleRate];
                NixBOOL isInterleavedDst = [outFmt isInterleaved];
                NixUI32 iCh, maxChannels = nixFmtConverter_maxChannels();
                switch([outFmt commonFormat]) {
                    case AVAudioPCMFormatFloat32:
                        srcFmt.samplesFormat = ENNix_sampleFormat_float;
                        srcFmt.bitsPerSample = 32;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!nixFmtConverter_setPtrAtDstChannel(obj->queues.conv, iCh, pair.cnv.floatChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_setPtrAtDstChannel failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    case AVAudioPCMFormatInt16:
                        srcFmt.samplesFormat = ENNix_sampleFormat_int;
                        srcFmt.bitsPerSample = 16;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!nixFmtConverter_setPtrAtDstChannel(obj->queues.conv, iCh, pair.cnv.int16ChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_setPtrAtDstChannel failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    case AVAudioPCMFormatInt32:
                        srcFmt.samplesFormat = ENNix_sampleFormat_int;
                        srcFmt.bitsPerSample = 32;
                        srcFmt.blockAlign = (srcFmt.bitsPerSample / 8) * (isInterleavedDst ? srcFmt.channels : 1);
                        for(iCh = 0; iCh < srcFmt.channels && iCh < maxChannels; ++iCh){
                            if(!nixFmtConverter_setPtrAtDstChannel(obj->queues.conv, iCh, pair.cnv.int32ChannelData[iCh], srcFmt.blockAlign)){
                                NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_setPtrAtDstChannel failed.\n");
                                r = NIX_FALSE;
                            }
                        }
                        break;
                    default:
                        NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::unexpected 'commonFormat'.\n");
                        r = NIX_FALSE;
                        break;
                }
                //conver
                if(r){
                    const NixUI32 srcBlocks = (buff->use / buff->desc.blockAlign);
                    const NixUI32 dstBlocks = [pair.cnv frameCapacity];
                    NixUI32 ammBlocksRead = 0;
                    NixUI32 ammBlocksWritten = 0;
                    if(!nixFmtConverter_convert(obj->queues.conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                        NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_convert failed from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s).\n"
                                         , obj->buffsFmt.samplerate
                                         , obj->buffsFmt.channels
                                         , obj->buffsFmt.bitsPerSample
                                         , obj->buffsFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : obj->buffsFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
                                         , srcFmt.samplerate
                                         , srcFmt.channels
                                         , srcFmt.bitsPerSample
                                         , srcFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : srcFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
                                         );
                        r = NIX_FALSE;
                    } else {
                        /*
#                       ifdef NIX_ASSERTS_ACTIVATED
                        if((ammBlocksRead * 100 / srcBlocks) < 90 || (ammBlocksWritten * 100 / dstBlocks) < 90){
                            NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::nixFmtConverter_convert under-consumption from(%uhz, %uch, %dbit-%s) to(%uhz, %uch, %dbit-%s) = %d%% of %u src-frames consumed, %d%% of %u dst-frames populated.\n"
                                             , obj->buffsFmt.samplerate
                                             , obj->buffsFmt.channels
                                             , obj->buffsFmt.bitsPerSample
                                             , obj->buffsFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : obj->buffsFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
                                             , srcFmt.samplerate
                                             , srcFmt.channels
                                             , srcFmt.bitsPerSample
                                             , srcFmt.samplesFormat == ENNix_sampleFormat_int ? "int" : srcFmt.samplesFormat == ENNix_sampleFormat_float ? "float" : "unknown"
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
            pair.org = buff;
            NIX_MUTEX_LOCK(&obj->queues.mutex);
            {
                if(!Nix_AVAudioQueue_pushOwning(&obj->queues.pend, &pair)){
                    NIX_PRINTF_ERROR("Nix_AVAudioSource_queueBufferForOutput::Nix_AVAudioQueue_pushOwning failed.\n");
                    pair.org = NULL;
                    r = NIX_FALSE;
                } else {
                    //added to queue
                    Nix_AVAudioQueue_prepareForSz(&obj->queues.reuse, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    Nix_AVAudioQueue_prepareForSz(&obj->queues.notify, obj->queues.pend.use); //this ensures malloc wont be calle inside a callback
                    //this is the first buffer i the queue
                    if(obj->queues.pend.use == 1){
                        obj->queues.pendSampleIdx = 0;
                    }
                }
            }
            NIX_MUTEX_UNLOCK(&obj->queues.mutex);
        }
        if(!r){
            Nix_AVAudioQueuePair_destroy(&pair);
        }
    }
    return r;
}

NixBOOL Nix_AVAudioSource_pendPopOldestBuffLocked_(STNix_AVAudioSource* obj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioQueuePair pair;
    if(!Nix_AVAudioQueue_popOrphaning(&obj->queues.pend, &pair)){
        NIX_ASSERT(NIX_FALSE); //program logic error
    } else {
        //move "cnv" to reusable queue
        if(pair.cnv != NULL){
            STNix_AVAudioQueuePair reuse;
            Nix_AVAudioQueuePair_init(&reuse);
            reuse.cnv = pair.cnv;
            if(!Nix_AVAudioQueue_pushOwning(&obj->queues.reuse, &reuse)){
                NIX_PRINTF_ERROR("Nix_AVAudioSource_pendPopOldestBuffLocked_::Nix_AVAudioQueue_pushOwning(reuse) failed.\n");
                reuse.cnv = NULL;
                Nix_AVAudioQueuePair_destroy(&reuse);
            } else {
                //now owned by reuse
                pair.cnv = NULL;
            }
        }
        //move "org" to notify queue
        if(pair.org != NULL){
            STNix_AVAudioQueuePair notif;
            Nix_AVAudioQueuePair_init(&notif);
            notif.org = pair.org;
            if(!Nix_AVAudioQueue_pushOwning(&obj->queues.notify, &notif)){
                NIX_PRINTF_ERROR("Nix_AVAudioSource_pendPopOldestBuffLocked_::Nix_AVAudioQueue_pushOwning(notify) failed.\n");
                notif.org = NULL;
                Nix_AVAudioQueuePair_destroy(&notif);
            } else {
                //now owned by reuse
                pair.org = NULL;
            }
        }
        NIX_ASSERT(pair.org == NULL); //program logic error
        NIX_ASSERT(pair.cnv == NULL); //program logic error
        Nix_AVAudioQueuePair_destroy(&pair);
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_AVAudioSource_pendPopAllBuffsLocked_(STNix_AVAudioSource* obj){
    NixBOOL r = NIX_TRUE;
    while(obj->queues.pend.use > 0){
        if(!Nix_AVAudioSource_pendPopOldestBuffLocked_(obj)){
            r = NIX_FALSE;
            break;
        }
    }
    return r;
}

//------
//Recorder
//------

void Nix_AVAudioRecorder_init(STNix_AVAudioRecorder* obj){
    memset(obj, 0, sizeof(*obj));
    //cfg
    {
        //
    }
    //queues
    {
        NIX_MUTEX_INIT(&obj->queues.mutex);
        Nix_AVAudioQueue_init(&obj->queues.notify);
        Nix_AVAudioQueue_init(&obj->queues.reuse);
    }
}

void Nix_AVAudioRecorder_destroy(STNix_AVAudioRecorder* obj){
    //queues
    {
        NIX_MUTEX_LOCK(&obj->queues.mutex);
        {
            //ToDo: remove all 'org' buffers manually
            Nix_AVAudioQueue_destroy(&obj->queues.notify);
            Nix_AVAudioQueue_destroy(&obj->queues.reuse);
        }
        NIX_MUTEX_UNLOCK(&obj->queues.mutex);
        NIX_MUTEX_DESTROY(&obj->queues.mutex);
    }
    //
    if(obj->eng != nil){
        AVAudioInputNode* input = [obj->eng inputNode];
        [input removeTapOnBus:0];
        [obj->eng stop];
        [obj->eng release];
        obj->eng = nil;
    }
}

void Nix_AVAudioRecorder_consumeInputBuffer_(STNix_AVAudioRecorder* obj, AVAudioPCMBuffer* buff){
    if(obj->queues.conv != NULL){
        NIX_MUTEX_LOCK(&obj->queues.mutex);
        {
            NixUI32 inIdx = 0;
            const NixUI32 inSz = [buff frameLength];
            while(inIdx < inSz){
                if(obj->queues.reuse.use <= 0){
                    //move oldest-notify buffer to reuse
                    if(!Nix_AVAudioQueue_popMovingTo(&obj->queues.notify, &obj->queues.reuse)){
                        //program logic error
                        NIX_ASSERT(NIX_FALSE);
                        break;
                    }
                } else {
                    STNix_AVAudioQueuePair* pair = &obj->queues.reuse.arr[0];
                    if(pair->org == NULL || pair->org->desc.blockAlign <= 0){
                        //just remove
                        STNix_AVAudioQueuePair pair;
                        if(!Nix_AVAudioQueue_popOrphaning(&obj->queues.reuse, &pair)){
                            NIX_ASSERT(NIX_FALSE);
                            //program logic error
                            break;
                        }
                        if(pair.org != NULL){
                            Nix_AVAudioPCMBuffer_destroy(pair.org);
                            NIX_FREE(pair.org);
                            pair.org = NULL;
                        }
                        Nix_AVAudioQueuePair_destroy(&pair);
                    } else {
                        const NixUI32 outSz = (pair->org->sz / pair->org->desc.blockAlign);
                        const NixUI32 outAvail = (obj->queues.filling.iCurSample >= outSz ? 0 : outSz - obj->queues.filling.iCurSample);
                        const NixUI32 inAvail = inSz - inIdx;
                        NixUI32 ammBlocksRead = 0, ammBlocksWritten = 0;
                        if(outAvail > 0 && inAvail > 0){
                            //convert
                            NixUI32 iCh;
                            const NixUI32 maxChannels = nixFmtConverter_maxChannels();
                            //dst
                            nixFmtConverter_setPtrAtDstInterlaced(obj->queues.conv, &pair->org->desc, pair->org->ptr, obj->queues.filling.iCurSample);
                            //src
                            AVAudioFormat* orgFmt = [ buff format];
                            NixBOOL isInterleavedSrc = [orgFmt isInterleaved];
                            NixUI32 chCountSrc = [orgFmt channelCount];
                            switch([orgFmt commonFormat]) {
                                case AVAudioPCMFormatFloat32:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 32;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        nixFmtConverter_setPtrAtSrcChannel(obj->queues.conv, iCh,  buff.floatChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                case AVAudioPCMFormatInt16:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 16;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        nixFmtConverter_setPtrAtSrcChannel(obj->queues.conv, iCh,  buff.int16ChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                case AVAudioPCMFormatInt32:
                                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                        const NixUI32 bitsPerSample = 32;
                                        const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                        nixFmtConverter_setPtrAtSrcChannel(obj->queues.conv, iCh,  buff.int32ChannelData[iCh] + inIdx, blockAlign);
                                    }
                                    break;
                                default:
                                    break;
                            }
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
                            if(!Nix_AVAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
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


NixBOOL Nix_AVAudioRecorder_prepare(STNix_AVAudioRecorder* obj, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer){
    NixBOOL r = NIX_FALSE;
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    if(obj->queues.conv == NULL && audioDesc->blockAlign > 0){
        obj->eng = [[AVAudioEngine alloc] init];
        {
            void* conv = nixFmtConverter_create();
            AVAudioInputNode* input = [obj->eng inputNode];
            AVAudioFormat* inFmt = [input inputFormatForBus:0];
            const NixUI32 inSampleRate = [inFmt sampleRate];
            STNix_audioDesc inDesc;
            nixFmtConverter_buffFmtToAudioDesc(inFmt, &inDesc);
            if(!nixFmtConverter_prepare(conv, &inDesc, audioDesc)){
                NIX_PRINTF_ERROR("Nix_AVAudioRecorder_prepare::nixFmtConverter_prepare failed.\n");
                nixFmtConverter_destroy(conv);
                conv = NULL;
            } else {
                //allocate reusable buffers
                while(obj->queues.reuse.use < buffersCount){
                    STNix_AVAudioQueuePair pair;
                    Nix_AVAudioQueuePair_init(&pair);
                    NIX_MALLOC(pair.org, STNix_AVAudioPCMBuffer, sizeof(STNix_AVAudioPCMBuffer), "Nix_AVAudioRecorder_prepare.pair.org");
                    if(pair.org == NULL){
                        NIX_PRINTF_ERROR("Nix_AVAudioRecorder_prepare::pair.org allocation failed.\n");
                        break;
                    } else {
                        Nix_AVAudioPCMBuffer_init(pair.org);
                        if(!Nix_AVAudioPCMBuffer_setData(pair.org, audioDesc, NULL, audioDesc->blockAlign * samplesPerBuffer)){
                            NIX_PRINTF_ERROR("Nix_AVAudioRecorder_prepare::Nix_AVAudioPCMBuffer_setData failed.\n");
                            Nix_AVAudioPCMBuffer_destroy(pair.org);
                            NIX_FREE(pair.org);
                            pair.org = NULL;
                            break;
                        } else {
                            Nix_AVAudioQueue_pushOwning(&obj->queues.reuse, &pair);
                        }
                    }
                }
                //
                if(obj->queues.reuse.use <= 0){
                    NIX_PRINTF_ERROR("Nix_AVAudioRecorder_prepare::no reusable buffer could be allocated.\n");
                } else {
                    r = NIX_TRUE;
                    //prepared
                    obj->queues.filling.iCurSample = 0;
                    obj->queues.conv = conv; conv = NULL; //consume
                    //cfg
                    obj->cfg.fmt = *audioDesc;
                    obj->cfg.maxBuffers = buffersCount;
                    obj->cfg.samplesPerBuffer = samplesPerBuffer;
                    //
                    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
                    //install tap (unlocked)
                    {
                        [input installTapOnBus:0 bufferSize:(inSampleRate / 30) format:inFmt block:^(AVAudioPCMBuffer * _Nonnull buffer, AVAudioTime * _Nonnull when) {
                            Nix_AVAudioRecorder_consumeInputBuffer_(obj, buffer);
                            //printf("AVFAudio recorder buffer with %d samples (%d samples in memory).\n", [buffer frameLength], obj->in.samples.cur);
                        }];
                    }
                    NIX_MUTEX_LOCK(&obj->queues.mutex);
                    [obj->eng prepare];
                }
            }
            //release (if not consumed)
            if(conv != NULL){
                nixFmtConverter_destroy(conv);
                conv = NULL;
            }
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    return r;
}

NixBOOL Nix_AVAudioRecorder_setCallback(STNix_AVAudioRecorder* obj, NixApiCaptureBufferFilledCallback callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    {
        obj->callback.func = callback;
        obj->callback.data = callbackData;
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL Nix_AVAudioRecorder_start(STNix_AVAudioRecorder* obj){
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

NixBOOL Nix_AVAudioRecorder_stop(STNix_AVAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    if(obj->eng != nil){
        [obj->eng stop];
        obj->engStarted = NIX_FALSE;
    }
    Nix_AVAudioRecorder_flush(obj);
    return r;
}

NixBOOL Nix_AVAudioRecorder_flush(STNix_AVAudioRecorder* obj){
    NixBOOL r = NIX_TRUE;
    //move filling buffer to notify (if data is available)
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    if(obj->queues.reuse.use > 0){
        STNix_AVAudioQueuePair* pair = &obj->queues.reuse.arr[0];
        if(pair->org != NULL && pair->org->use > 0){
            obj->queues.filling.iCurSample = 0;
            if(!Nix_AVAudioQueue_popMovingTo(&obj->queues.reuse, &obj->queues.notify)){
                //program logic error
                r = NIX_FALSE;
            }
        }
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    return r;
}

void Nix_AVAudioRecorder_notifyBuffers(STNix_AVAudioRecorder* obj){
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    {
        const NixUI32 maxProcess = obj->queues.notify.use;
        NixUI32 ammProcessed = 0;
        while(ammProcessed < maxProcess && obj->queues.notify.use > 0){
            STNix_AVAudioQueuePair pair;
            if(!Nix_AVAudioQueue_popOrphaning(&obj->queues.notify, &pair)){
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
                if(!Nix_AVAudioQueue_pushOwning(&obj->queues.reuse, &pair)){
                    //program logic error
                    NIX_ASSERT(NIX_FALSE);
                    if(pair.org != NULL){
                        Nix_AVAudioPCMBuffer_destroy(pair.org);
                        NIX_FREE(pair.org);
                        pair.org = NULL;
                    }
                    Nix_AVAudioQueuePair_destroy(&pair);
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

STNixApiEngine nixAVAudioEngine_create(void){
    STNixApiEngine r = STNixApiEngine_Zero;
    STNix_AVAudioEngine* obj = NULL;
    NIX_MALLOC(obj, STNix_AVAudioEngine, sizeof(STNix_AVAudioEngine), "STNix_AVAudioEngine");
    if(obj != NULL){
        Nix_AVAudioEngine_init(obj);
        r.opq = obj;
    }
    return r;
}

void nixAVAudioEngine_destroy(STNixApiEngine pObj){
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj.opq;
    if(obj != NULL){
        Nix_AVAudioEngine_destroy(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}

void nixAVAudioEngine_printCaps(STNixApiEngine pObj){
    //
}

NixBOOL nixAVAudioEngine_ctxIsActive(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj.opq;
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioEngine_ctxActivate(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj.opq;
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioEngine_ctxDeactivate(STNixApiEngine pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj.opq;
    if(obj != NULL){
        r = NIX_TRUE;
    }
    return r;
}

void nixAVAudioEngine_tick(STNixApiEngine pObj){
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj.opq;
    if(obj != NULL){
        Nix_AVAudioEngine_tick(obj, NIX_FALSE);
    }
}


//------
//PCMBuffer API
//------
       
STNixApiBuffer nixAVAudioPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    STNixApiBuffer r = STNixApiBuffer_Zero;
    if(audioDesc != NULL && audioDesc->blockAlign > 0){
        STNix_AVAudioPCMBuffer* obj = NULL;
        NIX_MALLOC(obj, STNix_AVAudioPCMBuffer, sizeof(STNix_AVAudioPCMBuffer), "STNix_AVAudioPCMBuffer");
        if(obj != NULL){
            Nix_AVAudioPCMBuffer_init(obj);
            if(!Nix_AVAudioPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes)){
                NIX_PRINTF_ERROR("nixAVAudioPCMBuffer_create::Nix_AVAudioPCMBuffer_setData failed.\n");
                Nix_AVAudioPCMBuffer_destroy(obj);
                NIX_FREE(obj);
                obj = NULL;
            }
            r.opq = obj;
        }
    }
    return r;
}

void nixAVAudioPCMBuffer_destroy(STNixApiBuffer pObj){
    if(pObj.opq != NULL){
        STNix_AVAudioPCMBuffer* obj = (STNix_AVAudioPCMBuffer*)pObj.opq;
        Nix_AVAudioPCMBuffer_destroy(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}
   
NixBOOL nixAVAudioPCMBuffer_setData(STNixApiBuffer pObj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && audioDesc != NULL && audioDesc->blockAlign > 0){
        STNix_AVAudioPCMBuffer* obj = (STNix_AVAudioPCMBuffer*)pObj.opq;
        r = Nix_AVAudioPCMBuffer_setData(obj, audioDesc, audioDataPCM, audioDataPCMBytes);
    }
    return r;
}

NixBOOL nixAVAudioPCMBuffer_fillWithZeroes(STNixApiBuffer pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_AVAudioPCMBuffer* obj = (STNix_AVAudioPCMBuffer*)pObj.opq;
        r = Nix_AVAudioPCMBuffer_fillWithZeroes(obj);
    }
    return r;
}


//------
//Source API
//------
  
STNixApiSource nixAVAudioSource_create(STNixApiEngine pEng){
    STNixApiSource r = STNixApiSource_Zero;
    STNix_AVAudioEngine* eng = (STNix_AVAudioEngine*)pEng.opq;
    if(eng != NULL){
        STNix_AVAudioSource* obj = NULL;
        NIX_MALLOC(obj, STNix_AVAudioSource, sizeof(STNix_AVAudioSource), "STNix_AVAudioSource");
        if(obj != NULL){
            memset(obj, 0, sizeof(STNix_AVAudioSource));
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
                        NIX_FREE(obj);
                        obj = NULL;
                    } else {
                        obj->engStarted = NIX_TRUE;
                    }
                }
            }
        }
        //add to engine
        if(!Nix_AVAudioEngine_srcsAdd(eng, obj)){
            NIX_PRINTF_ERROR("nixAVAudioSource_create::Nix_AVAudioEngine_srcsAdd failed.\n");
            Nix_AVAudioSource_release(obj);
            NIX_FREE(obj);
            obj = NULL;
        }
        r.opq = obj;
    }
    return r;
}

void nixAVAudioSource_removeAllBuffersAndNotify_(STNix_AVAudioSource* obj){
    STNix_AVAudioNotifQueue notifs;
    Nix_AVAudioNotifQueue_init(&notifs);
    //move all pending buffers to notify
    NIX_MUTEX_LOCK(&obj->queues.mutex);
    {
        Nix_AVAudioSource_pendPopAllBuffsLocked_(obj);
        Nix_AVAudioEngine_tick_addQueueNotifSrcLocked_(&notifs, obj);
    }
    NIX_MUTEX_UNLOCK(&obj->queues.mutex);
    //notify
    {
        NixUI32 i;
        for(i = 0; i < notifs.use; ++i){
            STNix_AVAudioSrcNotif* n = &notifs.arr[i];
            NIX_PRINTF_INFO("nixAVAudioSource_removeAllBuffersAndNotify_::notify(#%d/%d).\n", i + 1, notifs.use);
            if(n->callback.func != NULL){
                (*n->callback.func)(n->callback.eng, n->callback.sourceIndex, n->ammBuffs);
            }
        }
    }
}

void nixAVAudioSource_destroy(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        //flush all pending buffers
        nixAVAudioSource_removeAllBuffersAndNotify_(obj);
        //
        Nix_AVAudioSource_release(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}

void nixAVAudioSource_setCallback(STNixApiSource pObj, void (*callback)(void* pEng, const NixUI32 sourceIndex, const NixUI32 ammBuffs), void* callbackEng, NixUI32 callbackSourceIndex){
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        obj->queues.callback.func  = callback;
        obj->queues.callback.eng   = callbackEng;
        obj->queues.callback.sourceIndex = callbackSourceIndex;
    }
}

NixBOOL nixAVAudioSource_setVolume(STNixApiSource pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        [obj->src setVolume:(vol < 0.f ? 0.f : vol > 1.f ? 1.f : vol)];
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioSource_setRepeat(STNixApiSource pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        obj->isRepeat = isRepeat;
        r = NIX_TRUE;
    }
    return r;
}

   
void nixAVAudioSource_play(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        //
        obj->isPlaying = NIX_TRUE;
        obj->isPaused = NIX_FALSE;
        Nix_AVAudioSource_scheduleEnqueuedBuffers(obj);
        [obj->src play];
    }
}

void nixAVAudioSource_pause(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        [obj->src pause];
        obj->isPaused = NIX_TRUE;
    }
}

void nixAVAudioSource_stop(STNixApiSource pObj){
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        obj->isPlaying = NIX_FALSE;
        obj->isPaused = NIX_FALSE;
        [obj->src stop];
        //flush all pending buffers
        nixAVAudioSource_removeAllBuffersAndNotify_(obj);
    }
}

NixBOOL nixAVAudioSource_isPlaying(STNixApiSource pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        r = (obj->src != nil && [obj->src isPlaying] && obj->queues.pendScheduledCount > 0) ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

NixBOOL nixAVAudioSource_isPaused(STNixApiSource pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj.opq;
        r = obj->isPlaying && obj->isPaused ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

void* nixAVAudioSource_createConverter(const STNix_audioDesc* srcFmt, AVAudioFormat* outFmt){
    void* r = NULL;
    STNix_audioDesc outDesc;
    memset(&outDesc, 0, sizeof(outDesc));
    outDesc.samplerate  = [outFmt sampleRate];
    outDesc.channels    = [outFmt channelCount];
    switch([outFmt commonFormat]) {
        case AVAudioPCMFormatFloat32:
            outDesc.bitsPerSample   = 32;
            outDesc.samplesFormat   = ENNix_sampleFormat_float;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        case AVAudioPCMFormatInt16:
            outDesc.bitsPerSample   = 16;
            outDesc.samplesFormat   = ENNix_sampleFormat_int;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        case AVAudioPCMFormatInt32:
            outDesc.bitsPerSample   = 32;
            outDesc.samplesFormat   = ENNix_sampleFormat_int;
            outDesc.blockAlign      = (outDesc.bitsPerSample / 8) * ([outFmt isInterleaved] ? outDesc.channels : 1);
            break;
        default:
            break;
    }
    if(outDesc.bitsPerSample > 0){
        r = nixFmtConverter_create();
        if(!nixFmtConverter_prepare(r, srcFmt, &outDesc)){
            nixFmtConverter_destroy(r);
            r = NULL;
        }
    }
    return r;
}

NixBOOL nixAVAudioSource_setBuffer(STNixApiSource pObj, STNixApiBuffer pBuff){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && pBuff.opq != NULL){
        STNix_AVAudioSource* obj    = (STNix_AVAudioSource*)pObj.opq;
        STNix_AVAudioPCMBuffer* buff = (STNix_AVAudioPCMBuffer*)pBuff.opq;
        if(obj->queues.conv != NULL || obj->buffsFmt.blockAlign > 0){
            //error, buffer already set
        } else {
            AVAudioOutputNode* outNode  = [obj->eng outputNode];
            AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
            obj->queues.conv = nixAVAudioSource_createConverter(&buff->desc, outFmt);
            if(obj->queues.conv == NULL){
                NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, nixAVAudioSource_createConverter failed.\n");
            } else {
                //set format
                obj->buffsFmt = buff->desc;
                obj->isStatic = NIX_TRUE;
                if(!Nix_AVAudioSource_queueBufferForOutput(obj, buff)){
                    NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, Nix_AVAudioSource_queueBufferForOutput failed.\n");
                } else {
                    //enqueue
                    if(obj->isPlaying && !obj->isPaused){
                        Nix_AVAudioSource_scheduleEnqueuedBuffers(obj);
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
NixBOOL nixAVAudioSource_queueBuffer(STNixApiSource pObj, STNixApiBuffer pBuff) {
    NixBOOL r = NIX_FALSE;
    if(pObj.opq != NULL && pBuff.opq != NULL){
        STNix_AVAudioSource* obj    = (STNix_AVAudioSource*)pObj.opq;
        STNix_AVAudioPCMBuffer* buff = (STNix_AVAudioPCMBuffer*)pBuff.opq;
        //define format
        if(obj->queues.conv == NULL && obj->buffsFmt.blockAlign <= 0){
            //first buffer, define as format
            AVAudioOutputNode* outNode  = [obj->eng outputNode];
            AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
            obj->queues.conv = nixAVAudioSource_createConverter(&buff->desc, outFmt);
            if(obj->queues.conv == NULL){
                //error, converter creation failed
            } else {
                //set format
                obj->buffsFmt = buff->desc;
            }
        }
        //queue buffer
        if(!STNix_audioDesc_IsEqual(&obj->buffsFmt, &buff->desc)){
            NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, new buffer doesnt match first buffer's format.\n");
        } else if(!Nix_AVAudioSource_queueBufferForOutput(obj, buff)){
            NIX_PRINTF_ERROR("nixAVAudioSource_queueBuffer, Nix_AVAudioSource_queueBufferForOutput failed.\n");
        } else {
            //enqueue
            if(obj->isPlaying && !obj->isPaused){
                Nix_AVAudioSource_scheduleEnqueuedBuffers(obj);
            }
            r = NIX_TRUE;
        }
    }
    return r;
}

//------
//nixFmtConverter API
//------

void nixFmtConverter_buffFmtToAudioDesc(AVAudioFormat* buffFmt, STNix_audioDesc* dst){
    if(buffFmt != nil && dst != NULL){
        memset(dst, 0, sizeof(*dst));
        dst->samplerate  = [buffFmt sampleRate];
        dst->channels    = [buffFmt channelCount];
        switch([buffFmt commonFormat]) {
            case AVAudioPCMFormatFloat32:
                dst->bitsPerSample   = 32;
                dst->samplesFormat   = ENNix_sampleFormat_float;
                dst->blockAlign      = (dst->bitsPerSample / 8) * ([buffFmt isInterleaved] ? dst->channels : 1);
                break;
            case AVAudioPCMFormatInt16:
                dst->bitsPerSample   = 16;
                dst->samplesFormat   = ENNix_sampleFormat_int;
                dst->blockAlign      = (dst->bitsPerSample / 8) * ([buffFmt isInterleaved] ? dst->channels : 1);
                break;
            case AVAudioPCMFormatInt32:
                dst->bitsPerSample   = 32;
                dst->samplesFormat   = ENNix_sampleFormat_int;
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

STNixApiRecorder nixAVAudioRecorder_create(STNixApiEngine pEng, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer){
    STNixApiRecorder r = STNixApiRecorder_Zero;
    STNix_AVAudioEngine* eng = (STNix_AVAudioEngine*)pEng.opq;
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0 && audioDesc->blockAlign > 0 && eng->rec == NULL){
        STNix_AVAudioRecorder* obj = NULL;
        NIX_MALLOC(obj, STNix_AVAudioRecorder, sizeof(STNix_AVAudioRecorder), "STNix_AVAudioRecorder");
        if(obj != NULL){
            Nix_AVAudioRecorder_init(obj);
            if(!Nix_AVAudioRecorder_prepare(obj, audioDesc, buffersCount, samplesPerBuffer)){
                NIX_PRINTF_ERROR("nixAVAudioRecorder_create, Nix_AVAudioRecorder_prepare failed.\n");
                Nix_AVAudioRecorder_destroy(obj);
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

void nixAVAudioRecorder_destroy(STNixApiRecorder pObj){
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj.opq;
    if(obj != NULL){
        if(obj->engNx != NULL && obj->engNx->rec == obj){
            obj->engNx->rec = NULL;
        }
        Nix_AVAudioRecorder_destroy(obj);
        NIX_FREE(obj);
        obj = NULL;
    }
}

NixBOOL nixAVAudioRecorder_setCallback(STNixApiRecorder pObj, NixApiCaptureBufferFilledCallback callback, void* callbackData){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_AVAudioRecorder_setCallback(obj, callback, callbackData);
    }
    return r;
}

NixBOOL nixAVAudioRecorder_start(STNixApiRecorder pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_AVAudioRecorder_start(obj);
    }
    return r;
}

NixBOOL nixAVAudioRecorder_stop(STNixApiRecorder pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj.opq;
    if(obj != NULL){
        r = Nix_AVAudioRecorder_stop(obj);
    }
    return r;
}



