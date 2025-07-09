//
//  nixtla.h
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

#ifndef NixtlaAudioLib_nixtla_private_h
#define NixtlaAudioLib_nixtla_private_h

#define NIX_ASSERTS_ACTIVATED
//#define NIX_SILENT_MODE
//#define NIX_VERBOSE_MODE

//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++
//++ PRIVATE HEADER ++
//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++

#ifdef NIX_ASSERTS_ACTIVATED
    #include <assert.h>         //assert
#   define NIX_ASSERT(EVAL)     { if(!(EVAL)){ NIX_PRINTF_ERROR("ASSERT, cond '"#EVAL"'.\n"); NIX_PRINTF_ERROR("ASSERT, file '%s'\n", __FILE__); NIX_PRINTF_ERROR("ASSERT, line %d.\n", __LINE__); assert(0); }}
#else
#   define NIX_ASSERT(EVAL)     ((void)0);
#endif

//
// You can custom memory management by defining this MACROS
// and CONSTANTS before this file get included or compiled.
//
// This are the default memory management MACROS and CONSTANTS:
#if !defined(NIX_MALLOC) || !defined(NIX_FREE)
    #include <stdlib.h>        //malloc, free
    #ifndef NIX_MALLOC
        #define NIX_MALLOC(POINTER_DEST, POINTER_TYPE, SIZE_BYTES, STR_HINT) POINTER_DEST = (POINTER_TYPE*)malloc(SIZE_BYTES)
    #endif
    #ifndef NIX_FREE
        #define NIX_FREE(POINTER) free(POINTER)
    #endif
#endif

#ifndef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
    #ifdef NIX_OPENAL
        #define NIX_MSWAIT_BEFORE_DELETING_BUFFERS    250 //For some reason OpenAL fails when stopping, unqueuing and deleting buffers in the same tick
    #endif
#endif

#ifndef NIX_SOURCES_MAX
    #define NIX_SOURCES_MAX        0xFFFF
#endif

#ifndef NIX_SOURCES_GROWTH
    #define NIX_SOURCES_GROWTH    1
#endif

#ifndef NIX_BUFFERS_MAX
    #define NIX_BUFFERS_MAX        0xFFFF
#endif

#ifndef NIX_BUFFERS_GROWTH
    #define NIX_BUFFERS_GROWTH    1
#endif

#ifndef NIX_AUDIO_GROUPS_SIZE
    #define NIX_AUDIO_GROUPS_SIZE 8
#endif

// PRINTING/LOG

#if defined(__ANDROID__) //Android
    #include <android/log.h>
    #define NIX_PRINTF_ALLWAYS(STR_FMT, ...)        __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
#elif defined(__QNX__) //BB10
    #define NIX_PRINTF_ALLWAYS(STR_FMT, ...)        fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#else
    #define NIX_PRINTF_ALLWAYS(STR_FMT, ...)        printf("Nix, " STR_FMT, ##__VA_ARGS__)
#endif

#if defined(NIX_SILENT_MODE)
    #define NIX_PRINTF_INFO(STR_FMT, ...)           ((void)0)
    #define NIX_PRINTF_ERROR(STR_FMT, ...)          ((void)0)
    #define NIX_PRINTF_WARNING(STR_FMT, ...)        ((void)0)
#else
    #if defined(__ANDROID__) //Android
        #include <android/log.h>
        #ifndef NIX_VERBOSE_MODE
        #define NIX_PRINTF_INFO(STR_FMT, ...)        ((void)0)
        #else
        #define NIX_PRINTF_INFO(STR_FMT, ...)        __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
        #endif
        #define NIX_PRINTF_ERROR(STR_FMT, ...)        __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
        #define NIX_PRINTF_WARNING(STR_FMT, ...)    __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
    #elif defined(__QNX__) //BB10
        #ifndef NIX_VERBOSE_MODE
        #define NIX_PRINTF_INFO(STR_FMT, ...)        ((void)0)
        #else
        #define NIX_PRINTF_INFO(STR_FMT, ...)        fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
        #endif
        #define NIX_PRINTF_ERROR(STR_FMT, ...)        fprintf(stderr, "Nix ERROR, " STR_FMT, ##__VA_ARGS__); fflush(stderr)
        #define NIX_PRINTF_WARNING(STR_FMT, ...)    fprintf(stdout, "Nix WARNING, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
    #else
        #ifndef NIX_VERBOSE_MODE
        #define NIX_PRINTF_INFO(STR_FMT, ...)        ((void)0)
        #else
        #define NIX_PRINTF_INFO(STR_FMT, ...)        printf("Nix, " STR_FMT, ##__VA_ARGS__)
        #endif
        #define NIX_PRINTF_ERROR(STR_FMT, ...)        printf("Nix ERROR, " STR_FMT, ##__VA_ARGS__)
        #define NIX_PRINTF_WARNING(STR_FMT, ...)    printf("Nix WARNING, " STR_FMT, ##__VA_ARGS__)
    #endif
#endif

#include "nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

//AVFAudio interface to objective-c (iOS/MacOS)
void*       nixAVAudioEngine_create(void);
void        nixAVAudioEngine_destroy(void* obj);
//
void*       nixAVAudioSource_create(void* eng);
void        nixAVAudioSource_destroy(void* obj);
NixBOOL     nixAVAudioSource_setVolume(void* obj, const float vol);
NixBOOL     nixAVAudioSource_setRepeat(void* obj, const NixBOOL isRepeat);
void        nixAVAudioSource_play(void* obj);
void        nixAVAudioSource_pause(void* obj);
void        nixAVAudioSource_stop(void* obj);
NixBOOL     nixAVAudioSource_isPlaying(void* obj);
NixBOOL     nixAVAudioSource_isPaused(void* obj);
NixBOOL     nixAVAudioSource_setBuffer(void* obj, void* buff, void (*callback)(void* pEng, NixUI32 sourceIndex), void* callbackEng, NixUI32 callbackSourceIndex);  //static-source
NixBOOL     nixAVAudioSource_queueBuffer(void* obj, void* buff, void (*callback)(void* pEng, NixUI32 sourceIndex), void* pEng, NixUI32 sourceIndex); //stream-source
//
void*       nixAVAudioPCMBuffer_create(const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
void        nixAVAudioPCMBuffer_destroy(void* obj);
NixBOOL     nixAVAudioPCMBuffer_setData(void* obj, const STNix_audioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);

#ifdef __cplusplus
}
#endif

#endif
