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
//This file add support for AVFAudio for playing and recording in MacOS and iOS.
//

#include "nixtla-audio-private.h"
#include "nixtla-audio.h"
//
#include <AVFAudio/AVAudioEngine.h>
#include <AVFAudio/AVAudioPlayerNode.h>
#include <AVFAudio/AVAudioMixerNode.h>
#include <AVFAudio/AVAudioFormat.h>
#include <AVFAudio/AVAudioChannelLayout.h>
#include <AVFAudio/AVAudioConverter.h>

//nixFmtConverter

void nixFmtConverter_buffFmtToAudioDesc(AVAudioFormat* buffFmt, STNix_audioDesc* dst);

//engine

typedef struct STNix_AVAudioEngine_ {
    unsigned int dumy;  //just a dumy member
} STNix_AVAudioEngine;

void* nixAVAudioEngine_create(void){
    STNix_AVAudioEngine* obj = NULL;
    NIX_MALLOC(obj, STNix_AVAudioEngine, sizeof(STNix_AVAudioEngine), "STNix_AVAudioEngine");
    memset(obj, 0, sizeof(STNix_AVAudioEngine));
    return obj;
}

void nixAVAudioEngine_destroy(void* pObj){
    STNix_AVAudioEngine* obj = (STNix_AVAudioEngine*)pObj;
    if(obj != NULL){
        NIX_FREE(obj);
        obj = NULL;
    }
}

//

typedef struct STNix_AVAudioRecorder_ {
    NixBOOL                 engStarted;
    AVAudioEngine*          eng;    //AVAudioEngine
    //out
    struct {
        STNix_audioDesc     fmt;
        NixUI16             samplesPerBuffer;
        //conv
        struct {
            void*           obj;  //nixFmtConverter
            //src
            struct {
                AVAudioPCMBuffer* buff; //current buffer converting from format
                NixUI32     curSample;  //cur sample converting
            } src;
            //dst
            struct {
                NixBYTE*    buff;       //current buffer converting to format
                NixUI32     buffSz;     //cur sample converting
                NixUI32     curSample;  //cur sample converting
            } dst;
        } conv;
    } out;
    //in
    struct {
        NSLock*              lock;
        //buffs
        struct {
            AVAudioPCMBuffer** arr;
            NixUI32         use;
            NixUI32         sz;
        } buffs;
        //samples
        struct {
            NixUI32         cur;    //currently in buffer
            NixUI32         max;    //bassed on out.audioDesc, out.buffersCount, out.samplesPerBuffer
        } samples;
    } in;
} STNix_AVAudioRecorder;

void nixAVAudioRecorder_inBuffsClearLocked_(STNix_AVAudioRecorder* obj){
    if(obj->in.buffs.arr != NULL){
        NixUI32 i; for(i = 0; i < obj->in.buffs.use; i++){
            AVAudioPCMBuffer* b = obj->in.buffs.arr[i];
            if(b != nil) [b release];
            b = nil;
        }
        obj->in.buffs.use = 0;
    }
}

NixBOOL nixAVAudioRecorder_inBuffsPopOldestLocked_(STNix_AVAudioRecorder* obj, AVAudioPCMBuffer** dst){
    NixBOOL r = NIX_FALSE;
    if(obj->in.buffs.use > 0){
        AVAudioPCMBuffer* b = obj->in.buffs.arr[0];
        obj->in.samples.cur -= [b frameLength];
        obj->in.buffs.use--;
        *dst = b;
        //fill gap
        {
            NixUI32 i; for(i = 0; i < obj->in.buffs.use; i++){
                obj->in.buffs.arr[i] = obj->in.buffs.arr[i + 1];
            }
        }
        r = NIX_TRUE;
    }
    return r;
}
    
void nixAVAudioRecorder_inBuffsPushNewestLocked_(STNix_AVAudioRecorder* obj, AVAudioPCMBuffer* b){
    if(b != nil && [b frameLength] > 0){
        if(obj->in.samples.cur >= obj->in.samples.max){
            //pop oldest first
            AVAudioPCMBuffer* oldest = nil;
            if(nixAVAudioRecorder_inBuffsPopOldestLocked_(obj, &oldest)){
                if(oldest != nil){
                    [oldest release];
                    oldest = nil;
                }
            }
        }
        //push new buffer
        if(obj->in.buffs.use >= obj->in.buffs.sz){
            AVAudioPCMBuffer** arr = NULL;
            const NixUI32 sz = obj->in.buffs.use + 4;
            NIX_MALLOC(arr, AVAudioPCMBuffer*, sizeof(AVAudioPCMBuffer*) * sz, "arr");
            if(obj->in.buffs.arr != NULL){
                if(obj->in.buffs.use > 0){
                    memcpy(arr, obj->in.buffs.arr, sizeof(AVAudioPCMBuffer*) * obj->in.buffs.use);
                }
                NIX_FREE(obj->in.buffs.arr);
                obj->in.buffs.arr = NULL;
            }
            obj->in.buffs.arr = arr;
            obj->in.buffs.sz = sz;
        }
        obj->in.buffs.arr[obj->in.buffs.use] = b; [b retain];
        obj->in.buffs.use++;
        obj->in.samples.cur += [b frameLength];
    }
}

