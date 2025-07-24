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

//---------------------------------------------
//-- Usefull implementation for memory leaking
//-- detection and tracking.
//---------------------------------------------

//STNixMemStrs

typedef struct STNixMemStrs_ {
    char*            buff;
    unsigned long    buffSz;
    unsigned long    buffUse;
    unsigned long*   idxs;
    unsigned long    idxsSz;
    unsigned long    idxsUse;
} STNixMemStrs;

void NixMemStrs_init(STNixMemStrs* obj);
void NixMemStrs_destroy(STNixMemStrs* obj);
unsigned long NixMemStrs_find(STNixMemStrs* obj, const char* str);
unsigned long NixMemStrs_findOrAdd(STNixMemStrs* obj, const char* str);

//STNixPtrRetainer

typedef struct STNixPtrRetainer_ {
    unsigned long   iStrHint;
    long            balance; //(retain - release)
} STNixPtrRetainer;

void NixPtrRetainer_init(STNixPtrRetainer* obj);
void NixPtrRetainer_destroy(STNixPtrRetainer* obj);

//STNixMemBlock

typedef struct STNixMemBlock_ {
    char            regUsed;
    unsigned long   iStrHint;
    void*           pointer;
    unsigned long   bytes;
    //retains
    struct {
        STNixPtrRetainer*   arr;
        unsigned long       sz;
        unsigned long       use;
        long                balance; //(retain - release)
    } retains;
} STNixMemBlock;

void NixMemBlock_init(STNixMemBlock* obj);
void NixMemBlock_destroy(STNixMemBlock* obj);
void NixMemBlock_retainedBy(STNixMemBlock* obj, const unsigned long iStrHint);
void NixMemBlock_releasedBy(STNixMemBlock* obj, const unsigned long iStrHint);

//STNixMemBlocks

typedef struct STNixMemBlocks_ {
    STNixMemBlock*   arr;
    unsigned long    arrSz;
    unsigned long    arrUse;
} STNixMemBlocks;

void NixMemBlocks_init(STNixMemBlocks* obj);
void NixMemBlocks_destroy(STNixMemBlocks* obj);
STNixMemBlock* NixMemBlocks_findExact(STNixMemBlocks* obj, void* pointer);
STNixMemBlock* NixMemBlocks_findContainer(STNixMemBlocks* obj, void* pointer);
STNixMemBlock* NixMemBlocks_add(STNixMemBlocks* obj, void* pointer, unsigned long bytes, const unsigned long iStrHint);

//STNixMemStats

typedef struct STNixMemStats_ {
    //alive (blocks of memory currently alive)
    struct {
        unsigned long   count;
        unsigned long   bytes;
    } alive;
    //total (ammount allocated during program execution)
    struct {
        unsigned long   count;
        unsigned long   bytes;
    } total;
    //max (max during any point of execution)
    struct {
        unsigned long   count;
        unsigned long   bytes;
    } max;
} STNixMemStats;

void NixMemStats_init(STNixMemStats* obj);
void NixMemStats_destroy(STNixMemStats* obj);

//STNixMemMap

typedef struct STNixMemMap_ {
    STNixMemStats   stats;
    STNixMemStrs    strs;
    STNixMemBlocks  blocks;
} STNixMemMap;

void NixMemMap_init(STNixMemMap* obj);
void NixMemMap_destroy(STNixMemMap* obj);
//mem
void NixMemMap_ptrAdd(STNixMemMap* obj, void* pointer, unsigned long bytes, const char* strHint);
void NixMemMap_ptrRemove(STNixMemMap* obj, void* pointer);
//dbg
void NixMemMap_ptrRetainedBy(STNixMemMap* obj, void* pointer, const char* by);
void NixMemMap_ptrReleasedBy(STNixMemMap* obj, void* pointer, const char* by);
//report
void NixMemMap_printAlivePtrs(STNixMemMap* obj);
void NixMemMap_printFinalReport(STNixMemMap* obj);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
