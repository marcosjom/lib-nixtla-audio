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
//In this test, we include the "source" file to implement customized memory allocation.
//The "nixtla-audio.c" file is not part of the project tree.

typedef struct STTestSound {
	STNix_audioDesc audioDesc;
	NixBOOL			isStatic;	//NIX_FALSE for static sound
	NixBOOL			isPlaying;
	NixFLOAT		volume;
};

void printMemReport();
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
		loadAndPlayWav(&nix, "./res/audioTest.wav", &iSourceWav, &iBufferWav);
		//Source for stream eco (play the captured audio)
		iSourceStrm = nixSourceAssignStream(&nix, 1, 0, NULL, NULL, 4, NULL, NULL);
		if(iSourceStrm != 0){
			nixSourcePlay(&nix, iSourceStrm);
			//Init the capture
			audioDesc._canales			= 1;
			audioDesc._bitsPorMuestra	= 16;
			audioDesc._frecuencia		= 22050;
			audioDesc._blockAlign		= (audioDesc._bitsPorMuestra/8) * audioDesc._canales;
			if(nixCaptureInit(&nix, &audioDesc, 15, audioDesc._frecuencia/10, &bufferCapturedCallback, &iSourceStrm)){
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
