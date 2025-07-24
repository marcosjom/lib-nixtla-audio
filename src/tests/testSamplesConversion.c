//
//  testSamplesConversion.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This test loads a WAV's PCM samples, and switches between playing
// the original sound and playing a randomly converted version of it.
//

#include "NixDemosCommon.h"
#include "NixTestSamplesConverter.h"

#include <stdio.h>  //printf
#include <stdlib.h> //malloc, free, rand
#include <time.h>   //time
#include <string.h> //memset

#define NIX_DEMO_PLAY_SECS_TO_EXIT    60    //stop the demo after this time

//sleep function
#if defined(_WIN32) || defined(WIN32)
    #include <windows.h> //Sleep
    #pragma comment(lib, "OpenAL32.lib") //link to openAL's lib
    #define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
    #include <unistd.h> //sleep, usleep
    #define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

int main(int argc, const char * argv[]){
    STNixDemosCommon common = STNixDemosCommon_Zero;
    STNixTestSamplesConverter demo = STNixTestSamplesConverter_Zero;
    if(!NixDemosCommon_init(&common)){
        printf("ERROR, NixDemosCommon_init failed.\n");
        return -1;
    } else {
        if(!NixTestSamplesConverter_init(&demo, &common)){
            printf("ERROR, NixTestSamplesConverter_init failed.\n");
            return -1;
        } else {
            //cycle
            {
                const NixUI32 msPerTick = 1000 / 30;
                NixUI32 secsAccum = 0;
                NixUI32 msAccum = 0;
                while(secsAccum < NIX_DEMO_PLAY_SECS_TO_EXIT){
                    NixDemosCommon_tick(&common, msPerTick);
                    NixTestSamplesConverter_tick(&demo, &common);
                    DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                    msAccum += msPerTick;
                    if(msAccum >= 1000){
                        secsAccum += (msAccum / 1000);
                        printf("%u/%u secs running.\n", secsAccum, NIX_DEMO_PLAY_SECS_TO_EXIT);
                        msAccum %= 1000;
                    }
                }
            }
            NixTestSamplesConverter_destroy(&demo, &common);
        }
        NixDemosCommon_destroy(&common);;
    }
    return 0;
}
