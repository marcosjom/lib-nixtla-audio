//
//  NixDemosCommon.h
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This class represents a Nixtla's engine to be shared between demos/tests.
// It also uses a custom memory allocation method for memory-leak tracking.
//

#ifndef NIX_DEMOS_COMMON_H
#define NIX_DEMOS_COMMON_H

#include "nixaudio/nixtla-audio.h"
#include "../utils/NixMemMap.h"

#ifdef __ANDROID__
#   include <jni.h>            //for JNIEnv, jobject
#   include <android/log.h>    //for __android_log_print()
#endif

#ifdef __cplusplus
extern "C" {
#endif

//STNixDemosCommon

#define STNixDemosCommon_Zero { STNixContextRef_Zero, STNixEngineRef_Zero }

typedef struct STNixDemosCommon_ {
    STNixContextRef ctx;    // Nixtla's context
    STNixEngineRef  eng;    // Nixtla's engine
    STNixMemMap     memmap; // Custom memory allocation for memory leak detection.
} STNixDemosCommon;

NixBOOL NixDemosCommon_init(STNixDemosCommon* obj);
void    NixDemosCommon_destroy(STNixDemosCommon* obj);
NixBOOL NixDemosCommon_isNull(STNixDemosCommon* obj);
void    NixDemosCommon_tick(STNixDemosCommon* obj, const NixUI32 msPassed);


#if defined(__ANDROID__) //Android
#   define NIX_PRINTF_INFO(STR_FMT, ...)    __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)   __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
#elif defined(__QNX__) //BB10
#   define NIX_PRINTF_INFO(STR_FMT, ...)    fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)   fprintf(stderr, "Nix ERROR, " STR_FMT, ##__VA_ARGS__); fflush(stderr)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) fprintf(stdout, "Nix WARNING, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#else
#   define NIX_PRINTF_INFO(STR_FMT, ...)    printf("Nix, " STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)   printf("Nix ERROR, " STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) printf("Nix WARNING, " STR_FMT, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
} //extern "C"
#endif

#endif
