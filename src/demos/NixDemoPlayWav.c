//
//  NixDemoPlayWav.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo loads a WAV's PCM samples into one buffer,
// links it to a source in repeat-mode and plays it.
//

#include "NixDemoPlayWav.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"
//
#include <stdio.h>  //printf
#include <stdlib.h> //rand

NixBOOL NixDemoPlayWav_isNull(STNixDemoPlayWav* obj){
    return NixSource_isNull(obj->src);
}

NixBOOL NixDemoPlayWav_init(
                            STNixDemoPlayWav* obj
                            , STNixDemosCommon* common
#                           ifdef __ANDROID__
                            , JNIEnv *env
                            , jobject assetManager
#                           endif
                            )
{
    NixBOOL r = NIX_FALSE;
    const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
    NixUI8* audioData = NULL;
    NixUI32 audioDataBytes = 0;
    STNixAudioDesc audioDesc;
    if(
       !nixUtilLoadDataFromWavFile(
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
        STNixSourceRef src = NixEngine_allocSource(common->eng);
        if(NixSource_isNull(src)){
            NIX_PRINTF_ERROR("ERROR, NixEngine_allocSource failed.\n");
        } else {
            STNixBufferRef buff = NixEngine_allocBuffer(common->eng, &audioDesc, audioData, audioDataBytes);
            if(NixBuffer_isNull(buff)){
                NIX_PRINTF_ERROR("ERROR, NixEngine_allocBuffer failed.\n");
            } else {
                if(!NixSource_setBuffer(src, buff)){
                    NIX_PRINTF_ERROR("ERROR, NixSource_setBuffer failed.\n");
                } else {
                    NIX_PRINTF_INFO("Buffer loaded to source.\n");
                    NixSource_setRepeat(src, NIX_TRUE);
                    NixSource_setVolume(src, 1.0f);
                    NixSource_play(src);
                    //retain source
                    NixSource_set(&obj->src, src);
                    r = NIX_TRUE;
                }
                //Buffer is retained by the source
                NixBuffer_release(&buff);
            }
        }
        NixSource_release(&src);
        NixSource_null(&src);
    }
    //wav samples already loaded into buffer
    if(audioData != NULL){
        free(audioData);
        audioData = NULL;
    }
    return r;
}

void NixDemoPlayWav_destroy(STNixDemoPlayWav* obj){
    //src
    {
        NixSource_release(&obj->src);
        NixSource_null(&obj->src);
    }
    *obj = (STNixDemoPlayWav)STNixDemoPlayWav_Zero;
}
