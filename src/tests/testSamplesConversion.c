//
//  main.c
//  nixtla-audio-demos
//
//  Created by Marcos Ortega on 10/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#include <stdio.h>  //printf
#include <stdlib.h> //malloc, free
#include <string.h> //memset

#if defined(_WIN32) || defined(WIN32)
    #include <windows.h> //Sleep
    #pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
    #define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
    #include <unistd.h> //sleep, usleep
    #define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

#include "nixtla-audio.h"
#include "../utils/utilLoadWav.c"

int main(int argc, const char * argv[]){
    STNix_Engine nix;
    if(nixInit(&nix, 8)){
        nixPrintCaps(&nix);
        const char* strWavPath = "./res/beat_stereo_16_22050.wav";
        NixUI16 iSourceOrg = 0, iSourceOrgPlayCount = 0;
        NixUI8* audioData = NULL;
        NixUI32 audioDataBytes = 0;
        STNix_audioDesc audioDesc;
        //
        NixUI16 iSourceConv = 0, iSourceConvPlayCount = 0;
        NixUI8* audioDataConv = NULL;
        NixUI32 audioDataBytesConv = 0;
        STNix_audioDesc audioDescConv;
        //
        if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
            printf("ERROR, loading WAV file.\n");
        } else {
            printf("WAV file loaded.\n");
            iSourceOrg = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
            if(iSourceOrg == 0){
                printf("Source assign failed.\n");
            } else {
                NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
                if(iBufferWav == 0){
                    printf("Buffer assign failed.\n");
                } else {
                    if(!nixSourceSetBuffer(&nix, iSourceOrg, iBufferWav)){
                        printf("Buffer-to-source linking failed.\n");
                    } else {
                        printf("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
                        nixSourceSetVolume(&nix, iSourceOrg, 1.0f);
                        nixSourcePlay(&nix, iSourceOrg);
                    }
                }
            }
        }
        //
        //Infinite loop, usually sync with your program main loop, or in a independent thread
        //
        if(iSourceOrg > 0){
            const NixUI32 msPerTick = 1000 / 30;
            NixUI32 secsAccum = 0;
            NixUI32 msAccum = 0;
            while(1){
                nixTick(&nix);
                DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                msAccum += msPerTick;
                if(msAccum >= 1000){
                    secsAccum += (msAccum / 1000);
                    //printf("%u secs playing\n", secsAccum);
                    msAccum %= 1000;
                }
                //analyze converted playback
                if(iSourceConv != 0){
                    if(!nixSourceIsPlaying(&nix, iSourceConv)){
                        iSourceConvPlayCount++;
                        if(iSourceConvPlayCount < 2){
                            //play again
                            printf("converted played %u time(s).\n", iSourceConvPlayCount + 1);
                            nixSourcePlay(&nix, iSourceConv);
                        } else {
                            iSourceConvPlayCount = 0;
                            //release
                            nixSourceRelease(&nix, iSourceConv);
                            iSourceConv = 0;
                            //play original
                            iSourceOrgPlayCount = 0;
                            printf("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
                            nixSourcePlay(&nix, iSourceOrg);
                        }
                    }
                } else {
                    //analyze original playback
                    if(!nixSourceIsPlaying(&nix, iSourceOrg)){
                        iSourceOrgPlayCount++;
                        if(iSourceOrgPlayCount < 2){
                            //play again
                            printf("origin playing %u time(s).\n", iSourceOrgPlayCount + 1);
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
                            const NixUI32 samplesReq = nixFmtConverter_samplesForNewFrequency(audioDesc.samplerate, audioDescConv.samplerate, audioDataBytes / audioDesc.blockAlign);
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
                                    printf("Conversion buffer resized to %u bytes.\n", audioDataBytesConv);
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
                                        printf("nixFmtConverter_prepare failed.\n");
                                    } else if(!nixFmtConverter_setPtrAtSrcInterlaced(conv, &audioDesc, audioData, 0)){
                                        printf("nixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                                    } else if(!nixFmtConverter_setPtrAtDstInterlaced(conv, &audioDescConv, audioDataConv, 0)){
                                        printf("nixFmtConverter_setPtrAtDstInterlaced failed.\n");
                                    } else if(!nixFmtConverter_convert(conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                                        printf("nixFmtConverter_convert failed.\n");
                                    } else {
                                        printf("nixFmtConverter_convert transformed %u of %u samples (%u%%) (%u hz, %d bits, %d channels) to %u of %u samples(%u%%) (%u hz, %d bits, %d channels, %s).\n", ammBlocksRead, srcBlocks, ammBlocksRead * 100 / srcBlocks, audioDesc.samplerate, audioDesc.bitsPerSample, audioDesc.channels, ammBlocksWritten, dstBlocks, ammBlocksWritten * 100 / dstBlocks, audioDescConv.samplerate, audioDescConv.bitsPerSample, audioDescConv.channels, audioDescConv.samplesFormat == ENNix_sampleFormat_float ? "float" : "int");
                                        //iSourceConv
                                        NixUI16 iSrcConv = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
                                        if(iSrcConv == 0){
                                            printf("Source assign failed.\n");
                                        } else {
                                            NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDescConv, audioDataConv, ammBlocksWritten * audioDescConv.blockAlign);
                                            if(iBufferWav == 0){
                                                printf("Buffer assign failed.\n");
                                            } else {
                                                if(!nixSourceSetBuffer(&nix, iSrcConv, iBufferWav)){
                                                    printf("Buffer-to-source linking failed.\n");
                                                } else {
                                                    iSourceConvPlayCount = 0;
                                                    printf("converted played %u time(s).\n", iSourceConvPlayCount + 1);
                                                    nixSourceSetVolume(&nix, iSrcConv, 1.0f);
                                                    nixSourcePlay(&nix, iSrcConv);
                                                    nixSourceRetain(&nix, iSrcConv);
                                                    if(iSourceConv != 0){
                                                        nixSourceRelease(&nix, iSourceConv);
                                                        iSourceConv = 0;
                                                    }
                                                    iSourceConv = iSrcConv;
                                                }
                                            }
                                            nixSourceRelease(&nix, iSrcConv);
                                            iSrcConv = 0;
                                        }
                                    }
                                    nixFmtConverter_destroy(conv);
                                }
                            }
                        }
                    }
                }
            }
        }
        //
        if(audioData!=NULL){
            free(audioData);
            audioData = NULL;
        }
        if(iSourceOrg != 0){
            nixSourceRelease(&nix, iSourceOrg);
            iSourceOrg = 0;
        }
        if(audioDataConv != NULL){
            free(audioDataConv);
            audioDataConv = NULL;
        }
        if(iSourceConv != 0){
            nixSourceRelease(&nix, iSourceConv);
            iSourceConv = 0;
        }
        nixFinalize(&nix);
    }
    return 0;
}