void* nixAVAudioRecorder_create(void* pEng, const STNix_audioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 samplesPerBuffer){
    STNix_AVAudioRecorder* obj = NULL;
    STNix_AVAudioEngine* eng = (STNix_AVAudioEngine*)pEng;
    if(eng != NULL && audioDesc != NULL && audioDesc->samplerate > 0){
        NIX_MALLOC(obj, STNix_AVAudioRecorder, sizeof(STNix_AVAudioRecorder), "STNix_AVAudioRecorder");
        memset(obj, 0, sizeof(STNix_AVAudioRecorder));
        obj->eng = [[AVAudioEngine alloc] init];
        obj->in.lock = [[NSLock alloc] init];
        {
            AVAudioInputNode* input = [obj->eng inputNode];
            AVAudioFormat* inFmt = [input inputFormatForBus:0];
            const NixUI32 inSampleRate = [inFmt sampleRate];
            [input installTapOnBus:0 bufferSize:(inSampleRate / 30) format:inFmt block:^(AVAudioPCMBuffer * _Nonnull buffer, AVAudioTime * _Nonnull when) {
                AVAudioPCMBuffer* cpy = [buffer copy];
                [obj->in.lock lock];
                {
                    nixAVAudioRecorder_inBuffsPushNewestLocked_(obj, cpy);
                    //printf("AVFAudio recorder buffer with %d samples (%d samples in memory).\n", [buffer frameLength], obj->in.samples.cur);
                }
                [obj->in.lock unlock];
                [cpy release];
            }];
            [obj->eng prepare];
            //calculate buffers to keep
            obj->out.fmt = *audioDesc;
            obj->out.samplesPerBuffer = samplesPerBuffer;
            {
                const NixUI32 outSamplesMax = buffersCount * samplesPerBuffer;
                obj->in.samples.max = outSamplesMax * inSampleRate / audioDesc->samplerate;
            }
            //create notification buffer
            if(samplesPerBuffer > 0){
                obj->out.conv.dst.curSample = 0;
                obj->out.conv.dst.buffSz = samplesPerBuffer * (audioDesc->bitsPerSample / 8) * audioDesc->channels;
                NIX_MALLOC(obj->out.conv.dst.buff, NixBYTE, obj->out.conv.dst.buffSz, "bj->out.conv.dst.buff");
            }
        }
    }
    return obj;
}

void nixAVAudioRecorder_destroy(void* pObj){
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj;
    if(obj != NULL){
        //in
        {
            [obj->in.lock lock];
            if(obj->in.buffs.arr != NULL){
                nixAVAudioRecorder_inBuffsClearLocked_(obj);
                //
                NIX_FREE(obj->in.buffs.arr);
                obj->in.buffs.arr = NULL;
                obj->in.buffs.sz = 0;
            }
            [obj->in.lock unlock];
        }
        //out
        {
            obj->out.conv.src.curSample = 0;
            if(obj->out.conv.src.buff != nil){
                [obj->out.conv.src.buff release];
                obj->out.conv.src.buff = nil;
            }
            obj->out.conv.dst.curSample = 0;
            obj->out.conv.dst.buffSz = 0;
            if(obj->out.conv.dst.buff != NULL){
                NIX_FREE(obj->out.conv.dst.buff);
                obj->out.conv.dst.buff = NULL;
            }
            if(obj->out.conv.obj != NULL) {
                nixFmtConverter_destroy(obj->out.conv.obj);
                obj->out.conv.obj = NULL;
            }
        }
        if(obj->eng != nil){
            AVAudioInputNode* input = [obj->eng inputNode];
            [input removeTapOnBus:0];
            [obj->eng stop];
            [obj->eng release];
            obj->eng = nil;
        }
    }
}

