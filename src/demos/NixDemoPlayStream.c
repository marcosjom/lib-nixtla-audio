//
//  NixDemoPlayStream.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo loads a WAV's PCM samples into multiple buffers,
// links them to a source and re-links them when returned
// by the callback (buffer-played event).
//

#include "NixDemoPlayStream.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"
//
#include <stdio.h>  //printf
#include <stdlib.h> //rand

//player callback

void demoStreamBufferUnqueuedCallback_(STNixSourceRef* src, STNixBufferRef* buffs, const NixUI32 buffsSz, void* userData);

//

NixBOOL NixDemoPlayStream_isNull(STNixDemoPlayStream* obj){
    return NixSource_isNull(obj->src);
}

NixBOOL NixDemoPlayStream_init(
                               STNixDemoPlayStream* obj, STNixDemosCommon* common
#                               ifdef __ANDROID__
                               , JNIEnv *env
                               , jobject assetManager
#                               endif
                               )
{
    NixBOOL r = NIX_FALSE;
    const NixUI32 ammBuffs = (sizeof(obj->buffs) / sizeof(obj->buffs[0])); //ammount of buffers for stream
    //randomly select a wav from the list
    const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
    NixUI8* audioData = NULL;
    NixUI32 audioDataBytes = 0;
    STNixAudioDesc audioDesc;
    if(!nixUtilLoadDataFromWavFile(
#                                  ifdef __ANDROID__
                                   env,
                                   assetManager,
#                                  endif
                                   strWavPath, &audioDesc, &audioData, &audioDataBytes
                                   )
       )
    {
        NIX_PRINTF_ERROR("ERROR, loading WAV file: '%s'.\n", strWavPath);
    } else {
        NIX_PRINTF_INFO("WAV file loaded: '%s'.\n", strWavPath);
        obj->src = NixEngine_allocSource(common->eng);
        if(NixSource_isNull(obj->src)){
            NIX_PRINTF_ERROR("NixEngine_allocSource failed.\n");
        } else {
            NIX_PRINTF_INFO("NixEngine_allocSource ok.\n");
            //allocate buffers
            {
                const NixUI32 samplesWav = (audioDataBytes / audioDesc.blockAlign); //ammount of samples available
                const NixUI32 samplesPerBuff = (samplesWav / (ammBuffs - 1)); //-1, last buffer could be ignored or partially populated
                NixUI32 iSample = 0;
                while(iSample < samplesWav && obj->buffsUse < ammBuffs){
                    void* samplesPtr = &audioData[iSample * audioDesc.blockAlign];
                    NixUI32 samplesToUse = (samplesWav - iSample);
                    if(samplesToUse > samplesPerBuff){
                        samplesToUse = samplesPerBuff;
                    }
                    if(samplesToUse <= 0){
                        //no more samples to add
                        break;
                    } else {
                        obj->buffs[obj->buffsUse] = NixEngine_allocBuffer(common->eng, &audioDesc, samplesPtr, samplesToUse * audioDesc.blockAlign);
                        if(NixBuffer_isNull(obj->buffs[obj->buffsUse])){
                            NIX_PRINTF_ERROR("Buffer assign failed.\n");
                            break;
                        } else {
                            NIX_PRINTF_INFO("Buffer loaded with data and retained.\n");
                            if(!NixSource_queueBuffer(obj->src, obj->buffs[obj->buffsUse])){
                                NIX_PRINTF_ERROR("Buffer-to-source queueing failed.\n");
                                break;
                            } else {
                                obj->stats.curSec.buffQueued++;
                                iSample += samplesToUse;
                                obj->buffsUse++;
                            }
                        }
                    }
                }
            }
            //start stream
            NixSource_setCallback(obj->src, demoStreamBufferUnqueuedCallback_, obj);
            NixSource_setVolume(obj->src, 1.0f);
            NixSource_play(obj->src);
            //
            r = NIX_TRUE;
        }
    }
    //wav samples already loaded into buffers
    if(audioData != NULL){
        free(audioData);
        audioData = NULL;
    }
    return r;
}

void NixDemoPlayStream_destroy(STNixDemoPlayStream* obj){
    //release buffers
    {
        NixUI32 i = 0; for(i = 0; i < obj->buffsUse; i++){
            STNixBufferRef* b = &obj->buffs[i];
            if(!NixBuffer_isNull(*b)){
                NixBuffer_release(b);
                NixBuffer_null(b);
            }
        }
        obj->buffsUse = 0;
    }
    //src
    {
        NixSource_release(&obj->src);
        NixSource_null(&obj->src);
    }
    *obj = (STNixDemoPlayStream)STNixDemoPlayStream_Zero;
}

//player callback

void demoStreamBufferUnqueuedCallback_(STNixSourceRef* src, STNixBufferRef* buffs, const NixUI32 buffsSz, void* userData){
    STNixDemoPlayStream* obj = (STNixDemoPlayStream*)userData;
    //re-enqueue buffers
    if(NixSource_isSame(obj->src, *src)){
        NixUI16 buffToReEnq = buffsSz;
        while(buffToReEnq > 0){
            STNixBufferRef buff = obj->buffs[obj->iOldestBuffQueued % obj->buffsUse];
            obj->iOldestBuffQueued = (obj->iOldestBuffQueued + 1) % obj->buffsUse;
            if(!NixSource_queueBuffer(obj->src, buff)){
                NIX_PRINTF_ERROR("NixSource_queueBuffer failed.\n");
            } else {
                obj->stats.curSec.buffQueued++;
            }
            obj->stats.curSec.buffUnqueued++;
            --buffToReEnq;
        }
    }
}
