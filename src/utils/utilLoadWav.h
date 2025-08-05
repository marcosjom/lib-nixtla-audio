//
//  Nixtla
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#ifndef NIXTLA_UTIL_LOAD_WAV_H
#define NIXTLA_UTIL_LOAD_WAV_H

#ifdef __ANDROID__
#   include <jni.h>
#endif

#include "nixaudio/nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

NixUI8 nixUtilLoadDataFromWavFile(
#                           ifdef __ANDROID__
                                  JNIEnv *env, jobject assetManager,
#                           endif
                                  const char* pathToWav, STNixAudioDesc* audioDesc, NixUI8** audioData, NixUI32* audioDataBytes
                                  );

#ifdef __cplusplus
} //extern "C"
#endif

#endif
