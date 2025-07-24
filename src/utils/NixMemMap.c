//
//  NixMemMap.c
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#include <stdio.h>		//NULL
#include <stdlib.h>		//malloc, free
#include <string.h>		//memcpy, memset
#include <assert.h>		//assert

#include "../utils/NixMemMap.h"

#if defined(__ANDROID__) //Android
#   include <jni.h>            //for JNIEnv, jobject
#   include <android/log.h>    //for __android_log_print()
#   define NIX_PRINTF_INFO(STR_FMT, ...)   __android_log_print(ANDROID_LOG_INFO, "Nixtla", STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)  __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
#elif defined(__QNX__) //BB10
#   define NIX_PRINTF_INFO(STR_FMT, ...)   fprintf(stdout, "Nix, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)  fprintf(stderr, "Nix ERROR, " STR_FMT, ##__VA_ARGS__); fflush(stderr)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) fprintf(stdout, "Nix WARNING, " STR_FMT, ##__VA_ARGS__); fflush(stdout)
#else
#   define NIX_PRINTF_INFO(STR_FMT, ...)   printf("Nix, " STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_ERROR(STR_FMT, ...)   printf("Nix ERROR, " STR_FMT, ##__VA_ARGS__)
#   define NIX_PRINTF_WARNING(STR_FMT, ...) printf("Nix WARNING, " STR_FMT, ##__VA_ARGS__)
#endif

#define NIX_MEM_STR_BLOCKS_SZ       2048
#define NIX_MEM_STR_IDX_BLOCKS_SZ   128
#define NIX_MEM_PTRS_BLOCKS_SZ      256

//STNixMemStrs

void NixMemStrs_init(STNixMemStrs* obj){
    memset(obj, 0, sizeof(*obj));
    //init str buffs
    {
        unsigned long sizeN = NIX_MEM_STR_BLOCKS_SZ;
        char* buffN = (char*)malloc(sizeof(obj->buff[0]) * sizeN);
        if(buffN != NULL){
            obj->buff       = buffN;
            obj->buff[0]    = '\0'; //The first string is always <empty string>
            obj->buffUse    = 1;
            obj->buffSz     = sizeN;
        }
    }
}

void NixMemStrs_destroy(STNixMemStrs* obj){
    if(obj->buff != NULL) {
        free(obj->buff);
        obj->buff = NULL;
    }
    if(obj->idxs != NULL) {
        free(obj->idxs);
        obj->idxs = NULL;
    }

}

unsigned long NixMemStrs_find(STNixMemStrs* obj, const char* str){
    unsigned long r = 0;
    if(str == NULL || str[0] == '\0') return r;
    //Look for string
    unsigned long i;
    for(i = 0; i < obj->idxsUse; i++){
        assert(obj->idxs[i] < obj->buffUse);
        if(0 == strcmp(&obj->buff[obj->idxs[i]], str)){
            r = obj->idxs[i];
            break;
        }
    }
    return r;
}

unsigned long NixMemStrs_findOrAdd(STNixMemStrs* obj, const char* str){
    unsigned long r = 0;
    if(str == NULL || str[0] == '\0') return r;
    //
    r = NixMemStrs_find(obj, str);
    //Register new string
    if(r == 0){
        unsigned long strLen = strlen(str);
        //resize buffer (if necesary)
        if((obj->buffUse + strLen + 1) > obj->buffSz){
            unsigned long szN = (((obj->buffUse + strLen + 1) + NIX_MEM_STR_BLOCKS_SZ - 1) / NIX_MEM_STR_BLOCKS_SZ * NIX_MEM_STR_BLOCKS_SZ);
            char* buffN = (char*)realloc(obj->buff, sizeof(obj->buff[0]) * szN);
            if(buffN != NULL){
                obj->buff   = buffN;
                obj->buffSz = szN;
            }
        }
        //resize indexes (if necesary)
        if(obj->idxsUse >= obj->idxsSz){
            unsigned long szN = (((obj->idxsUse + 1) + NIX_MEM_STR_IDX_BLOCKS_SZ - 1) / NIX_MEM_STR_IDX_BLOCKS_SZ * NIX_MEM_STR_IDX_BLOCKS_SZ);
            unsigned long* idxsN = (unsigned long*)realloc(obj->idxs, sizeof(obj->idxs[0]) * szN);
            if(idxsN != NULL){
                obj->idxs   = idxsN;
                obj->idxsSz = szN;
            }
        }
        //Add index and string
        if(obj->idxsUse < obj->idxsSz && (obj->buffUse + strLen + 1) <= obj->buffSz){
            obj->idxs[obj->idxsUse++] = r = obj->buffUse;
            memcpy(&obj->buff[obj->buffUse], str, strLen + 1);
            obj->buffUse += (strLen + 1);
        }
    }
    //
    return r;
}

