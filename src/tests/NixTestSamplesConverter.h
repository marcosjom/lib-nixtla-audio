//
//  NixTestSamplesConverter.h
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This test loads a WAV's PCM samples, and switches between playing
// the original sound and playing a randomly converted version of it.
//

#ifndef NIX_TEST_SAMPLES_CONVERTER_H
#define NIX_TEST_SAMPLES_CONVERTER_H

#include "nixaudio/nixtla-audio.h"
#include "../demos/NixDemosCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STNixTestSamplesConverter_Zero   { { STNixSourceRef_Zero, 0, NULL, 0, STNixAudioDesc_Zero }, { STNixSourceRef_Zero, 0, NULL, 0, STNixAudioDesc_Zero } }

typedef struct STNixTestSamplesConverter_ {
    //org
    struct {
        STNixSourceRef  src;
        NixUI16         playCount;
        NixUI8*         data;
        NixUI32         dataSz;
        STNixAudioDesc  audioDesc;
    } org;
    //conv (converted)
    struct {
        STNixSourceRef  src;
        NixUI16         playCount;
        NixUI8*         data;
        NixUI32         dataSz;
        STNixAudioDesc  audioDesc;
    } conv;
} STNixTestSamplesConverter;

NixBOOL NixTestSamplesConverter_init(
                                     STNixTestSamplesConverter* obj, STNixDemosCommon* common
#                                    ifdef __ANDROID__
                                     , JNIEnv *env
                                     , jobject assetManager
#                                    endif
                                     );
void    NixTestSamplesConverter_destroy(STNixTestSamplesConverter* obj, STNixDemosCommon* common);
NixBOOL NixTestSamplesConverter_isNull(STNixTestSamplesConverter* obj);
void    NixTestSamplesConverter_tick(STNixTestSamplesConverter* obj, STNixDemosCommon* common);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