NixBOOL nixAVAudioRecorder_start(void* pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj;
    if(obj != NULL){
        r = NIX_TRUE;
        if(!obj->engStarted){
            NSError* err = nil;
            if(![obj->eng startAndReturnError:&err]){
                NIX_PRINTF_ERROR("nixAVAudioRecorder_create, AVAudioEngine::startAndReturnError failed: '%s'.\n", err == nil ? "unknown" : [[err description] UTF8String]);
                r = NIX_FALSE;
            } else {
                obj->engStarted = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixAVAudioRecorder_stop(void* pObj){
    NixBOOL r = NIX_FALSE;
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj;
    if(obj != NULL){
        if(obj->eng != nil){
            [obj->eng stop];
            obj->engStarted = NIX_FALSE;
            //in
            {
                [obj->in.lock lock];
                if(obj->in.buffs.arr != NULL){
                    nixAVAudioRecorder_inBuffsClearLocked_(obj);
                }
                [obj->in.lock unlock];
            }
            //out
            {
                obj->out.conv.src.curSample = 0;
                if(obj->out.conv.src.buff != nil){
                    [obj->out.conv.src.buff release];
                    obj->out.conv.src.buff = nil;
                }
                obj->out.conv.dst.curSample = 0;
            }
        }
        r = NIX_FALSE;
    }
    return r;
}

void nixAVAudioRecorder_notifyBuffers(void* pObj, STNix_Engine* engAbs, PTRNIX_CaptureBufferFilledCallback bufferCaptureCallback, void* bufferCaptureCallbackUserData){
    STNix_AVAudioRecorder* obj = (STNix_AVAudioRecorder*)pObj;
    if(obj != NULL){
        //create converter
        if(obj->out.conv.obj == NULL){
            AVAudioInputNode* input = [obj->eng inputNode];
            AVAudioFormat* inFmt = [input inputFormatForBus:0];
            obj->out.conv.obj = nixFmtConverter_create();
            //
            STNix_audioDesc inDesc;
            nixFmtConverter_buffFmtToAudioDesc(inFmt, &inDesc);
            if(!nixFmtConverter_prepare(obj->out.conv.obj, &inDesc, &obj->out.fmt)){
                nixFmtConverter_destroy(obj->out.conv.obj);
                obj->out.conv.obj = NULL;
            }
        }
        //convert
        if(obj->out.conv.obj != NULL && obj->out.conv.dst.buff != NULL){
            NixUI32 iBuff = 0, buffsSrcMax = 0;
            [obj->in.lock lock];
            {
                //this count incudes the currently pending buffer
                buffsSrcMax = (obj->out.conv.src.buff != nil ? 1 : 0) + obj->in.buffs.use;
            }
            [obj->in.lock unlock];
            //
            while(iBuff < buffsSrcMax){
                //get buffer
                if(obj->out.conv.src.buff == nil){
                    //pop from queue
                    [obj->in.lock lock];
                    {
                        AVAudioPCMBuffer* buff = nil;
                        nixAVAudioRecorder_inBuffsPopOldestLocked_(obj, &buff);
                        if(buff != nil){
                            obj->out.conv.src.buff = buff;
                            obj->out.conv.src.curSample = 0;
                        }
                    }
                    [obj->in.lock unlock];
                }
                //no new buffer
                if(obj->out.conv.src.buff == nil){
                    break;
                }
                //configure converter
                {
                    NixUI32 iCh;
                    const NixUI32 maxChannels = nixFmtConverter_maxChannels();
                    //dst
                    nixFmtConverter_setPtrAtDstInterlaced(obj->out.conv.obj, &obj->out.fmt, obj->out.conv.dst.buff, obj->out.conv.dst.curSample);
                    //src
                    AVAudioFormat* orgFmt = [obj->out.conv.src.buff format];
                    NixBOOL isInterleavedSrc = [orgFmt isInterleaved];
                    NixUI32 chCountSrc = [orgFmt channelCount];
                    switch([orgFmt commonFormat]) {
                        case AVAudioPCMFormatFloat32:
                            for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                const NixUI32 bitsPerSample = 32;
                                const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                nixFmtConverter_setPtrAtSrcChannel(obj->out.conv.obj, iCh, obj->out.conv.src.buff.floatChannelData[iCh] + obj->out.conv.src.curSample, blockAlign);
                            }
                            break;
                        case AVAudioPCMFormatInt16:
                            for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                const NixUI32 bitsPerSample = 16;
                                const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                nixFmtConverter_setPtrAtSrcChannel(obj->out.conv.obj, iCh, obj->out.conv.src.buff.int16ChannelData[iCh] + obj->out.conv.src.curSample, blockAlign);
                            }
                            break;
                        case AVAudioPCMFormatInt32:
                            for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                                const NixUI32 bitsPerSample = 32;
                                const NixUI32 blockAlign = (bitsPerSample / 8) * (isInterleavedSrc ? chCountSrc : 1);
                                nixFmtConverter_setPtrAtSrcChannel(obj->out.conv.obj, iCh, obj->out.conv.src.buff.int32ChannelData[iCh] + obj->out.conv.src.curSample, blockAlign);
                            }
                            break;
                        default:
                            break;
                    }
                    //convert
                    const NixUI32 ammBlockToRead = (obj->out.conv.dst.curSample > obj->out.samplesPerBuffer ? 0 : obj->out.samplesPerBuffer - obj->out.conv.dst.curSample);
                    const NixUI32 ammBlockForWrite = (obj->out.conv.src.curSample > [obj->out.conv.src.buff frameLength] ? 0 : [obj->out.conv.src.buff frameLength] - obj->out.conv.src.curSample);
                    NixUI32 ammBlocksRead = 0, ammBlocksWritten = 0;
                    if(!nixFmtConverter_convert(obj->out.conv.obj, ammBlockToRead, ammBlockForWrite, &ammBlocksRead, &ammBlocksWritten)){
                        //error
                        break;
                    } else if(ammBlocksRead == 0 && ammBlocksWritten == 0){
                        //converter did nothing, avoid infinite cycle
                        break;
                    } else {
                        //converted
                        {
                            obj->out.conv.src.curSample += ammBlocksRead;
                            if(obj->out.conv.src.curSample >= [obj->out.conv.src.buff frameLength]){
                                //release src buffer
                                [obj->out.conv.src.buff release];
                                obj->out.conv.src.buff = nil;
                                obj->out.conv.src.curSample = 0;
                                iBuff++;
                            }
                        }
                        //notify
                        {
                            obj->out.conv.dst.curSample += ammBlocksWritten;
                            if(obj->out.conv.dst.curSample >= obj->out.samplesPerBuffer){
                                if(bufferCaptureCallback != NULL){
                                    (*bufferCaptureCallback)(engAbs, bufferCaptureCallbackUserData, obj->out.fmt, (const NixUI8*)obj->out.conv.dst.buff, (obj->out.conv.dst.curSample * obj->out.fmt.blockAlign), obj->out.conv.dst.curSample);
                                }
                                obj->out.conv.dst.curSample = 0;
                            }
                        }
                    }
                }
            }
        }
        
    }
}

//

typedef struct STNix_AVAudioSourceCallback_ {
    void            (*func)(void* pEng, NixUI32 sourceIndex);
    void*           eng;
    NixUI32         sourceIndex;
} STNix_AVAudioSourceCallback;
    
typedef struct STNix_AVAudioSource_ {
    AVAudioFormat*          fmt;    //AVAudioFormat
    AVAudioPlayerNode*      src;    //AVAudioPlayerNode
    AVAudioEngine*          eng;    //AVAudioEngine
    void*                   conv;  //nixFmtConverter
    //owned
    struct {
        AVAudioPCMBuffer*   buffStatic; //nil for streams, not-nil for static-sources
        NixBOOL             isQueued;
        STNix_AVAudioSourceCallback callback;
    } owned;
    //pcm
    struct {
        STNix_audioDesc     desc;
    } pcm;
    //packed bools to reduce padding
    NixBOOL                 engStarted;
    NixBOOL                 isRepeat;
    NixBOOL                 isPlaying;
    NixBOOL                 isPaused;
} STNix_AVAudioSource;

void nixAVAudioSource_ownBuffStaticStop_(STNix_AVAudioSource* obj);
void nixAVAudioSource_ownBuffStaticSchedule_(STNix_AVAudioSource* obj);
    
void* nixAVAudioSource_create(void* pEng){
    STNix_AVAudioSource* obj = NULL;
    STNix_AVAudioEngine* eng = (STNix_AVAudioEngine*)pEng;
    if(eng != NULL){
        NIX_MALLOC(obj, STNix_AVAudioSource, sizeof(STNix_AVAudioSource), "STNix_AVAudioSource");
        memset(obj, 0, sizeof(STNix_AVAudioSource));
        obj->eng = [[AVAudioEngine alloc] init];
        obj->src = [[AVAudioPlayerNode alloc] init];
        {
            //attach
            [obj->eng attachNode:obj->src];
            //connect
            [obj->eng connect:obj->src to:[obj->eng outputNode] format:obj->fmt];
            //
            if(!obj->engStarted){
                NSError* err = nil;
                [obj->eng prepare];
                if(![obj->eng startAndReturnError:&err]){
                    NIX_PRINTF_ERROR("nixAVAudioSource_create, AVAudioEngine::startAndReturnError failed: '%s'.\n", err == nil ? "unknown" : [[err description] UTF8String]);
                } else {
                    obj->engStarted = NIX_TRUE;
                }
            }
        }
    }
    return obj;
}

void nixAVAudioSource_destroy(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        if(obj->conv != NULL){ nixFmtConverter_destroy(obj->conv); obj->conv = NULL; }
        if(obj->src != nil){ [obj->src release]; obj->src = nil; }
        if(obj->fmt != nil){ [obj->fmt release]; obj->fmt = nil; }
        if(obj->eng != nil){ [obj->eng release]; obj->eng = nil; }
        if(obj->owned.buffStatic != nil){ [obj->owned.buffStatic  release]; obj->owned.buffStatic = nil; }
        NIX_FREE(obj);
        obj = NULL;
    }
}

