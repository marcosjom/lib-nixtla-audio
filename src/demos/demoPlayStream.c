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

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h> //Sleep
	#pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
	#define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
	#include <unistd.h> //sleep, usleep
	#define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

#include "nixtla-audio.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"

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

//callback

void demoStreamBufferUnqueuedCallback_(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount);

int main(int argc, const char * argv[]){
    STDemoStreamState state;
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
		if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
			printf("ERROR, loading WAV file: '%s'.\n", strWavPath);
		} else {
			printf("WAV file loaded: '%s'.\n", strWavPath);
            state.iSourcePlay = nixSourceAssignStream(&state.nix, NIX_TRUE, 0, NULL, NULL, ammBuffs, demoStreamBufferUnqueuedCallback_, &state);
			if(state.iSourcePlay == 0){
				printf("Source assign failed.\n");
			} else {
				printf("Source(%d) assigned and retained.\n", state.iSourcePlay);
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
                            printf("Buffer assign failed.\n");
                            break;
                        } else {
                            printf("Buffer(%d) loaded with data and retained.\n", state.buffsWav[state.buffsUse]);
                            if(!nixSourceStreamAppendBuffer(&state.nix, state.iSourcePlay, state.buffsWav[state.buffsUse])){
                                printf("Buffer-to-source queueing failed.\n");
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
		//
		//Infinite loop, usually sync with your program main loop, or in a independent thread
		//
        if(state.iSourcePlay > 0){
            const NixUI32 msPerTick = 1000 / 30;
            NixUI32 secsAccum = 0;
            NixUI32 msAccum = 0;
            while(1){
                nixTick(&state.nix);
                DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                msAccum += msPerTick;
                if(msAccum >= 1000){
                    secsAccum += (msAccum / 1000);
                    printf("%u secs playing (%d buffs enqueued, %d dequeued)\n", secsAccum, state.stats.buffQueued, state.stats.buffUnqueued);
                    state.stats.buffQueued = 0;
                    state.stats.buffUnqueued = 0;
                    msAccum %= 1000;
                }
            }
        }
		//
        if(state.iSourcePlay != 0){
            nixSourceRelease(&state.nix, state.iSourcePlay);
            state.iSourcePlay = 0;
        }
		nixFinalize(&state.nix);
	}
    return 0;
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
            printf("Buffer-to-source queueing failed.\n");
        } else {
            state->stats.buffQueued++;
        }
        state->stats.buffUnqueued++;
        --buffToReEnq;
    }
}
