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

int main(int argc, const char * argv[]){
    STNix_Engine nix;
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
		if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
			printf("ERROR, loading WAV file: '%s'.\n", strWavPath);
		} else {
			printf("WAV file loaded: '%s'.\n", strWavPath);
            iSourcePlay = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
			if(iSourcePlay == 0){
				printf("Source assign failed.\n");
			} else {
				printf("Source(%d) assigned and retained.\n", iSourcePlay);
                NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
				if(iBufferWav == 0){
					printf("Buffer assign failed.\n");
				} else {
					printf("Buffer(%d) loaded with data and retained.\n", iBufferWav);
					if(!nixSourceSetBuffer(&nix, iSourcePlay, iBufferWav)){
						printf("Buffer-to-source linking failed.\n");
					} else {
						printf("Buffer(%d) linked with source(%d).\n", iBufferWav, iSourcePlay);
						nixSourceSetRepeat(&nix, iSourcePlay, NIX_TRUE);
						nixSourceSetVolume(&nix, iSourcePlay, 1.0f);
						nixSourcePlay(&nix, iSourcePlay);
					}
                    //release buffer (already retained by source if success)
                    nixBufferRelease(&nix, iBufferWav);
                    iBufferWav = 0;
				}
			}
		}
        //wav samples already loaded into buffer
        if(audioData != NULL){
            free(audioData);
            audioData = NULL;
        }
		//
		//Infinite loop, usually sync with your program main loop, or in a independent thread
		//
        if(iSourcePlay > 0){
            const NixUI32 msPerTick = 1000 / 30;
            NixUI32 secsAccum = 0;
            NixUI32 msAccum = 0;
            while(1){
                nixTick(&nix);
                DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                msAccum += msPerTick;
                if(msAccum >= 1000){
                    secsAccum += (msAccum / 1000);
                    printf("%u secs playing\n", secsAccum);
                    msAccum %= 1000;
                }
            }
        }
		//
        if(iSourcePlay != 0){
            nixSourceRelease(&nix, iSourcePlay);
            iSourcePlay = 0;
        }
		nixFinalize(&nix);
	}
    return 0;
}
