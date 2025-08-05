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

#define NIX_DEBUG
//#define NIX_SILENT_MODE
//#define NIX_VERBOSE_MODE

//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++
//++ PRIVATE HEADER ++
//++++++++++++++++++++
//++++++++++++++++++++
//++++++++++++++++++++

#ifdef NIX_DEBUG
#   define NIX_ASSERTS_ACTIVATED
#endif

#ifdef NIX_ASSERTS_ACTIVATED
    #include <assert.h>         //assert
#   define NIX_ASSERT(EVAL)     { if(!(EVAL)){ NIX_PRINTF_ERROR("ASSERT, cond '"#EVAL"'.\n"); NIX_PRINTF_ERROR("ASSERT, file '%s'\n", __FILE__); NIX_PRINTF_ERROR("ASSERT, line %d.\n", __LINE__); assert(0); }}
#else
#   define NIX_ASSERT(EVAL)     ((void)0);
#endif

#ifndef NIX_MSWAIT_BEFORE_DELETING_BUFFERS
    #ifdef NIX_OPENAL
#        define NIX_MSWAIT_BEFORE_DELETING_BUFFERS    250 //OpenAL fails when stopping, unqueuing and deleting buffers in the same tick
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
#   include <android/log.h>
#   define NIX_PRINTF_ALLWAYS(STR_FMT, ...)     __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
#elif defined(__QNX__) //BB10
#   include <stdio.h>
#   define NIX_PRINTF_ALLWAYS(STR_FMT, ...)     fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#else
#   include <stdio.h>
#   define NIX_PRINTF_ALLWAYS(STR_FMT, ...)     printf("Nix, " STR_FMT, ##__VA_ARGS__)
#endif

#if defined(NIX_SILENT_MODE)
#   define NIX_PRINTF_INFO(STR_FMT, ...)        ((void)0)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)       ((void)0)
#   define NIX_PRINTF_WARNING(STR_FMT, ...)     ((void)0)
#else
#   if defined(__ANDROID__) //Android
#        ifndef NIX_VERBOSE_MODE
#        define NIX_PRINTF_INFO(STR_FMT, ...)   ((void)0)
#        else
#        define NIX_PRINTF_INFO(STR_FMT, ...)   __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
#        endif
#        define NIX_PRINTF_ERROR(STR_FMT, ...)  __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
#        define NIX_PRINTF_WARNING(STR_FMT, ...) __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
#   elif defined(__QNX__) //BB10
#        ifndef NIX_VERBOSE_MODE
#        define NIX_PRINTF_INFO(STR_FMT, ...)   ((void)0)
#        else
#        define NIX_PRINTF_INFO(STR_FMT, ...)   fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#        endif
#        define NIX_PRINTF_ERROR(STR_FMT, ...)  fprintf(stderr, "Nix ERROR, " STR_FMT, ##__VA_ARGS__); fflush(stderr)
#        define NIX_PRINTF_WARNING(STR_FMT, ...) fprintf(stdout, "Nix WARNING, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#   else
#        ifndef NIX_VERBOSE_MODE
#        define NIX_PRINTF_INFO(STR_FMT, ...)   ((void)0)
#        else
#        define NIX_PRINTF_INFO(STR_FMT, ...)   printf("Nix, " STR_FMT, ##__VA_ARGS__)
#        endif
#       define NIX_PRINTF_ERROR(STR_FMT, ...)   printf("Nix ERROR, " STR_FMT, ##__VA_ARGS__)
#       define NIX_PRINTF_WARNING(STR_FMT, ...) printf("Nix WARNING, " STR_FMT, ##__VA_ARGS__)
#   endif
#endif

#include "nixaudio/nixtla-audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
