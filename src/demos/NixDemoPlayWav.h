//
//  NixDemoPlayWav.h
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This demo loads a WAV's PCM samples into one buffer,
// links it to a source in repeat-mode and plays it.
//

#ifndef NIX_DEMO_PLAY_WAV_H
#define NIX_DEMO_PLAY_WAV_H

#include "nixaudio/nixtla-audio.h"
#include "NixDemosCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STNixDemoPlayWav_Zero   { STNixSourceRef_Zero }

typedef struct STNixDemoPlayWav_ {
    STNixSourceRef  src;
} STNixDemoPlayWav;

NixBOOL NixDemoPlayWav_init(
                            STNixDemoPlayWav* obj, STNixDemosCommon* common
#                           ifdef __ANDROID__
                            , JNIEnv *env
                            , jobject assetManager
#                           endif
                            );
void    NixDemoPlayWav_destroy(STNixDemoPlayWav* obj);
NixBOOL NixDemoPlayWav_isNull(STNixDemoPlayWav* obj);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
