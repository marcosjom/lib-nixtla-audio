//
//  NixDemoPlayStream.h
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo loads a WAV's PCM samples into multiple buffers,
// links them to a source and re-links them when returned
// by the callback (buffer-played event).
//

#ifndef NIX_DEMO_PLAY_STREAM_H
#define NIX_DEMO_PLAY_STREAM_H

#include "nixtla-audio.h"
#include "NixDemosCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STNixDemoPlayStream_Zero   { STNixSourceRef_Zero, { 0 , 0 } , 0, 0 }

typedef struct STNixDemoPlayStream_ {
    STNixSourceRef  src;
    //stats
    struct {
        //curSec
        struct {
            NixUI32     buffQueued;
            NixUI32     buffUnqueued;
        } curSec;
    } stats;
    //buffs
    NixUI32         iOldestBuffQueued;
    NixUI32         buffsUse;
    STNixBufferRef  buffs[10];
} STNixDemoPlayStream;

NixBOOL NixDemoPlayStream_init(
                               STNixDemoPlayStream* obj, STNixDemosCommon* common
#                               ifdef __ANDROID__
                               , JNIEnv *env
                               , jobject assetManager
#                               endif
                               );
void    NixDemoPlayStream_destroy(STNixDemoPlayStream* obj);
NixBOOL NixDemoPlayStream_isNull(STNixDemoPlayStream* obj);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
