//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>	//printf
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

typedef struct STDemoCallbackParam_ {
    NixUI16 iSourceStrm;
    NixUI64 samplesRecordedCount;
    NixUI64 buffsPlayedCount;
} STDemoCallbackParam;

//recorder
void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples);

//player
void bufferUnqueuedCallback(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount);

int main(int argc, const char * argv[]){
	//
	STNix_Engine nix;
    const NixUI16 ammPregeneratedSources = 0;
	if(nixInit(&nix, ammPregeneratedSources)){
		nixPrintCaps(&nix);
        STDemoCallbackParam callbackParam;
        memset(&callbackParam, 0, sizeof(callbackParam));
		//Source for stream eco (play the captured audio)
        const NixUI8 lookIntoReusable   = NIX_TRUE;
        const NixUI8 audioGroupIndex    = 0;
        PTRNIX_SourceReleaseCallback releaseCallBack = NULL;
        void* releaseCallBackUserData   = NULL;
        const NixUI16 buffsQueueSize    = 4;
        PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback = bufferUnqueuedCallback;
        void* bufferUnqueueCallbackData = &callbackParam;
        NixUI16 iSourceStrm = nixSourceAssignStream(&nix, lookIntoReusable, audioGroupIndex, releaseCallBack, releaseCallBackUserData, buffsQueueSize,bufferUnqueueCallback, bufferUnqueueCallbackData);
		if(iSourceStrm != 0){
            callbackParam.iSourceStrm = iSourceStrm;
            nixSourceSetVolume(&nix, iSourceStrm, 1.0f);
            nixSourcePlay(&nix, iSourceStrm);
			//Init the capture
            STNix_audioDesc audioDesc;
            memset(&audioDesc, 0, sizeof(audioDesc));
			audioDesc.samplesFormat		= ENNix_sampleFormat_int;
			audioDesc.channels			= 1;
			audioDesc.bitsPerSample		= 16;
			audioDesc.samplerate		= 22050;
			audioDesc.blockAlign		= (audioDesc.bitsPerSample/8) * audioDesc.channels;
            const NixUI16 buffersCount = 15;
            const NixUI16 samplesPerBuffer = (audioDesc.samplerate / 10);
			if(nixCaptureInit(&nix, &audioDesc, buffersCount, samplesPerBuffer, &bufferCapturedCallback, &callbackParam)){
				if(nixCaptureStart(&nix)){
                    const NixUI32 msPerTick = 1000 / 30;
                    NixUI32 msAccum = 0;
					printf("Capturing and playing audio...\n");
					while(1){ //Infinite loop, usually sync with your program main loop, or in a independent thread
						nixTick(&nix);
						DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                        msAccum += msPerTick;
                        if(msAccum >= 1000){
                            printf("%llu samples-recorded/sec, %llu buffs-played/sec\n", callbackParam.samplesRecordedCount, callbackParam.buffsPlayedCount);
                            callbackParam.samplesRecordedCount = 0;
                            callbackParam.buffsPlayedCount = 0;
                            msAccum %= 1000;
                        }
					}
					nixCaptureStop(&nix);
				}
				nixCaptureFinalize(&nix);
			}
			nixSourceRelease(&nix, iSourceStrm);
		}
		nixFinalize(&nix);
	}
    return 0;
}

//recorder

void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples){
    STDemoCallbackParam* param = (STDemoCallbackParam*)userdata;
	const NixUI16 iSource = param->iSourceStrm;
	const NixUI16 iBuffer = nixBufferWithData(eng, &audioDesc, audioData, audioDataBytes);
	if(iBuffer == 0){
		printf("bufferCapturedCallback, nixBufferWithData failed for iSource(%d)\n", iSource);
	} else {
        param->samplesRecordedCount += audioDataSamples;
        if(!nixSourceStreamAppendBuffer(eng, iSource, iBuffer)){
            printf("bufferCapturedCallback, ERROR attaching new buffer buffer(%d) to source(%d)\n", iBuffer, iSource);
        } else {
            if(!nixSourceIsPlaying(eng, iSource)){
                printf("bufferCapturedCallback, iSource(%d) was not playing\n", iSource);
                nixSourcePlay(eng, iSource);
            }
            //printf("bufferCapturedCallback, source(%d) new buffer(%d) attached\n", iSource, iBuffer);
		}
		nixBufferRelease(eng, iBuffer);
	}
}

//player

void bufferUnqueuedCallback(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount){
    STDemoCallbackParam* param = (STDemoCallbackParam*)userdata;
    param->buffsPlayedCount += buffersUnqueuedCount;
}
