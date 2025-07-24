//
//  NixMemMap.h
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#ifndef NIX_UTILS_MEM_MAP_H
#define NIX_UTILS_MEM_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

//STNixDbgStr

typedef struct STNixDbgStr_ {
    char*     str;
    NixUI32   use;
    NixUI32   sz;
} STNixDbgStr;

void NixDbgStr_init(STNixDbgStr* obj);
void NixDbgStr_destroy(STNixDbgStr* obj);
void NixDbgStr_set(STNixDbgStr* obj, const char* str);

//STNixDbgStackRec

typedef struct STNixDbgStackRec_ {
    STNixDbgStr className;
} STNixDbgStackRec;

void NixDbgStackRec_init(STNixDbgStackRec* obj);
void NixDbgStackRec_destroy(STNixDbgStackRec* obj);

//STNixDbgThreadState

//These methods are used to track code execution; for better retain/release/leak detection.

typedef struct STNixDbgThreadState_ {
    //stack
    struct {
        STNixDbgStackRec*   arr;
        NixUI32             use;
        NixUI32             sz;
    } stack;
} STNixDbgThreadState;

STNixDbgThreadState* NixDbgThreadState_get(void);
void NixDbgThreadState_push(const char* className);
void NixDbgThreadState_pop(void);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
