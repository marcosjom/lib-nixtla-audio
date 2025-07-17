//
//  main.c
//  nixtla-audio-demos
//
//  Created by Marcos Ortega on 24/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>		//printf
#include <stdlib.h>		//malloc, free
#include "testMemMap.h"

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h> //Sleep
	#define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
	#pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
#else
	#include <unistd.h> //sleep, usleep
	#define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

// Custom memory allocation for this test,
// for detecting memory-leaks using a STNB_MemMap.
STNB_MemMap memmap;
#define NIX_MALLOC(POINTER_DEST, POINTER_TYPE, SIZE_BYTES, STR_HINT) \
	{ \
		POINTER_DEST = (POINTER_TYPE*)malloc(SIZE_BYTES); \
		nbMemmapRegister(&memmap, POINTER_DEST, SIZE_BYTES, STR_HINT); \
	}
#define NIX_FREE(POINTER)	\
	{ \
		free(POINTER); \
		nbMemmapUnregister(&memmap, POINTER); \
	}
#include "nixtla-audio.h"
#include "../nixtla-audio.c"

//In this demo, we include the "source" file to implement customized memory allocation.
//The "nixtla-audio.c" file is not part of the project tree.

#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"

void printMemReport();
void loadAndPlayWav(STNix_Engine* nix, const char* strWavPath, NixUI16* iSourceWav, NixUI16* iBufferWav);
void bufferCapturedCallback(STNix_Engine* nix, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples);

int main(int argc, const char * argv[]) {
	//
	STNix_Engine nix;
	nbMemmapInit(&memmap);
	if(nixInit(&nix, 8)){
		STNix_audioDesc audioDesc; NixUI16 iSourceStrm;
		nixPrintCaps(&nix);
		//Load and play wav file
		NixUI16 iSourceWav = 0; NixUI16 iBufferWav = 0;
        //randomly select a wav from the list
        const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
		loadAndPlayWav(&nix, strWavPath, &iSourceWav, &iBufferWav);
		//Source for stream eco (play the captured audio)
		iSourceStrm = nixSourceAssignStream(&nix, 1, 0, NULL, NULL, 4, NULL, NULL);
		if(iSourceStrm != 0){
			nixSourcePlay(&nix, iSourceStrm);
			//Init the capture
			audioDesc.samplesFormat		= ENNix_sampleFormat_int;
			audioDesc.channels			= 1;
			audioDesc.bitsPerSample		= 16;
			audioDesc.samplerate		= 22050;
			audioDesc.blockAlign		= (audioDesc.bitsPerSample/8) * audioDesc.channels;
			if(nixCaptureInit(&nix, &audioDesc, 15, audioDesc.samplerate/10, &bufferCapturedCallback, &iSourceStrm)){
				if(nixCaptureStart(&nix)){
					NixUI32 msSleep		= (1000 / 30);
					NixUI32 msAcum		= 0;
					printf("Capturing and playing audio...\n");
					while(msAcum < 5000){ //capture and play for 5 secodns
						nixTick(&nix);
						DEMO_SLEEP_MILLISEC(msSleep); //30 ticks per second for this demo
						msAcum			+= msSleep;
					}
					nixCaptureStop(&nix);
				}
				nixCaptureFinalize(&nix);
			}
			nixSourceRelease(&nix, iSourceStrm);
		}
		nixFinalize(&nix);
	}
	//Memory report
	printMemReport();
	//
	nbMemmapFinalize(&memmap);
	//
	{
		char c;
		printf("Press ENTER to exit.");
		scanf("%c", &c);
	}
    return 0;
}

void printMemReport(){
	printf("-------------- MEM REPORT -----------\n");
	if(memmap.currCountAllocationsActive == 0){
		printf("Nixtla: no memory leaking detected :)\n");
	} else {
		printf("WARNING, NIXTLA MEMORY-LEAK DETECTED! :(\n");
	}
	nbMemmapPrintActive(&memmap);
	printf("-------------------------------------\n");
}

void loadAndPlayWav(STNix_Engine* nix, const char* strWavPath, NixUI16* p_iSourceWav, NixUI16* p_iBufferWav){
	STNix_audioDesc audioDesc;
	NixUI16 iSourceWav = 0, iBufferWav = 0;
	NixUI8* audioData = NULL;
    NixUI32 audioDataBytes = 0;
	if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
		printf("ERROR, loading WAV file: '%s'.\n", strWavPath);
	} else {
		printf("WAV file loaded: '%s'.\n", strWavPath);
		iSourceWav = nixSourceAssignStatic(nix, 1, 0, NULL, NULL);
		if(iSourceWav == 0){
			printf("Source assign failed.\n");
		} else {
			printf("Source(%d) assigned and retained.\n", iSourceWav);
			iBufferWav = nixBufferWithData(nix, &audioDesc, audioData, audioDataBytes);
			if(iBufferWav == 0){
				printf("Buffer assign failed.\n");
			} else {
				printf("Buffer(%d) loaded with data and retained.\n", iBufferWav);
				if(!nixSourceSetBuffer(nix, iSourceWav, iBufferWav)){
					printf("Buffer-to-source linking failed.\n");
				} else {
					printf("Buffer(%d) linked with source(%d).\n", iBufferWav, iSourceWav);
					nixSourceSetRepeat(nix, iSourceWav, NIX_TRUE);
					nixSourceSetVolume(nix, iSourceWav, 1.0f);
					nixSourcePlay(nix, iSourceWav);
				}
			}
		}
	}
	if(audioData!=NULL) free(audioData); audioData = NULL;
	*p_iSourceWav = iSourceWav;
	*p_iBufferWav = iBufferWav;
}

void bufferCapturedCallback(STNix_Engine* nix, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples){
	const NixUI16 iSource = *((NixUI16*)userdata);
	const NixUI16 iBuffer = nixBufferWithData(nix, &audioDesc, audioData, audioDataBytes);
	if(iBuffer == 0){
		printf("bufferCapturedCallback, nixBufferWithData failed for iSource(%d)\n", iSource);
	} else {
		if(nixSourceStreamAppendBuffer(nix, iSource, iBuffer)){
			//PRINTF_INFO("bufferCapturedCallback, source(%d) new buffer(%d) attached\n", iSource, iBuffer);
		} else {
			printf("bufferCapturedCallback, ERROR attaching new buffer buffer(%d) to source(%d)\n", iBuffer, iSource);
		}
		nixBufferRelease(nix, iBuffer);
	}
}