NixBOOL nixAVAudioSource_setVolume(void* pObj, const float vol){
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        [obj->src setVolume:(vol < 0.f ? 0.f : vol > 1.f ? 1.f : vol)];
        r = NIX_TRUE;
    }
    return r;
}

NixBOOL nixAVAudioSource_setRepeat(void* pObj, const NixBOOL isRepeat){
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        obj->isRepeat = isRepeat;
        r = NIX_TRUE;
    }
    return r;
}

void nixAVAudioSource_play(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        [obj->src play];
        obj->isPlaying = NIX_TRUE;
        obj->isPaused = NIX_FALSE;
        if(obj->owned.buffStatic != nil && !obj->owned.isQueued){
            nixAVAudioSource_ownBuffStaticSchedule_(obj);
        }
    }
}

void nixAVAudioSource_pause(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        [obj->src pause];
        obj->isPaused = NIX_TRUE;
    }
}

void nixAVAudioSource_stop(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        obj->isPlaying = NIX_FALSE;
        obj->isPaused = NIX_FALSE;
        //callback (player was stopped)
        nixAVAudioSource_ownBuffStaticStop_(obj);
        //
        [obj->src stop];
    }
}

NixBOOL nixAVAudioSource_isPlaying(void* pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        if(obj->owned.buffStatic != nil){
            //static source
            r = [obj->src isPlaying] && obj->owned.isQueued? NIX_TRUE : NIX_FALSE;
        } else {
            //stream source
            r = [obj->src isPlaying] ? NIX_TRUE : NIX_FALSE;
        }
    }
    return r;
}

