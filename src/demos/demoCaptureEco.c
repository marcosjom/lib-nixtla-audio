//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>	//printf
#include <string.h> //memset
#include <stdlib.h> //malloc

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h> //Sleep
	#pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
	#define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
	#include <unistd.h> //sleep, usleep
	#define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

#include "nixtla-audio.h"

#define NIX_DEMO_ECO_RECORD_SECS    5
#define NIX_DEMO_ECO_BUFFS_PER_SEC  10

typedef struct STNixDemoEcoState_ {
    STNix_Engine    nix;
    NixUI16         iSourceStrm;
    //buffs
    struct {
        NixUI16     arr[NIX_DEMO_ECO_RECORD_SECS * NIX_DEMO_ECO_BUFFS_PER_SEC];
        NixUI32     use;
        NixUI32     iFilled;
        NixUI32     iPlayed;
        NixUI32     bytesPerBuffer;
        NixUI32     samplesPerBuffer;
    } buffs;
    //stats
    struct {
        //curSec
        struct {
            NixUI64     samplesRecordedCount;
            NixUI64     buffsPlayedCount;
            //
            NixUI64     samplesRecSum;
            NixUI64     samplesRecCount;
        } curSec;
    } stats;
} STNixDemoEcoState;

//recorder

void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples);

//player

void bufferUnqueuedCallback(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount);

//main

int main(int argc, const char * argv[]){
	//
    STNixDemoEcoState state;
    memset(&state, 0, sizeof(state));
    const NixUI16 ammPregeneratedSources = 0;
	if(nixInit(&state.nix, ammPregeneratedSources)){
		nixPrintCaps(&state.nix);
		//Source for stream eco (play the captured audio)
        const NixUI8 lookIntoReusable   = NIX_TRUE;
        const NixUI8 audioGroupIndex    = 0;
        PTRNIX_SourceReleaseCallback releaseCallBack = NULL;
        void* releaseCallBackUserData   = NULL;
        const NixUI16 buffsQueueSize    = 4;
        PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback = bufferUnqueuedCallback;
        void* bufferUnqueueCallbackData = &state;
        state.iSourceStrm = nixSourceAssignStream(&state.nix, lookIntoReusable, audioGroupIndex, releaseCallBack, releaseCallBackUserData, buffsQueueSize,bufferUnqueueCallback, bufferUnqueueCallbackData);
		if(state.iSourceStrm != 0){
            STNix_audioDesc audioDesc   = STNix_audioDesc_Zero;
            audioDesc.samplesFormat     = ENNix_sampleFormat_int;
            audioDesc.channels          = 1;
            audioDesc.bitsPerSample     = 16;
            audioDesc.samplerate        = 22050;
            audioDesc.blockAlign        = (audioDesc.bitsPerSample / 8) * audioDesc.channels;
            const NixUI16 buffersCount  = (sizeof(state.buffs.arr) / sizeof(state.buffs.arr[0]));
            const NixUI16 samplesPerBuffer = (audioDesc.samplerate / NIX_DEMO_ECO_BUFFS_PER_SEC);
            //allocate buffers
            while(state.buffs.use < (sizeof(state.buffs.arr) / sizeof(state.buffs.arr[0]))){
                NixUI16 iBuffer = nixBufferWithData(&state.nix, &audioDesc, NULL, samplesPerBuffer * audioDesc.blockAlign);
                if(iBuffer == 0){
                    printf("nixBufferWithData failed.\n");
                    break;
                } else {
                    state.buffs.arr[state.buffs.use++] = iBuffer;
                    iBuffer = 0;
                }
            }
            //
            if(state.buffs.use <= 0){
                printf("No stream-buffer could be created\n");
            } else {
                //init the capture
                state.buffs.iFilled = 0;
                state.buffs.iPlayed = state.buffs.use;
                state.buffs.bytesPerBuffer = samplesPerBuffer * audioDesc.blockAlign;
                state.buffs.samplesPerBuffer = samplesPerBuffer;
                //
                nixSourceSetVolume(&state.nix, state.iSourceStrm, 1.0f);
                nixSourcePlay(&state.nix, state.iSourceStrm);
                //
                if(!nixCaptureInit(&state.nix, &audioDesc, buffersCount, samplesPerBuffer, &bufferCapturedCallback, &state)){
                    printf("nixCaptureInit failed.\n");
                } else {
                    if(!nixCaptureStart(&state.nix)){
                        printf("nixCaptureStart failed.\n");
                    } else {
                        const NixUI32 msPerTick = 1000 / 30;
                        NixUI32 msAccum = 0;
                        printf("Capturing and playing audio... switching every %d seconds.\n", NIX_DEMO_ECO_RECORD_SECS);
                        while(1){ //Infinite loop, usually sync with your program main loop, or in a independent thread
                            nixTick(&state.nix);
                            DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                            msAccum += msPerTick;
                            if(msAccum >= 1000){
                                if(state.stats.curSec.samplesRecordedCount > 0){
                                    printf("%llu samples-recorded/sec, %llu buffs-played/sec (recorded avg-value: %d)\n", state.stats.curSec.samplesRecordedCount, state.stats.curSec.buffsPlayedCount, (NixSI32)(state.stats.curSec.samplesRecSum / state.stats.curSec.samplesRecordedCount));
                                } else {
                                    printf("%llu samples-recorded/sec, %llu buffs-played/sec\n", state.stats.curSec.samplesRecordedCount, state.stats.curSec.buffsPlayedCount);
                                }
                                state.stats.curSec.samplesRecordedCount = 0;
                                state.stats.curSec.buffsPlayedCount = 0;
                                msAccum %= 1000;
                            }
                        }
                        nixCaptureStop(&state.nix);
                    }
                    nixCaptureFinalize(&state.nix);
                }
            }
			nixSourceRelease(&state.nix, state.iSourceStrm);
		}
		nixFinalize(&state.nix);
	}
    return 0;
}

