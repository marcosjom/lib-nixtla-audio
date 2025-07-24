//
//  demoCommon.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This class represents a Nixtla's engine to be shared between demos/tests.
// It also uses a custom memory allocation method for memory-leak tracking.
//

#include "NixDemosCommon.h"
#include <string.h> //memset
#include <time.h>   //time
#include <stdio.h>  //printf
#include <stdlib.h> //malloc/realloc/free

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h> //Sleep
	#define DEMO_SLEEP_MILLISEC(MS) Sleep(MS)
#else
	#include <unistd.h> //sleep, usleep
	#define DEMO_SLEEP_MILLISEC(MS) usleep((MS) * 1000);
#endif

// Custom memory allocation for this demos and tests,
// for detecting memory-leaks using NixMemMap.
void* NixMemMap_custom_malloc(const NixUI32 newSz, const char* dbgHintStr);
void* NixMemMap_custom_realloc(void* ptr, const NixUI32 newSz, const char* dbgHintStr);
void NixMemMap_custom_free(void* ptr);

// STNixDemosCommon

STNixDemosCommon* gCurInstance = NULL;

NixBOOL NixDemosCommon_isNull(STNixDemosCommon* obj){
    return NixContext_isNull(obj->ctx);
}

NixBOOL NixDemosCommon_init(STNixDemosCommon* obj){
    NixBOOL r = NIX_FALSE;
    gCurInstance = obj;
    //Init memory custom manager
    NixMemMap_init(&obj->memmap);
    //Init engine
    {
        STNixContextItf ctxItf;
        memset(&ctxItf, 0, sizeof(ctxItf));
        //define context interface
        {
            //custom memory allocation (for memory leaks dtection)
            {
                //mem
                ctxItf.mem.malloc   = NixMemMap_custom_malloc;
                ctxItf.mem.realloc  = NixMemMap_custom_realloc;
                ctxItf.mem.free     = NixMemMap_custom_free;
            }
            //use default for others
            NixContextItf_fillMissingMembers(&ctxItf);
        }
        //allocate a context
        STNixContextRef ctx = NixContext_alloc(&ctxItf);
        if(ctx.itf == NULL){
            NIX_PRINTF_ERROR("NixDemosCommon_init::ctx.itf == NULL.\n");
        } else if(ctx.itf->mem.malloc == NULL){
            NIX_PRINTF_ERROR("NixDemosCommon_init::ctx.itf->mem.malloc == NULL.\n");
        }
        {
            //get the API interface
            STNixApiItf apiItf;
            if(!NixApiItf_getDefaultApiForCurrentOS(&apiItf)){
                NIX_PRINTF_ERROR("ERROR, NixApiItf_getDefaultApiForCurrentOS failed.\n");
            } else {
                //create engine
                obj->eng = NixEngine_alloc(ctx, &apiItf);
                if(NixEngine_isNull(obj->eng)){
                    NIX_PRINTF_ERROR("ERROR, NixEngine_alloc failed.\n");
                } else {
                    NixContext_set(&obj->ctx, ctx);
                    r = NIX_TRUE;
                    //
                    if(obj->ctx.itf == NULL){
                        NIX_PRINTF_ERROR("NixDemosCommon_init::obj->ctx.itf == NULL.\n");
                    } else if(obj->ctx.itf->mem.malloc == NULL){
                        NIX_PRINTF_ERROR("NixDemosCommon_init::obj->ctx.itf->mem.malloc == NULL.\n");
                    }
                }
            }
        }
        //context is retained by the engine
        NixContext_release(&ctx);
        NixContext_null(&ctx);
    }
    return r;
}

void NixDemosCommon_destroy(STNixDemosCommon* obj){
    //engine
    if(!NixEngine_isNull(obj->eng)){
        NixEngine_release(&obj->eng);
        NixEngine_null(&obj->eng);
    }
    //context
    if(!NixContext_isNull(obj->ctx)){
        NixContext_release(&obj->ctx);
        NixContext_null(&obj->ctx);
    }
    //custom memory manager
    {
        NixMemMap_printFinalReport(&obj->memmap);
        NixMemMap_destroy(&obj->memmap);
    }
    //
    gCurInstance = NULL;
    *obj = (STNixDemosCommon)STNixDemosCommon_Zero;
}

void NixDemosCommon_tick(STNixDemosCommon* obj, const NixUI32 msPassed){
    NixEngine_tick(obj->eng);
}

//custom memory allocation (for memory leaks detection)

void* NixMemMap_custom_malloc(const NixUI32 newSz, const char* dbgHintStr){
    STNixDemosCommon* obj = gCurInstance;
    void* r = malloc(newSz);
    if(obj == NULL){
        NIX_PRINTF_ERROR("NixMemMap_custom_malloc::gCurInstance is NULL.\n");
    } else {
        NixMemMap_ptrAdd(&obj->memmap, r, newSz, dbgHintStr);
    }
    return r;
}

void* NixMemMap_custom_realloc(void* ptr, const NixUI32 newSz, const char* dbgHintStr){
    STNixDemosCommon* obj = gCurInstance;
    //"If there is not enough memory, the old memory block is not freed and null pointer is returned."
    void* r = realloc(ptr, newSz);
    if(obj == NULL){
        NIX_PRINTF_ERROR("NixMemMap_custom_realloc::gCurInstance is NULL.\n");
    } else {
        if(r != NULL){
            if(ptr != NULL){
                NixMemMap_ptrRemove(&obj->memmap, ptr);
            }
            NixMemMap_ptrAdd(&obj->memmap, r, newSz, dbgHintStr);
        }
    }
    return r;
}

void NixMemMap_custom_free(void* ptr){
    STNixDemosCommon* obj = gCurInstance;
    if(obj == NULL){
        NIX_PRINTF_ERROR("NixMemMap_custom_free::gCurInstance is NULL.\n");
    } else {
        NixMemMap_ptrRemove(&obj->memmap, ptr);
    }
    free(ptr);
}