NixBOOL nixAVAudioSource_isPaused(void* pObj){
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        r = obj->isPlaying && obj->isPaused ? NIX_TRUE : NIX_FALSE;
    }
    return r;
}

typedef struct STNix_AVAudioPCMBuffer_ {
    AVAudioFormat*      fmt;     //AVAudioFormat
    struct {
        AVAudioPCMBuffer* org;  //AVAudioPCMBuffer, original
        AVAudioPCMBuffer* conv; //AVAudioPCMBuffer, converted
    } buff;
    //pcm
    struct {
        STNix_audioDesc desc;
    } pcm;
} STNix_AVAudioPCMBuffer;

void nixAVAudioSource_ownBuffStaticSchedule_(STNix_AVAudioSource* obj){
    if(obj->owned.buffStatic == nil){
        obj->owned.isQueued = NIX_FALSE;
    } else {
        obj->owned.isQueued = NIX_TRUE;
        [obj->src scheduleBuffer:obj->owned.buffStatic completionHandler:^(void) {
            if(obj->isPlaying){
                if(!obj->isPaused && obj->isRepeat){
                    //queue again
                    nixAVAudioSource_ownBuffStaticSchedule_(obj);
                } else {
                    obj->owned.isQueued = NIX_FALSE;
                }
            } else {
                //callback (player was stopped)
                nixAVAudioSource_ownBuffStaticStop_(obj);
            }
        }];
    }
}

void nixAVAudioSource_ownBuffStaticStop_(STNix_AVAudioSource* obj){
    //callback (player was stopped)
    STNix_AVAudioSourceCallback callback = obj->owned.callback;
    {
        if(obj->owned.buffStatic != nil){
            [obj->owned.buffStatic release];
            obj->owned.buffStatic = nil;
        }
        obj->owned.isQueued = NIX_FALSE;
        memset(&obj->owned.callback, 0, sizeof(obj->owned.callback));
    }
    if(callback.func != NULL){
        (*callback.func)(callback.eng, callback.sourceIndex);
    }
}

