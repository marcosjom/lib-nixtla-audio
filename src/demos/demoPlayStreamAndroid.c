//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>	//printf
#include <stdlib.h>	//malloc, free, rand
#include <time.h>   //time
#include <string.h> //memset

#include <jni.h>
#include <android/log.h>    //for __android_log_print()

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

typedef struct STDemoStreamState_ {
    STNix_Engine    nix;
    NixUI16         iSourcePlay;
    NixUI16         buffsWav[10];
    NixUI32         buffsUse;
    NixUI32         iOldestBuffQueued;
    //stats
    struct {
        NixUI32     buffQueued;
        NixUI32     buffUnqueued;
    } stats;
} STDemoStreamState;

STDemoStreamState state;
NixUI32 secsAccum = 0;
NixUI32 msAccum = 0;

//callback

void demoStreamBufferUnqueuedCallback_(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount);

JNIEXPORT jboolean JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoStreamStart(JNIEnv* env, jobject obj, jobject assetManager){
    jboolean r = 0;
    PRINTF_INFO("native-demoStart\n");
    memset(&state, 0, sizeof(state));
    //
    srand((unsigned int)time(NULL));
    //
    if(nixInit(&state.nix, 8)){
        nixPrintCaps(&state.nix);
        const NixUI32 ammBuffs = (sizeof(state.buffsWav) / sizeof(state.buffsWav[0])); //ammount of buffers for stream
        //randomly select a wav from the list
        const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
        NixUI8* audioData = NULL;
        NixUI32 audioDataBytes = 0;
        STNix_audioDesc audioDesc;
        if(!loadDataFromWavFile(env, assetManager, strWavPath, &audioDesc, &audioData, &audioDataBytes)){
            PRINTF_ERROR("ERROR, loading WAV file: '%s'.\n", strWavPath);
        } else {
            PRINTF_INFO("WAV file loaded: '%s'.\n", strWavPath);
            state.iSourcePlay = nixSourceAssignStream(&state.nix, NIX_TRUE, 0, NULL, NULL, ammBuffs, demoStreamBufferUnqueuedCallback_, &state);
            if(state.iSourcePlay == 0){
                PRINTF_ERROR("Source assign failed.\n");
            } else {
                PRINTF_INFO("Source(%d) assigned and retained.\n", state.iSourcePlay);
                //populate buffers
                const NixUI32 samplesWav = (audioDataBytes / audioDesc.blockAlign); //ammount of samples available
                const NixUI32 samplesPerBuff = (samplesWav / (ammBuffs - 1)); //-1, last buffer could be ignored or partially populated
                NixUI32 iSample = 0;
                while(iSample < samplesWav && state.buffsUse < ammBuffs){
                    void* samplesPtr = &audioData[iSample * audioDesc.blockAlign];
                    NixUI32 samplesToUse = (samplesWav - iSample);
                    if(samplesToUse > samplesPerBuff){
                        samplesToUse = samplesPerBuff;
                    }
                    if(samplesToUse <= 0){
                        //no more samples to add
                        break;
                    } else {
                        state.buffsWav[state.buffsUse] = nixBufferWithData(&state.nix, &audioDesc, samplesPtr, samplesToUse * audioDesc.blockAlign);
                        if(state.buffsWav[state.buffsUse] == 0){
                            PRINTF_ERROR("Buffer assign failed.\n");
                            break;
                        } else {
                            PRINTF_INFO("Buffer(%d) loaded with data and retained.\n", state.buffsWav[state.buffsUse]);
                            if(!nixSourceStreamAppendBuffer(&state.nix, state.iSourcePlay, state.buffsWav[state.buffsUse])){
                                PRINTF_ERROR("Buffer-to-source queueing failed.\n");
                                break;
                            } else {
                                state.stats.buffQueued++;
                                iSample += samplesToUse;
                                state.buffsUse++;
                            }
                        }
                    }
                }
                //start stream
                nixSourceSetVolume(&state.nix, state.iSourcePlay, 1.0f);
                nixSourcePlay(&state.nix, state.iSourcePlay);
            }
        }
        //wav samples already loaded into buffers
        if(audioData != NULL){
            free(audioData);
            audioData = NULL;
        }
    }
    //
    return r;
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoStreamEnd(JNIEnv* env, jobject obj){
    PRINTF_INFO("native-demoEnd\n");
    if(state.iSourcePlay != 0){
        nixSourceRelease(&state.nix, state.iSourcePlay);
        state.iSourcePlay = 0;
    }
    nixFinalize(&state.nix);
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_demoStreamTick(JNIEnv* env, jobject obj){
    const NixUI32 msPerTick = 1000 / 30;
    nixTick(&state.nix);
    msAccum += msPerTick;
    if(msAccum >= 1000){
        secsAccum += (msAccum / 1000);
        PRINTF_INFO("%u secs playing (%d buffs enqueued, %d dequeued)\n", secsAccum, state.stats.buffQueued, state.stats.buffUnqueued);
        state.stats.buffQueued = 0;
        state.stats.buffUnqueued = 0;
        msAccum %= 1000;
    }
}

//callback

void demoStreamBufferUnqueuedCallback_(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount){
    STDemoStreamState* state = (STDemoStreamState*)userdata;
    //re-enqueue buffers
    NixUI16 buffToReEnq = buffersUnqueuedCount;
    while(buffToReEnq > 0){
        NixUI16 iBuff = state->buffsWav[state->iOldestBuffQueued % state->buffsUse];
        state->iOldestBuffQueued = (state->iOldestBuffQueued + 1) % state->buffsUse;
        if(!nixSourceStreamAppendBuffer(&state->nix, state->iSourcePlay, iBuff)){
            PRINTF_ERROR("Buffer-to-source queueing failed.\n");
        } else {
            state->stats.buffQueued++;
        }
        state->stats.buffUnqueued++;
        --buffToReEnq;
    }
}
