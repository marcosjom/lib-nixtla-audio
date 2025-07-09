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

#include "nixtla-audio-private.h"
#include "nixtla-audio.h"
//
#include <AVFAudio/AVAudioEngine.h>
#include <AVFAudio/AVAudioPlayerNode.h>
#include <AVFAudio/AVAudioMixerNode.h>
#include <AVFAudio/AVAudioFormat.h>
#include <AVFAudio/AVAudioChannelLayout.h>
#include <AVFAudio/AVAudioConverter.h>

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

typedef struct STNix_AVAudioSourceCallback_ {
    void            (*func)(void* pEng, NixUI32 sourceIndex);
    void*           eng;
    NixUI32         sourceIndex;
} STNix_AVAudioSourceCallback;
    
typedef struct STNix_AVAudioSource_ {
    AVAudioFormat*          fmt;    //AVAudioFormat
    AVAudioPlayerNode*      src;    //AVAudioPlayerNode
    AVAudioEngine*          eng;    //AVAudioEngine
    //AVAudioConverter*     conv;   //AVAudioConverter
    void*                   convv;  //nixFmtConverter
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
                    NIX_PRINTF_ERROR("AVAudioEngine::startAndReturnError failed: '%s'.\n", err == nil ? "unknown" : [[err description] UTF8String]);
                }
                obj->engStarted = NIX_TRUE;
            }
        }
    }
    return obj;
}

void nixAVAudioSource_destroy(void* pObj){
    if(pObj != NULL){
        STNix_AVAudioSource* obj = (STNix_AVAudioSource*)pObj;
        if(obj->convv != NULL){ nixFmtConverter_destroy(obj->convv); obj->convv = NULL; }
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
        r = [obj->src isPlaying] ? NIX_TRUE : NIX_FALSE;
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
    if(obj->convv != NULL && !STNix_audioDesc_IsEqual(&obj->pcm.desc, &buff->pcm.desc)){
        nixFmtConverter_destroy(obj->convv);
        obj->convv = NULL;
    }
    if(obj->convv == NULL){
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
            obj->convv = nixFmtConverter_create();
            if(!nixFmtConverter_prepare(obj->convv, &obj->pcm.desc, &outDesc)){
                nixFmtConverter_destroy(obj->convv);
                obj->convv = NULL;
            }
        }
    }
    if(obj->convv != NULL){
        AVAudioFormat* orgFmt = [buff->buff.org format];
        const NixUI32 capCur = [buff->buff.org frameCapacity];
        const NixUI32 capReq = nixFmtConverter_samplesForNewFrequency([orgFmt sampleRate], [outFmt sampleRate], capCur);
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
                        nixFmtConverter_setPtrAtDstChannel(obj->convv, iCh, buff->buff.conv.floatChannelData[iCh], (32 / 8) * (isInterleavedDst ? chCountDst : 1));
                    }
                    break;
                case AVAudioPCMFormatInt16:
                    for(iCh = 0; iCh < chCountDst && iCh < maxChannels; ++iCh){
                        nixFmtConverter_setPtrAtDstChannel(obj->convv, iCh, buff->buff.conv.int16ChannelData[iCh], (16 / 8) * (isInterleavedDst ? chCountDst : 1));
                    }
                    break;
                case AVAudioPCMFormatInt32:
                    for(iCh = 0; iCh < chCountDst && iCh < maxChannels; ++iCh){
                        nixFmtConverter_setPtrAtDstChannel(obj->convv, iCh, buff->buff.conv.int32ChannelData[iCh], (16 / 8) * (isInterleavedDst ? chCountDst : 1));
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
                        nixFmtConverter_setPtrAtSrcChannel(obj->convv, iCh, buff->buff.org.floatChannelData[iCh], (32 / 8) * (isInterleavedSrc ? chCountSrc : 1));
                    }
                    break;
                case AVAudioPCMFormatInt16:
                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                        nixFmtConverter_setPtrAtSrcChannel(obj->convv, iCh, buff->buff.org.int16ChannelData[iCh], (16 / 8) * (isInterleavedSrc ? chCountSrc : 1));
                    }
                    break;
                case AVAudioPCMFormatInt32:
                    for(iCh = 0; iCh < chCountSrc && iCh < maxChannels; ++iCh){
                        nixFmtConverter_setPtrAtSrcChannel(obj->convv, iCh, buff->buff.org.int32ChannelData[iCh], (16 / 8) * (isInterleavedSrc ? chCountSrc : 1));
                    }
                    break;
                default:
                    break;
            }
            //convert
            NixUI32 ammBlockToRead = [buff->buff.org frameLength], ammBlocksWritten = 0;
            if(!nixFmtConverter_convert(obj->convv, ammBlockToRead, &ammBlocksWritten)){
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

