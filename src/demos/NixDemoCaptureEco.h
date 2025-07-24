//
//  NixDemoCaptureEco.h
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

#ifndef NIX_DEMO_CAPTURE_ECO_H
#define NIX_DEMO_CAPTURE_ECO_H

#include "nixtla-audio.h"
#include "NixDemosCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NIX_DEMO_ECO_RECORD_SECS    5   //ammount of seconds to capture audio, the play it back
#define NIX_DEMO_ECO_BUFFS_PER_SEC  10  //ammount of buffers per second

#define STNixDemoCaptureEco_Zero   { STNixSourceRef_Zero, STNixRecorderRef_Zero, { { 0, 0, 0, 0 } } , { 0, 0, 0, 0, 0 } }

typedef struct STNixDemoCaptureEco_ {
    STNixSourceRef  src;
    STNixRecorderRef rec;
    //stats
    struct {
        //curSec
        struct {
            NixUI64     framesRecordedCount;
            NixUI64     buffsPlayedCount;
            //avg
            struct {
                NixSI64 sum;
                NixSI64 count;
            } avgRec;
        } curSec;
    } stats;
    //buffs
    struct {
        NixUI32     use;
        NixUI32     iFilled;
        NixUI32     iPlayed;
        NixUI32     bytesPerBuffer;
        NixUI32     blocksPerBuffer;
        STNixBufferRef arr[NIX_DEMO_ECO_RECORD_SECS * NIX_DEMO_ECO_BUFFS_PER_SEC];
    } buffs;
} STNixDemoCaptureEco;

NixBOOL NixDemoCaptureEco_init(STNixDemoCaptureEco* obj, STNixDemosCommon* common);
void    NixDemoCaptureEco_destroy(STNixDemoCaptureEco* obj);
NixBOOL NixDemoCaptureEco_isNull(STNixDemoCaptureEco* obj);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
