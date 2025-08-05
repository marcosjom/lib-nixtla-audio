//
//  nixtla-aaudio.c
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 12/07/25.
//  Copyright (c) 2025 Marcos Ortega. All rights reserved.
//
//  This entire notice must be retained in this source code.
//  This source code is under MIT Licence.
//
//  This software is provided "as is", with absolutely no warranty expressed
//  or implied. Any use is at your own risk.
//
//  Latest fixes enhancements and documentation at https://github.com/marcosjom/lib-nixtla-audio
//

//
//This file adds support for OpenAL for playing and recording.
//

#ifndef NixtlaAudioLib_nixtla_openal_h
#define NixtlaAudioLib_nixtla_openal_h

#include "nixaudio/nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

//Provides an interface for OpenAL.
//By calling this method your final app will require linkage to "OpenAL.framework" or "openal".
NixBOOL nixOpenALEngine_getApiItf(STNixApiItf* dst);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
