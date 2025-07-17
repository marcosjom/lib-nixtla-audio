//
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#ifndef NIXTLA_UTIL_LOAD_WAV_H
#define NIXTLA_UTIL_LOAD_WAV_H

#ifdef __ANDROID__
#   include <jni.h>
#endif

#include "nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

NixUI8 loadDataFromWavFile(
#                           ifdef __ANDROID__
                            JNIEnv *env, jobject assetManager,
#                           endif
                            const char* pathToWav, STNix_audioDesc* audioDesc, NixUI8** audioData, NixUI32* audioDataBytes
                           );

#ifdef __cplusplus
}
#endif

#endif
