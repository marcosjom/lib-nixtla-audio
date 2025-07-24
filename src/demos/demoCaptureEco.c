//
//  demoCaptureEco.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo creates multiple buffers,
// and switches between recording on them (filling)
// and playing them (stream playback).
//

#include "NixDemosCommon.h"
#include "NixDemoCaptureEco.h"

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
    STNixDemoCaptureEco demo = STNixDemoCaptureEco_Zero;
    if(!NixDemosCommon_init(&common)){
        NIX_PRINTF_ERROR("ERROR, NixDemosCommon_init failed.\n");
        return -1;
    } else {
        if(!NixDemoCaptureEco_init(&demo, &common)){
            NIX_PRINTF_ERROR("ERROR, NixDemoCaptureEco_init failed.\n");
            return -1;
        } else {
            //cycle
            {
                const NixUI32 msPerTick = 1000 / 30;
                NixUI32 secsAccum = 0;
                NixUI32 msAccum = 0;
                while(secsAccum < NIX_DEMO_PLAY_SECS_TO_EXIT){
                    NixDemosCommon_tick(&common, msPerTick);
                    DEMO_SLEEP_MILLISEC(msPerTick); //30 ticks per second for this demo
                    msAccum += msPerTick;
                    if(msAccum >= 1000){
                        secsAccum += (msAccum / 1000);
                        NIX_PRINTF_INFO("%u/%u secs running: %llu buffs played, %lld frames recorded (%lld avg).\n", secsAccum, NIX_DEMO_PLAY_SECS_TO_EXIT, demo.stats.curSec.buffsPlayedCount, demo.stats.curSec.framesRecordedCount, (demo.stats.curSec.avgRec.count == 0 ? 0 : demo.stats.curSec.avgRec.sum / demo.stats.curSec.avgRec.count));
                        memset(&demo.stats.curSec, 0, sizeof(demo.stats.curSec));
                        msAccum %= 1000;
                    }
                }
            }
            NixDemoCaptureEco_destroy(&demo);
        }
        NixDemosCommon_destroy(&common);;
    }
    return 0;
}