NixBOOL nixAVAudioSource_convertBuffer_(STNix_AVAudioSource* obj, STNix_AVAudioPCMBuffer* buff, AVAudioFormat* outFmt){
    NixBOOL r = NIX_FALSE;
    if(obj->conv != NULL && !STNix_audioDesc_IsEqual(&obj->pcm.desc, &buff->pcm.desc)){
        nixFmtConverter_destroy(obj->conv);
        obj->conv = NULL;
    }
    if(obj->conv == NULL){
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
            obj->conv = nixFmtConverter_create();
            if(!nixFmtConverter_prepare(obj->conv, &obj->pcm.desc, &outDesc)){
                nixFmtConverter_destroy(obj->conv);
                obj->conv = NULL;
            }
        }
    }
    if(obj->conv != NULL){
        AVAudioFormat* orgFmt = [buff->buff.org format];
        const NixUI32 capCur = [buff->buff.org frameCapacity];
        const NixUI32 capReq = nixFmtConverter_samplesForNewFrequency(capCur, [orgFmt sampleRate], [outFmt sampleRate]);
        //release converted buffer if format or capacity do not macth requirements
        if(buff->buff.conv != nil && (![[buff->buff.conv format] isEqual:outFmt] || [buff->buff.conv frameCapacity] < capReq)){
            [buff->buff.conv release];
            buff->buff.conv = nil;
        }
        if(buff->buff.conv == nil){
            buff->buff.conv = [[AVAudioPCMBuffer alloc] initWithPCMFormat:outFmt frameCapacity:capReq];
        }
        if(buff->buff.org != nil && buff->buff.conv != nil){
            //configure
            NixUI32 iCh;
            const NixUI32 maxChannels = nixFmtConverter_maxChannels();
            //dst
            NixBOOL isInterleavedDst = [outFmt isInterleaved];
            NixUI32 chCountDst = [outFmt channelCount];
            switch([outFmt commonFormat]) {
                case AVAudioPCMFormatFloat32:
                    for(iCh = 0; iCh < chCountDst && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (32 / 8) * (isInterleavedDst ? chCountDst : 1);
                        nixFmtConverter_setPtrAtDstChannel(obj->conv, iCh, buff->buff.conv.floatChannelData[iCh], blockAlign);
                    }
                    break;
                case AVAudioPCMFormatInt16:
                    for(iCh = 0; iCh < chCountDst && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (16 / 8) * (isInterleavedDst ? chCountDst : 1);
                        nixFmtConverter_setPtrAtDstChannel(obj->conv, iCh, buff->buff.conv.int16ChannelData[iCh], blockAlign);
                    }
                    break;
                case AVAudioPCMFormatInt32:
                    for(iCh = 0; iCh < chCountDst && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (32 / 8) * (isInterleavedDst ? chCountDst : 1);
                        nixFmtConverter_setPtrAtDstChannel(obj->conv, iCh, buff->buff.conv.int32ChannelData[iCh], blockAlign);
                    }
                    break;
                default:
                    break;
            }
            //src
            NixBOOL isInterleavedSrc = [orgFmt isInterleaved];
            NixUI32 chCountSrc = [orgFmt channelCount];
            switch([orgFmt commonFormat]) {
                case AVAudioPCMFormatFloat32:
                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (32 / 8) * (isInterleavedSrc ? chCountSrc : 1);
                        nixFmtConverter_setPtrAtSrcChannel(obj->conv, iCh, buff->buff.org.floatChannelData[iCh], blockAlign);
                    }
                    break;
                case AVAudioPCMFormatInt16:
                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (16 / 8) * (isInterleavedSrc ? chCountSrc : 1);
                        nixFmtConverter_setPtrAtSrcChannel(obj->conv, iCh, buff->buff.org.int16ChannelData[iCh], blockAlign);
                    }
                    break;
                case AVAudioPCMFormatInt32:
                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                        const NixUI32 blockAlign = (32 / 8) * (isInterleavedSrc ? chCountSrc : 1);
                        nixFmtConverter_setPtrAtSrcChannel(obj->conv, iCh, buff->buff.org.int32ChannelData[iCh], blockAlign);
                    }
                    break;
                default:
                    break;
            }
            //convert
            NixUI32 ammBlockToRead = [buff->buff.org frameLength], ammBlocksRead = 0, ammBlocksWritten = 0;
            NixUI32 ammBlockForWrite = [buff->buff.conv frameCapacity];
            if(!nixFmtConverter_convert(obj->conv, ammBlockToRead, ammBlockForWrite, &ammBlocksRead, &ammBlocksWritten)){
                //error
            } else {
                //converted
                [buff->buff.conv setFrameLength:ammBlocksWritten];
                r = NIX_TRUE;
            }
        }
    }
    return r;
}

