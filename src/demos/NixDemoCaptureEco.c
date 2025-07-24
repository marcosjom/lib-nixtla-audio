//
//  NixDemoCaptureEco.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo creates multiple buffers,
// and switches between recording on them (filling)
// and playing them (stream playback).
//

#include "NixDemoCaptureEco.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"
//
#include <stdio.h>  //printf
#include <stdlib.h> //rand

//recorder callback

void bufferCapturedCallback(STNixEngineRef* eng, STNixRecorderRef* rec, const STNixAudioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 blocksCount, void* userdata);

//player callback

void bufferUnqueuedCallback(STNixSourceRef* src, STNixBufferRef* buffs, const NixUI32 buffsSz, void* userdata);

//

NixBOOL NixDemoCaptureEco_isNull(STNixDemoCaptureEco* obj){
    return NixSource_isNull(obj->src);
}

NixBOOL NixDemoCaptureEco_init(STNixDemoCaptureEco* obj, STNixDemosCommon* common){
    NixBOOL r = NIX_FALSE;
    obj->src = NixEngine_allocSource(common->eng);
    if(!NixSource_isNull(obj->src)){
        STNixAudioDesc audioDesc   = STNixAudioDesc_Zero;
        audioDesc.samplesFormat     = ENNixSampleFmt_Int;
        audioDesc.channels          = 1;
        audioDesc.bitsPerSample     = 16;
        audioDesc.samplerate        = 22050;
        audioDesc.blockAlign        = (audioDesc.bitsPerSample / 8) * audioDesc.channels;
        const NixUI16 buffersCount  = (sizeof(obj->buffs.arr) / sizeof(obj->buffs.arr[0]));
        const NixUI16 blocksPerBuffer = (audioDesc.samplerate / NIX_DEMO_ECO_BUFFS_PER_SEC);
        //allocate buffers
        while(obj->buffs.use < (sizeof(obj->buffs.arr) / sizeof(obj->buffs.arr[0]))){
            STNixBufferRef buff = NixEngine_allocBuffer(common->eng, &audioDesc, NULL, blocksPerBuffer * audioDesc.blockAlign);
            if(NixBuffer_isNull(buff)){
                NIX_PRINTF_ERROR("NixEngine_allocBuffer failed.\n");
                break;
            } else {
                obj->buffs.arr[obj->buffs.use++] = buff;
                NixBuffer_null(&buff);
            }
        }
        //
        if(obj->buffs.use <= 0){
            NIX_PRINTF_ERROR("No stream-buffer could be created\n");
        } else {
            //init the capture
            obj->buffs.iFilled = 0;
            obj->buffs.iPlayed = obj->buffs.use;
            obj->buffs.bytesPerBuffer = blocksPerBuffer * audioDesc.blockAlign;
            obj->buffs.blocksPerBuffer = blocksPerBuffer;
            //
            NixSource_setCallback(obj->src, bufferUnqueuedCallback, obj);
            NixSource_setVolume(obj->src, 1.0f);
            NixSource_play(obj->src);
            //
            obj->rec = NixEngine_allocRecorder(common->eng, &audioDesc, buffersCount, blocksPerBuffer);
            if(NixRecorder_isNull(obj->rec)){
                NIX_PRINTF_ERROR("NixEngine_allocRecorder failed.\n");
            } else {
                NixRecorder_setCallback(obj->rec, bufferCapturedCallback, obj);
                if(!NixRecorder_start(obj->rec)){
                    NIX_PRINTF_ERROR("nixCaptureStart failed.\n");
                } else {
                    r = NIX_TRUE;
                }
            }
        }
    }
    return r;
}

void NixDemoCaptureEco_destroy(STNixDemoCaptureEco* obj){
    //recorder
    if(!NixRecorder_isNull(obj->rec)){
        NixRecorder_stop(obj->rec);
        NixRecorder_release(&obj->rec);
        NixRecorder_null(&obj->rec);
    }
    //source
    if(!NixSource_isNull(obj->src)){
        NixSource_release(&obj->src);
        NixSource_null(&obj->src);
    }
    //buffers
    {
        NixUI32 i; for(i = 0; i < obj->buffs.use; i++){
            STNixBufferRef* b = &obj->buffs.arr[i];
            if(!NixBuffer_isNull(*b)){
                NixBuffer_release(b);
                NixBuffer_null(b);
            }
        }
    }
    *obj = (STNixDemoCaptureEco)STNixDemoCaptureEco_Zero;
}

//recorder callback

void bufferCapturedCallback(STNixEngineRef* eng, STNixRecorderRef* rec, const STNixAudioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 blocksCount, void* userdata){
    STNixDemoCaptureEco* obj = (STNixDemoCaptureEco*)userdata;
    obj->stats.curSec.framesRecordedCount += blocksCount;
    //calculate avg (for dbg and stats)
    if(audioDesc.bitsPerSample == 16 && audioDesc.samplesFormat == ENNixSampleFmt_Int){
        const NixSI16* s16 = (const NixSI16*)audioData;
        const NixSI16* s16AfterEnd = s16 + (audioDataBytes / 2);
        obj->stats.curSec.avgRec.count += (s16AfterEnd - s16);
        while(s16 < s16AfterEnd){
            obj->stats.curSec.avgRec.sum += *s16;
            ++s16;
        }
    }
    //fill buffer
    if(obj->buffs.iFilled >= obj->buffs.use){
        //ignore samples, probably is the flush after stopping the recording
    } else {
        //fill samples into a buffer
        STNixBufferRef buff = obj->buffs.arr[obj->buffs.iFilled];
        if(!NixBuffer_setData(buff, &audioDesc, audioData, audioDataBytes)){
            NIX_PRINTF_ERROR("bufferCapturedCallback, NixBuffer_setData failed\n");
        } else {
            if(audioDataBytes < obj->buffs.bytesPerBuffer){
                //fill remainig space with zeroes
                if(!NixBuffer_fillWithZeroes(buff)){
                    NIX_PRINTF_ERROR("bufferCapturedCallback, NixBuffer_fillWithZeroes failed\n");
                }
            }
            ++obj->buffs.iFilled;
        }
        //start playback
        if(obj->buffs.iFilled >= obj->buffs.use){
            NIX_PRINTF_INFO("bufferCapturedCallback, stopping capture, feeding stream\n");
            NixRecorder_stop(obj->rec);
            //
            obj->buffs.iPlayed = 0;
            //queue all buffers
            {
                NixUI32 iBuff; for(iBuff = 0; iBuff < obj->buffs.use; ++iBuff){
                    STNixBufferRef buff = obj->buffs.arr[iBuff];
                    if(!NixSource_queueBuffer(obj->src, buff)){
                        NIX_PRINTF_ERROR("bufferCapturedCallback, NixSource_queueBuffer failed.\n");
                    }
                }
            }
        }
    }
}

//player callback

void bufferUnqueuedCallback(STNixSourceRef* src, STNixBufferRef* buffs, const NixUI32 buffsSz, void* userdata){
    STNixDemoCaptureEco* obj = (STNixDemoCaptureEco*)userdata;
    obj->stats.curSec.buffsPlayedCount += buffsSz;
    //
    obj->buffs.iPlayed += buffsSz;
    if(obj->buffs.iPlayed >= obj->buffs.use){
        NIX_PRINTF_INFO("bufferCapturedCallback, stream played (hungry), starting capture\n");
        //
        obj->buffs.iFilled = 0;
        if(!NixRecorder_start(obj->rec)){
            NIX_PRINTF_ERROR("NixRecorder_start failed.\n");
        }
    }
}
