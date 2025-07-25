//
//  nixtla.m
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 06/07/25.
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
//This file adds support for AVFAudio for playing and recording in MacOS and iOS.
//

#ifndef NixtlaAudioLib_nixtla_avfudio_h
#define NixtlaAudioLib_nixtla_avfudio_h

#include "nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

//Provides an interface for AVFAudio.
//By calling this method your final program will require linkage to "AVFAudio.framework".
NixBOOL nixAVAudioEngine_getApiItf(STNixApiItf* dst);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
