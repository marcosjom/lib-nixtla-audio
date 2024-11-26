//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include <stdio.h>	//printf
#include <stdlib.h>	//malloc, free

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h> //Sleep
	#pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
	#define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
	#include <unistd.h> //sleep, usleep
	#define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

#include "nixtla-audio.h"
#include "utilLoadWav.c"

int main(int argc, const char * argv[]){
	STNix_Engine nix;
	if(nixInit(&nix, 8)){
		NixUI16 iSourceWav = 0; NixUI16 iBufferWav = 0;
		STNix_audioDesc audioDesc;
		NixUI8* audioData = NULL; NixUI32 audioDataBytes;

		if (argc==0) {
			printf("please provide a WAV sound filename\n");
			exit(1);
		}

		const char* strWavPath = argv[1];

		FILE* testWavFile = fopen(strWavPath, "rb");
		if(testWavFile==NULL){
			printf("file %s cannot be read\n");
			exit(1);
		}
		fclose(testWavFile);

		nixPrintCaps(&nix);
		if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
			printf("ERROR, loading WAV file.\n");
		} else {
			printf("WAV file loaded.\n");
			iSourceWav = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
			if(iSourceWav==0){
				printf("Source assign failed.\n");
			} else {
				printf("Source(%d) assigned and retained.\n", iSourceWav);
				iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
				if(iBufferWav==0){
					printf("Buffer assign failed.\n");
				} else {
					printf("Buffer(%d) loaded with data and retained.\n", iBufferWav);
					if(nixSourceSetBuffer(&nix, iSourceWav, iBufferWav)==0){
						printf("Buffer-to-source linking failed.\n");
					} else {
						printf("Buffer(%d) linked with source(%d).\n", iBufferWav, iSourceWav);
						nixSourceSetRepeat(&nix, iSourceWav, 1);
						nixSourceSetVolume(&nix, iSourceWav, 1.0f);
						nixSourcePlay(&nix, iSourceWav);
					}
				}
			}
		}
		if(audioData!=NULL) free(audioData); audioData = NULL;
		//
		//Infinite loop, usually sync with your program main loop, or in a independent thread
		//
		while(1){
			nixTick(&nix);
			DEMO_SLEEP_MILLISEC(1000 / 30); //30 ticks per second for this demo
		}
		//
		if(iSourceWav!=0) nixSourceRelease(&nix, iSourceWav);
		nixFinalize(&nix);
	}
    return 0;
}