//STNixPtrRetainer

void NixPtrRetainer_init(STNixPtrRetainer* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixPtrRetainer_destroy(STNixPtrRetainer* obj){
    memset(obj, 0, sizeof(*obj));
}

//STNixMemBlock

void NixMemBlock_init(STNixMemBlock* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixMemBlock_destroy(STNixMemBlock* obj){
    //retains
    {
        if(obj->retains.arr != NULL){
            unsigned long i; for(i = 0; i < obj->retains.use; i++){
                STNixPtrRetainer* rr = &obj->retains.arr[i];
                NixPtrRetainer_destroy(rr);
            }
            free(obj->retains.arr);
            obj->retains.arr = NULL;
        }
        obj->retains.sz = obj->retains.use = 0;
    }
    memset(obj, 0, sizeof(*obj));
}

void NixMemBlock_retainedBy(STNixMemBlock* obj, const unsigned long iStrHint){
    //search record
    if(obj->retains.arr != NULL){
        unsigned long i; for(i = 0; i < obj->retains.use; i++){
            STNixPtrRetainer* rr = &obj->retains.arr[i];
            if(rr->iStrHint == iStrHint){
                obj->retains.balance++;
                rr->balance++;
                return;
            }
        }
    }
    //create record
    if(obj->retains.use >= obj->retains.sz){
        unsigned long szN = obj->retains.use + 32;
        STNixPtrRetainer* arrN = (STNixPtrRetainer*)realloc(obj->retains.arr, szN * sizeof(obj->retains.arr[0]));
        if(arrN != NULL){
            obj->retains.arr = arrN;
            obj->retains.sz = szN;
        }
    }
    if(obj->retains.use < obj->retains.sz){
        STNixPtrRetainer* rr = &obj->retains.arr[obj->retains.use++];
        NixPtrRetainer_init(rr);
        rr->iStrHint = iStrHint;
        rr->balance++;
    }
    obj->retains.balance++;
}

void NixMemBlock_releasedBy(STNixMemBlock* obj, const unsigned long iStrHint){
    //search record
    if(obj->retains.arr != NULL){
        unsigned long i; for(i = 0; i < obj->retains.use; i++){
            STNixPtrRetainer* rr = &obj->retains.arr[i];
            if(rr->iStrHint == iStrHint){
                obj->retains.balance--;
                rr->balance--;
                return;
            }
        }
    }
    //create record
    if(obj->retains.use >= obj->retains.sz){
        unsigned long szN = obj->retains.use + 32;
        STNixPtrRetainer* arrN = (STNixPtrRetainer*)realloc(obj->retains.arr, szN * sizeof(obj->retains.arr[0]));
        if(arrN != NULL){
            obj->retains.arr = arrN;
            obj->retains.sz = szN;
        }
    }
    if(obj->retains.use < obj->retains.sz){
        STNixPtrRetainer* rr = &obj->retains.arr[obj->retains.use++];
        NixPtrRetainer_init(rr);
        rr->iStrHint = iStrHint;
        rr->balance--;
    }
    obj->retains.balance--;
}

//STNixMemBlocks

void NixMemBlocks_init(STNixMemBlocks* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixMemBlocks_destroy(STNixMemBlocks* obj){
    if(obj->arr){
        unsigned long i; for(i = 0; i < obj->arrUse; i++){
            STNixMemBlock* b = &obj->arr[i];
            NixMemBlock_destroy(b);
        }
        free(obj->arr);
        obj->arr = NULL;
        obj->arrSz = 0;
        obj->arrUse = 0;
    }
}

STNixMemBlock* NixMemBlocks_findExact(STNixMemBlocks* obj, void* pointer){
    STNixMemBlock* r = NULL;
    unsigned long i; for(i = 0; i < obj->arrUse; i++){
        STNixMemBlock* b = &obj->arr[i];
        if(b->regUsed && b->pointer == pointer){
            r = b;
            break;
        }
    }
    return r;
}

STNixMemBlock* NixMemBlocks_findContainer(STNixMemBlocks* obj, void* pointer){
    STNixMemBlock* r = NULL;
    unsigned long i; for(i = 0; i < obj->arrUse; i++){
        STNixMemBlock* b = &obj->arr[i];
        if(b->regUsed && b->pointer <= pointer && pointer <= (void*)((char*)b->pointer + b->bytes)){
            r = b;
            break;
        }
    }
    return r;
}

STNixMemBlock* NixMemBlocks_add(STNixMemBlocks* obj, void* pointer, unsigned long bytes, const unsigned long iStrHint){
    STNixMemBlock* r = NULL;
    //search for an available record
    {
        unsigned long i; for(i = 0; i < obj->arrUse; i++){
            STNixMemBlock* b = &obj->arr[i];
            if(!b->regUsed){
                r = b;
                break;
            }
        }
    }
    //create new record
    if(r == NULL){
        //resize indexes (if necesary)
        if(obj->arrUse >= obj->arrSz){
            unsigned long szN = (((obj->arrUse + 1) + NIX_MEM_PTRS_BLOCKS_SZ - 1) / NIX_MEM_PTRS_BLOCKS_SZ * NIX_MEM_PTRS_BLOCKS_SZ);
            STNixMemBlock* arrN = (STNixMemBlock*)realloc(obj->arr, sizeof(obj->arr[0]) * szN);
            if(arrN != NULL){
                obj->arr   = arrN;
                obj->arrSz = szN;
            }
        }
        //
        if(obj->arrUse < obj->arrSz){
            r = &obj->arr[obj->arrUse++];
            NixMemBlock_init(r);
        }
    }
    //populate
    if(r){
        r->regUsed  = 1;
        r->pointer  = pointer;
        r->bytes    = bytes;
        r->iStrHint = iStrHint;
    }
    return r;
}

//STNixMemStats

void NixMemStats_init(STNixMemStats* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixMemStats_destroy(STNixMemStats* obj){
    memset(obj, 0, sizeof(*obj));
}

//---------------------------------------------
//-- Usefull implementation for memory leaking
//-- detection and tracking.
//---------------------------------------------

void NixMemMap_init(STNixMemMap* obj){
    memset(obj, 0, sizeof(*obj));
    NixMemStats_init(&obj->stats);
    NixMemStrs_init(&obj->strs);
    NixMemBlocks_init(&obj->blocks);
}

void NixMemMap_destroy(STNixMemMap* obj){
    NixMemStats_destroy(&obj->stats);
    NixMemStrs_destroy(&obj->strs);
    NixMemBlocks_destroy(&obj->blocks);
}

//mem

void NixMemMap_ptrAdd(STNixMemMap* obj, void* pointer, unsigned long bytes, const char* strHint){
    assert(NULL == NixMemBlocks_findExact(&obj->blocks, pointer)); //should not be registered
    //Find or register hint string
    const unsigned long iStrHint = NixMemStrs_findOrAdd(&obj->strs, strHint);
    if(NULL != NixMemBlocks_add(&obj->blocks, pointer, bytes, iStrHint)){
        obj->stats.alive.count++;
        obj->stats.alive.bytes += bytes;
        //
        obj->stats.total.count++;
        obj->stats.total.bytes += bytes;
        //
        if(obj->stats.max.count < obj->stats.alive.count) obj->stats.max.count = obj->stats.alive.count;
        if(obj->stats.max.bytes < obj->stats.alive.bytes) obj->stats.max.bytes = obj->stats.alive.bytes;
    }
}

void NixMemMap_ptrRemove(STNixMemMap* obj, void* pointer){
    unsigned long i; const unsigned long use = obj->blocks.arrUse;
    for(i = 0; i < use; i++){
        STNixMemBlock* b = &obj->blocks.arr[i];
        if(b->regUsed && b->pointer == pointer){
            b->regUsed = 0;
            obj->stats.alive.count--;
            obj->stats.alive.bytes -= b->bytes;
            return;
        }
    }
    assert(0); //Pointer was not found
}

//dbg

void NixMemMap_ptrRetainedBy(STNixMemMap* obj, void* pointer, const char* by){
    STNixMemBlock* b = NixMemBlocks_findExact(&obj->blocks, pointer);
    assert(b != NULL); //should be registered
    if(b != NULL){
        const unsigned long iStrHint = NixMemStrs_findOrAdd(&obj->strs, by);
        NixMemBlock_retainedBy(b, iStrHint);
    }
}

void NixMemMap_ptrReleasedBy(STNixMemMap* obj, void* pointer, const char* by){
    STNixMemBlock* b = NixMemBlocks_findExact(&obj->blocks, pointer);
    assert(b != NULL); //should be registered
    if(b != NULL){
        const unsigned long iStrHint = NixMemStrs_findOrAdd(&obj->strs, by);
        NixMemBlock_releasedBy(b, iStrHint);
    }
}

//report

void NixMemMap_printAlivePtrs(STNixMemMap* obj){
    unsigned long i, countUsed = 0, bytesUsed = 0; const unsigned long use = obj->blocks.arrUse;
    for(i = 0; i < use; i++){
        STNixMemBlock* b = &obj->blocks.arr[i];
        if(b->regUsed){
            ++countUsed;
            bytesUsed += b->bytes;
            NIX_PRINTF_INFO("#%lu) %lu, %lu bytes, '%s'\n", countUsed, (unsigned long)b->pointer, b->bytes, &obj->strs.buff[b->iStrHint]);
            if(b->retains.arr != NULL && b->retains.use > 0){
                NIX_PRINTF_INFO("        Retains balance: %ld (should be -1)\n", b->retains.balance);
                unsigned long i; for(i = 0; i < b->retains.use; i++){
                    STNixPtrRetainer* rr = &b->retains.arr[i];
                    NIX_PRINTF_INFO("        %ld by '%s'\n", rr->balance, rr->iStrHint == 0 ? "stack-or-unmanaged-obj": &obj->strs.buff[rr->iStrHint]);
                }
            }
        }
    }
    NIX_PRINTF_INFO("\n");
    NIX_PRINTF_INFO("CURRENTLY USED   : %lu blocks (%lu bytes)\n", obj->stats.alive.count, obj->stats.alive.bytes);
    NIX_PRINTF_INFO("MAX USED         : %lu blocks (%lu bytes)\n", obj->stats.max.count, obj->stats.max.bytes);
    NIX_PRINTF_INFO("TOTAL ALLOCATIONS: %lu blocks (%lu bytes)\n", obj->stats.total.count, obj->stats.total.bytes);
    assert(obj->stats.alive.count == countUsed); //program logic error
    assert(obj->stats.alive.bytes == bytesUsed); //program logic error
}

void NixMemMap_printFinalReport(STNixMemMap* obj){
    NIX_PRINTF_INFO("-------------- MEM REPORT -----------\n");
    if(obj->stats.alive.count == 0){
        NIX_PRINTF_INFO("Nixtla: no memory leaking detected :)\n");
    } else {
        NIX_PRINTF_WARNING("WARNING, NIXTLA MEMORY-LEAK DETECTED! :(\n");
    }
    NixMemMap_printAlivePtrs(obj);
    NIX_PRINTF_INFO("-------------------------------------\n");
}

