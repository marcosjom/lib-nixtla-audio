//
//  main.c
//  nixtla-audio-demos
//
//  Created by Marcos Ortega on 10/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#include <stdio.h>  //printf
#include <stdlib.h> //malloc, free, rand
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

static STNix_Engine nix;
static NixUI32 secsAccum = 0;
static NixUI32 msAccum = 0;
//original source
static NixUI16 iSourceOrg = 0, iSourceOrgPlayCount = 0;
static NixUI8* audioData = NULL;
static NixUI32 audioDataBytes = 0;
static STNix_audioDesc audioDesc;
//converted source
static NixUI16 iSourceConv = 0, iSourceConvPlayCount = 0;
static NixUI8* audioDataConv = NULL;
static NixUI32 audioDataBytesConv = 0;
static STNix_audioDesc audioDescConv;

JNIEXPORT jboolean JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_testStart(JNIEnv* env, jobject obj, jobject assetManager){
    jboolean r = 0;
    PRINTF_INFO("native-testStart\n");
    //
    srand((unsigned int)time(NULL));
    //
    if(nixInit(&nix, 8)){
        nixPrintCaps(&nix);
        //randomly select a wav from the list
        const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
        if(!loadDataFromWavFile(env, assetManager, strWavPath, &audioDesc, &audioData, &audioDataBytes)){
            PRINTF_ERROR("ERROR, loading WAV file: '%s'.\n", strWavPath);
        } else {
            PRINTF_INFO("WAV file loaded: '%s'.\n", strWavPath);
            NixUI16 iSrcTmp = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
            if(iSrcTmp == 0){
                PRINTF_ERROR("Source assign failed.\n");
            } else {
                NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
                if(iBufferWav == 0){
                    PRINTF_ERROR("Buffer assign failed.\n");
                } else {
                    if(!nixSourceSetBuffer(&nix, iSrcTmp, iBufferWav)){
                        PRINTF_ERROR("Buffer-to-source linking failed.\n");
                    } else {
                        PRINTF_INFO("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
                        //retain source
                        iSourceOrg = iSrcTmp;
                        nixSourceRetain(&nix, iSrcTmp);
                        //start
                        nixSourceSetVolume(&nix, iSrcTmp, 1.0f);
                        nixSourcePlay(&nix, iSrcTmp);
                        //
                        r = 1;
                    }
                    //release buffer (already retained by source if success)
                    nixBufferRelease(&nix, iBufferWav);
                    iBufferWav = 0;
                }
                //release source
                nixSourceRelease(&nix, iSrcTmp);
                iSrcTmp = 0;
            }
        }
    }
    return r;
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_testEnd(JNIEnv* env, jobject obj){
    PRINTF_INFO("native-testEnd\n");
    if(audioData!=NULL){
        free(audioData);
        audioData = NULL;
    }
    if(audioDataConv != NULL){
        free(audioDataConv);
        audioDataConv = NULL;
    }
    if(iSourceOrg != 0){
        nixSourceRelease(&nix, iSourceOrg);
        iSourceOrg = 0;
    }
    if(iSourceConv != 0){
        nixSourceRelease(&nix, iSourceConv);
        iSourceConv = 0;
    }
    nixFinalize(&nix);
}

JNIEXPORT void JNICALL Java_com_mortegam_nixtla_1audio_MainActivity_testTick(JNIEnv* env, jobject obj){
    if(iSourceOrg > 0){
        const NixUI32 msPerTick = 1000 / 30;
        nixTick(&nix);
        msAccum += msPerTick;
        if(msAccum >= 1000){
            secsAccum += (msAccum / 1000);
            //PRINTF_INFO("%u secs playing\n", secsAccum);
            msAccum %= 1000;
        }
        //analyze converted playback
        if(iSourceConv != 0){
            if(!nixSourceIsPlaying(&nix, iSourceConv)){
                iSourceConvPlayCount++;
                if(iSourceConvPlayCount < 2){
                    //play again
                    PRINTF_INFO("converted played %u time(s).\n", iSourceConvPlayCount + 1);
                    nixSourcePlay(&nix, iSourceConv);
                } else {
                    iSourceConvPlayCount = 0;
                    //release
                    nixSourceRelease(&nix, iSourceConv);
                    iSourceConv = 0;
                    //play original
                    iSourceOrgPlayCount = 0;
                    PRINTF_INFO("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
                    nixSourcePlay(&nix, iSourceOrg);
                }
            }
        } else {
            //analyze original playback
            if(!nixSourceIsPlaying(&nix, iSourceOrg)){
                iSourceOrgPlayCount++;
                if(iSourceOrgPlayCount < 2){
                    //play again
                    PRINTF_INFO("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
                    nixSourcePlay(&nix, iSourceOrg);
                } else {
                    iSourceOrgPlayCount = 0;
                    //create random conversion
                    if(iSourceConv != 0){
                        nixSourceRelease(&nix, iSourceConv);
                        iSourceConv = 0;
                    }
                    switch(rand() % 4){
                        case 0:
                            audioDescConv.samplesFormat = ENNix_sampleFormat_int;
                            audioDescConv.bitsPerSample = 8;
                            break;
                        case 1:
                            audioDescConv.samplesFormat = ENNix_sampleFormat_int;
                            audioDescConv.bitsPerSample = 16;
                            break;
                        case 2:
                            audioDescConv.samplesFormat = ENNix_sampleFormat_int;
                            audioDescConv.bitsPerSample = 32;
                            break;
                        default:
                            audioDescConv.bitsPerSample = 32;
                            audioDescConv.samplesFormat = ENNix_sampleFormat_float;
                            break;
                    }
                    audioDescConv.channels = 1 + (rand() % 2);
                    audioDescConv.samplerate = (rand() % 10) == 0 ? audioDesc.samplerate : 800 + (rand() % 92000);
                    audioDescConv.blockAlign = (audioDescConv.bitsPerSample / 8) * audioDescConv.channels;
                    const NixUI32 samplesReq = nixFmtConverter_samplesForNewFrequency(audioDataBytes / audioDesc.blockAlign, audioDesc.samplerate, audioDescConv.samplerate);
                    const NixUI32 bytesReq = samplesReq * audioDescConv.blockAlign;
                    if(audioDataBytesConv < bytesReq){
                        if(audioDataConv != NULL){
                            free(audioDataConv);
                            audioDataConv = NULL;
                        }
                        audioDataBytesConv = 0;
                        audioDataConv = (NixUI8*)malloc(bytesReq);
                        if(audioDataConv != NULL){
                            audioDataBytesConv = bytesReq;
                            PRINTF_INFO("Conversion buffer resized to %u bytes.\n", audioDataBytesConv);
                        }
                    }
                    if(audioDataConv != NULL){
                        void* conv = nixFmtConverter_create();
                        if(conv != NULL){
                            const NixUI32 srcBlocks = (audioDataBytes / audioDesc.blockAlign);
                            const NixUI32 dstBlocks = samplesReq;
                            NixUI32 ammBlocksRead = 0;
                            NixUI32 ammBlocksWritten = 0;
                            if(!nixFmtConverter_prepare(conv, &audioDesc, &audioDescConv)){
                                PRINTF_ERROR("nixFmtConverter_prepare failed.\n");
                            } else if(!nixFmtConverter_setPtrAtSrcInterlaced(conv, &audioDesc, audioData, 0)){
                                PRINTF_ERROR("nixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                            } else if(!nixFmtConverter_setPtrAtDstInterlaced(conv, &audioDescConv, audioDataConv, 0)){
                                PRINTF_ERROR("nixFmtConverter_setPtrAtDstInterlaced failed.\n");
                            } else if(!nixFmtConverter_convert(conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                                PRINTF_ERROR("nixFmtConverter_convert failed.\n");
                            } else {
                                PRINTF_INFO("nixFmtConverter_convert transformed %u of %u samples (%u%%) (%u hz, %d bits, %d channels) to %u of %u samples(%u%%) (%u hz, %d bits, %d channels, %s).\n", ammBlocksRead, srcBlocks, ammBlocksRead * 100 / srcBlocks, audioDesc.samplerate, audioDesc.bitsPerSample, audioDesc.channels, ammBlocksWritten, dstBlocks, ammBlocksWritten * 100 / dstBlocks, audioDescConv.samplerate, audioDescConv.bitsPerSample, audioDescConv.channels, audioDescConv.samplesFormat == ENNix_sampleFormat_float ? "float" : "int");
                                //iSourceConv
                                NixUI16 iSrcConv = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
                                if(iSrcConv == 0){
                                    PRINTF_ERROR("Source assign failed.\n");
                                } else {
                                    NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDescConv, audioDataConv, ammBlocksWritten * audioDescConv.blockAlign);
                                    if(iBufferWav == 0){
                                        PRINTF_ERROR("Buffer assign failed.\n");
                                    } else {
                                        if(!nixSourceSetBuffer(&nix, iSrcConv, iBufferWav)){
                                            PRINTF_ERROR("Buffer-to-source linking failed.\n");
                                        } else {
                                            iSourceConvPlayCount = 0;
                                            //assign as new source
                                            {
                                                //retain new source
                                                nixSourceRetain(&nix, iSrcConv);
                                                //release previous source
                                                if(iSourceConv != 0){
                                                    nixSourceRelease(&nix, iSourceConv);
                                                    iSourceConv = 0;
                                                }
                                                //assign
                                                iSourceConv = iSrcConv;
                                            }
                                            //
                                            PRINTF_INFO("converted played %u time(s).\n", iSourceConvPlayCount + 1);
                                            nixSourceSetVolume(&nix, iSrcConv, 1.0f);
                                            nixSourcePlay(&nix, iSrcConv);
                                        }
                                        //release buffer (already retained by source if success)
                                        nixBufferRelease(&nix, iBufferWav);
                                        iBufferWav = 0;
                                    }
                                    nixSourceRelease(&nix, iSrcConv);
                                    iSrcConv = 0;
                                }
                            }
                            nixFmtConverter_destroy(conv);
                            conv = NULL;
                        }
                    }
                }
            }
        }
    }
}