//recorder

void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples){
    STNixDemoEcoState* state = (STNixDemoEcoState*)userdata;
    state->stats.curSec.samplesRecordedCount += audioDataSamples;
    //calculate avg (for dbg and stats)
    if(audioDesc.bitsPerSample == 16 && audioDesc.samplesFormat == ENNix_sampleFormat_int){
        const NixSI16* s16 = (const NixSI16*)audioData;
        const NixSI16* s16AfterEnd = s16 + (audioDataBytes / 2);
        state->stats.curSec.samplesRecordedCount += (s16AfterEnd - s16);
        while(s16 < s16AfterEnd){
            state->stats.curSec.samplesRecSum += *s16;
            ++s16;
        }
    }
    //fill buffer
    if(state->buffs.iFilled >= state->buffs.use){
        //ignore samples, probably is the flush after stopping the recording
    } else {
        //fill samples into a buffer
        NixUI16 iBuffer = state->buffs.arr[state->buffs.iFilled];
        if(!nixBufferSetData(eng, iBuffer, &audioDesc, audioData, audioDataBytes)){
            printf("bufferCapturedCallback, nixBufferSetData failed for iSource(%d)\n", state->iSourceStrm);
        } else {
            if(audioDataBytes < state->buffs.bytesPerBuffer){
                //fill remainig space with zeroes
                if(!nixBufferFillZeroes(eng, iBuffer)){
                    printf("bufferCapturedCallback, nixBufferFillZeroes failed for iSource(%d)\n", state->iSourceStrm);
                }
            }
            ++state->buffs.iFilled;
        }
        //start playback
        if(state->buffs.iFilled >= state->buffs.use){
            printf("bufferCapturedCallback, stopping capture, feeding stream\n");
            nixCaptureStop(&state->nix);
            //
            state->buffs.iPlayed = 0;
            //queue all buffers
            {
                NixUI32 iBuff; for(iBuff = 0; iBuff < state->buffs.use; ++iBuff){
                    NixUI16 iBuffer = state->buffs.arr[iBuff];
                    if(!nixSourceStreamAppendBuffer(eng, state->iSourceStrm, iBuffer)){
                        printf("bufferCapturedCallback, ERROR attaching new buffer buffer(%d) to source(%d)\n", iBuffer, state->iSourceStrm);
                    }
                }
            }
        }
    }
}

//player

void bufferUnqueuedCallback(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount){
    STNixDemoEcoState* state = (STNixDemoEcoState*)userdata;
    state->stats.curSec.buffsPlayedCount += buffersUnqueuedCount;
    //
    state->buffs.iPlayed += buffersUnqueuedCount;
    if(state->buffs.iPlayed >= state->buffs.use){
        printf("bufferCapturedCallback, stream played (hungry), starting capture\n");
        //
        state->buffs.iFilled = 0;
        if(!nixCaptureStart(&state->nix)){
            printf("nixCaptureStart failed.\n");
        }
    }
}
