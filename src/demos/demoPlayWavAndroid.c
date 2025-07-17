//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>	//printf
#include <stdlib.h>	//malloc, free, rand()
#include <time.h>   //time
#include <string.h> //memset

#include <jni.h>
#include <android/log.h>	//for __android_log_print()

#include "nixtla-audio.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"

#ifndef PRINTF_INFO
#   define PRINTF_INFO(STR_FMT, ...)    __android_log_print(ANDROID_LOG_INFO, "Nixtla", "INFO, "STR_FMT, ##__VA_ARGS__)
#endif
#ifndef PRINTF_ERROR
#   define PRINTF_ERROR(STR_FMT, ...)   __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
#endif
#ifndef PRINTF_WARNING
#   define PRINTF_WARNING(STR_FMT, ...) __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C" {
#endif

static STNix_Engine nix;
static NixUI16 iSourcePlayG = 0;
static NixUI32 secsAccum = 0;
static NixUI32 msAccum = 0;
    
JNIEXPORT jboolean JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoStart(JNIEnv* env, jobject obj, jobject assetManager){
    jboolean r = 0;
    PRINTF_INFO("native-demoStart\n");
    //
    srand((unsigned int)time(NULL));
    //
    if(nixInit(&nix, 8)){
        nixPrintCaps(&nix);
        NixUI16 iSourcePlay = 0;
        //randomly select a wav from the list
        const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
        NixUI8* audioData = NULL;
        NixUI32 audioDataBytes = 0;
        STNix_audioDesc audioDesc;
        if(!loadDataFromWavFile(env, assetManager, strWavPath, &audioDesc, &audioData, &audioDataBytes)){
            PRINTF_ERROR("ERROR, loading WAV file: '%s'.\n", strWavPath);
        } else {
            PRINTF_INFO("WAV file loaded (%d bytes): '%s'.\n", audioDataBytes, strWavPath);
            iSourcePlay = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
            if(iSourcePlay == 0){
                PRINTF_ERROR("Source assign failed.\n");
            } else {
                PRINTF_INFO("Source(%d) assigned and retained.\n", iSourcePlay);
                NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
                if(iBufferWav == 0){
                    PRINTF_ERROR("Buffer assign failed.\n");
                } else {
                    PRINTF_INFO("Buffer(%d) loaded with data and retained.\n", iBufferWav);
                    if(!nixSourceSetBuffer(&nix, iSourcePlay, iBufferWav)){
                        PRINTF_ERROR("Buffer-to-source linking failed.\n");
                    } else {
                        PRINTF_INFO("Buffer(%d) linked with source(%d).\n", iBufferWav, iSourcePlay);
                        iSourcePlayG = iSourcePlay;
                        nixSourceSetRepeat(&nix, iSourcePlay, NIX_TRUE);
                        nixSourceSetVolume(&nix, iSourcePlay, 1.0f);
                        nixSourcePlay(&nix, iSourcePlay);
                        r = 1;
                    }
                    //release buffer (already retained by source if success)
                    nixBufferRelease(&nix, iBufferWav);
                    iBufferWav = 0;
                }
            }
        }
        if(audioData != NULL){
            free(audioData);
            audioData = NULL;
        }
    }
    return r;
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoEnd(JNIEnv* env, jobject obj){
    PRINTF_INFO("native-demoEnd\n");
    if(iSourcePlayG != 0){
        nixSourceRelease(&nix, iSourcePlayG);
        iSourcePlayG = 0;
    }
    nixFinalize(&nix);
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoTick(JNIEnv* env, jobject obj){
    const NixUI32 msPerTick = 1000 / 30;
    nixTick(&nix);
    msAccum += msPerTick;
    if(msAccum >= 1000){
        secsAccum += (msAccum / 1000);
        if(iSourcePlayG > 0){
            PRINTF_INFO("%u secs playing\n", secsAccum);
        } else {
            PRINTF_INFO("%u secs running\n", secsAccum);
        }
        msAccum %= 1000;
    }
}

#ifdef __cplusplus
}
#endif