NixBOOL nixAVAudioSource_setBuffer(void* pObj, void* pBuff, void (*callback)(void* pEng, NixUI32 sourceIndex), void* callbackEng, NixUI32 callbackSourceIndex){  //static-source
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL && pBuff != NULL){
        STNix_AVAudioSource* obj    = (STNix_AVAudioSource*)pObj;
        STNix_AVAudioPCMBuffer* buff = (STNix_AVAudioPCMBuffer*)pBuff;
        AVAudioOutputNode* outNode  = [obj->eng outputNode];
        AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
        if(obj->fmt == nil){
            //first buffer to be attached, attach this format to this source
            {
                if(outFmt != nil) [outFmt retain];
                if(obj->fmt != nil){ [obj->fmt release]; obj->fmt = nil; }
            }
            obj->fmt = outFmt;
            obj->pcm.desc = buff->pcm.desc;
        }
        if(obj->fmt != nil && buff->buff.org != nil){
            r = NIX_TRUE;
            if([outFmt isEqual: buff->fmt]){
                //set static buffer
                {
                    [buff->buff.org retain];
                    if(obj->owned.buffStatic != nil) [obj->owned.buffStatic release];
                    obj->owned.buffStatic = buff->buff.org;
                    //
                    obj->owned.callback.func = callback;
                    obj->owned.callback.eng = callbackEng;
                    obj->owned.callback.sourceIndex = callbackSourceIndex;
                }
                //schedule
                if(obj->isPlaying && !obj->isPaused){
                    nixAVAudioSource_ownBuffStaticSchedule_(obj);
                }
            } else {
                //convert and set static buffer
                if(!nixAVAudioSource_convertBuffer_(obj, buff, outFmt)){
                    //error
                } else {
                    //set static buffer
                    {
                        [buff->buff.conv retain];
                        if(obj->owned.buffStatic != nil) [obj->owned.buffStatic release];
                        obj->owned.buffStatic = buff->buff.conv;
                        //
                        obj->owned.callback.func = callback;
                        obj->owned.callback.eng = callbackEng;
                        obj->owned.callback.sourceIndex = callbackSourceIndex;
                    }
                    //schedule
                    if(obj->isPlaying && !obj->isPaused){
                        nixAVAudioSource_ownBuffStaticSchedule_(obj);
                    }
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
NixBOOL nixAVAudioSource_queueBuffer(void* pObj, void* pBuff, void (*callback)(void* pEng, NixUI32 sourceIndex), void* callbackEng, NixUI32 callbackSourceIndex) {
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL && pBuff != NULL){
        STNix_AVAudioSource* obj    = (STNix_AVAudioSource*)pObj;
        STNix_AVAudioPCMBuffer* buff = (STNix_AVAudioPCMBuffer*)pBuff;
        AVAudioOutputNode* outNode  = [obj->eng outputNode];
        AVAudioFormat* outFmt       = [outNode outputFormatForBus:0];
        if(obj->fmt == nil){
            //first buffer to be attached, attach this format to this source
            {
                if(outFmt != nil) [outFmt retain];
                if(obj->fmt != nil){ [obj->fmt release]; obj->fmt = nil; }
            }
            obj->fmt = outFmt;
            obj->pcm.desc = buff->pcm.desc;
        }
        if(obj->fmt != nil && buff->buff.org != nil){
            r = NIX_TRUE;
            if([outFmt isEqual: buff->fmt]){
                //queue buffer directly
                [obj->src scheduleBuffer:buff->buff.org completionHandler:^(void) {
                    if(callback != NULL){
                        (*callback)(callbackEng, callbackSourceIndex);
                    }
                }];
            } else {
                //convert and queue
                if(!nixAVAudioSource_convertBuffer_(obj, buff, outFmt)){
                    //error
                } else {
                    //queue converted buffer
                    [obj->src scheduleBuffer:buff->buff.conv completionHandler:^(void) {
                        if(callback != NULL){
                            (*callback)(callbackEng, callbackSourceIndex);
                        }
                    }];
                }
            }
        }
    }
    return r;
}

//Buffer

void nixAVAudioPCMBuffer_copyPCMData_(AVAudioPCMBuffer* buff, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    void* dataPtr = NULL;
    switch (audioDesc->samplesFormat) {
        case ENNix_sampleFormat_int:
            switch (audioDesc->bitsPerSample) {
                case 16:
                {
                    int16_t* const* chnls = [buff int16ChannelData];
                    if(chnls != NULL && chnls[0] != NULL){
                        dataPtr = chnls[0];
                    }
                }
                    break;
                case 32:
                {
                    int32_t* const* chnls = [buff int32ChannelData];
                    if(chnls != NULL && chnls[0] != NULL){
                        dataPtr = chnls[0];
                    }
                }
                    break;
                default: break;
            }
            break;
        case ENNix_sampleFormat_float:
            switch (audioDesc->bitsPerSample) {
                case 32:
                {
                    float* const* chnls = [buff floatChannelData];
                    if(chnls != NULL && chnls[0] != NULL){
                        dataPtr = chnls[0];
                    }
                }
                    break;
                default: break;
            }
            break;
        default:
            break;
    }
    if(dataPtr != NULL){
        //copy samples data
        if(audioDataPCM != NULL){
            const NixUI32 alignedSz = (audioDataPCMBytes / audioDesc->blockAlign * audioDesc->blockAlign);
            memcpy(dataPtr, audioDataPCM, alignedSz);
            [buff setFrameLength:(alignedSz / audioDesc->blockAlign)];
        }
    }
}
    
AVAudioPCMBuffer* nixAVAudioPCMBuffer_createWithPCMData_(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    AVAudioPCMBuffer* r = nil;
    AVAudioCommonFormat cFmt = AVAudioOtherFormat;
    switch (audioDesc->samplesFormat) {
        case ENNix_sampleFormat_int:
            switch (audioDesc->bitsPerSample) {
                case 16: cFmt = AVAudioPCMFormatInt16; break;
                case 32: cFmt = AVAudioPCMFormatInt32; break;
                default: break;
            }
            break;
        case ENNix_sampleFormat_float:
            switch (audioDesc->bitsPerSample) {
                case 32: cFmt = AVAudioPCMFormatFloat32; break;
                default: break;
            }
            break;
        default:
            break;
    }
    if(cFmt != AVAudioOtherFormat){
        //44100 48000
        AVAudioFormat* fmt = [[AVAudioFormat alloc] initWithCommonFormat:cFmt sampleRate:(double)audioDesc->samplerate channels:(AVAudioChannelCount)audioDesc->channels interleaved:TRUE];
        if(fmt != nil){
            AVAudioPCMBuffer* buff = [[AVAudioPCMBuffer alloc] initWithPCMFormat:fmt frameCapacity:(audioDataPCMBytes / audioDesc->blockAlign)];
            if(buff){
                nixAVAudioPCMBuffer_copyPCMData_(buff, audioDesc, audioDataPCM, audioDataPCMBytes);
                r = buff; [r retain];
                //
                [buff release];
            }
            [fmt release];
        }
    }
    return r;
}
    
void* nixAVAudioPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    void* r = NULL;
    if(audioDesc != NULL){
        AVAudioPCMBuffer* buff = nixAVAudioPCMBuffer_createWithPCMData_(audioDesc, audioDataPCM, audioDataPCMBytes);
        if(buff != nil){
            STNix_AVAudioPCMBuffer* obj = NULL;
            NIX_MALLOC(obj, STNix_AVAudioPCMBuffer, sizeof(STNix_AVAudioPCMBuffer), "STNix_AVAudioPCMBuffer");
            memset(obj, 0, sizeof(STNix_AVAudioPCMBuffer));
            obj->fmt = [buff format]; [obj->fmt retain];
            obj->buff.org = buff; [buff retain];
            //PCM
            {
                obj->pcm.desc = *audioDesc;
            }
            //
            r = obj;
            //
            [buff release];
        }
    }
    return r;
}

void nixAVAudioPCMBuffer_destroy(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioPCMBuffer* obj = (STNix_AVAudioPCMBuffer*)pObj;
        [obj->buff.org release];
        [obj->buff.conv release];
        [obj->fmt release];
        //PCM
        {
            //
        }
        memset(obj, 0, sizeof(STNix_AVAudioPCMBuffer));
        NIX_FREE(obj);
        obj = NULL;
    }
}
   
NixBOOL nixAVAudioPCMBuffer_setData(void* pObj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes){
    NixBOOL r = NIX_FALSE;
    if(pObj != NULL && audioDesc != NULL){
        STNix_AVAudioPCMBuffer* obj = (STNix_AVAudioPCMBuffer*)pObj;
        const NixUI32 reqCap = (audioDataPCMBytes / audioDesc->blockAlign);
        if(obj->buff.org != nil && (!STNix_audioDesc_IsEqual(&obj->pcm.desc, audioDesc) || [obj->buff.org frameCapacity] < reqCap)){
            //release
            if(obj->fmt != nil){
                [obj->fmt release];
                obj->fmt = nil;
            }
            [obj->buff.org release];
            [obj->buff.conv release];
            obj->buff.org = NULL;
            obj->buff.conv = NULL;
            //create
            AVAudioPCMBuffer* buff = nixAVAudioPCMBuffer_createWithPCMData_(audioDesc, audioDataPCM, audioDataPCMBytes);
            if(buff != nil){
                obj->buff.org = buff; [buff retain];
                obj->fmt = [buff format]; [obj->fmt retain];
                r = NIX_TRUE;
                //
                [buff release];
            }
        } else {
            nixAVAudioPCMBuffer_copyPCMData_(obj->buff.org, audioDesc, audioDataPCM, audioDataPCMBytes);
            r = NIX_TRUE;
        }
    }
    return r;
}

//nixFmtConverter

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
