//
//  NixMemMap.c
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

#include <stdlib.h> //malloc
#include <string.h> //memset

#include "../nixaudio/nixtla-audio-private.h"
#include "nixaudio/nixtla-audio.h"
#include "NixDbg.h"

#if !defined(NIX_MUTEX_T) || !defined(NIX_MUTEX_INIT) || !defined(NIX_MUTEX_DESTROY) || !defined(NIX_MUTEX_LOCK) || !defined(NIX_MUTEX_UNLOCK)
#   ifdef _WIN32
//#     define WIN32_LEAN_AND_MEAN
#       include <windows.h>             //for TlsAlloc
#   else
#       include <pthread.h>             //for pthread_mutex_t
#   endif
#endif

//STNixDbgStr

void NixDbgStr_init(STNixDbgStr* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixDbgStr_destroy(STNixDbgStr* obj){
    if(obj->str != NULL){
        free(obj->str);
    }
    obj->use = obj->sz = 0;
    memset(obj, 0, sizeof(*obj));
}

void NixDbgStr_set(STNixDbgStr* obj, const char* str){
    if(str == NULL){
        obj->use = 0;
        if(obj->str != NULL){
            obj->str[0] = '\0';
        }
    } else {
        size_t len = strlen(str);
        if((len + 1) > obj->sz){
            const NixUI32 szN = (NixUI32)(len + 1);
            char* strN = realloc(obj->str, szN);
            if(strN != NULL){
                obj->str = strN;
                obj->sz = szN;
            }
        }
        if((len + 1) <= obj->sz){
            memcpy(obj->str, str, len + 1);
            obj->use = (NixUI32)(len + 1);
        }
    }
}

//STNixDbgStackRec

void NixDbgStackRec_init(STNixDbgStackRec* obj){
    memset(obj, 0, sizeof(*obj));
    NixDbgStr_init(&obj->className);
}

void NixDbgStackRec_destroy(STNixDbgStackRec* obj){
    NixDbgStr_destroy(&obj->className);
}


//STNixDbgThreadState

//These methods are used to track code execution; for better retain/release/leak detection.

#ifdef _WIN32
NixUI32         _nixThreadStateIdx = TLS_OUT_OF_INDEXES; //TLS_OUT_OF_INDEXES(0xFFFFFFFF) means error or not-initialized
#else
pthread_once_t  _nixThreadStatekeyOnce = PTHREAD_ONCE_INIT;
pthread_key_t   _nixThreadStatekey;
#endif

void NixDbgThreadState_init(STNixDbgThreadState* obj){
    memset(obj, 0, sizeof(*obj));
}

void NixDbgThreadState_destroy(STNixDbgThreadState* obj){
    if(obj->stack.arr != NULL){
        NixUI32 i; for(i = 0; i < obj->stack.use; i++){
            STNixDbgStackRec* rec = &obj->stack.arr[i];
            NixDbgStackRec_destroy(rec);
        }
        free(obj->stack.arr);
        obj->stack.arr = NULL;
    }
    obj->stack.use = obj->stack.sz = 0;
}

void NixDbgThreadState_destroyData_(void* pData){
    STNixDbgThreadState* state = (STNixDbgThreadState*)pData;
    //ToDo: destroy tls/key
    if(state != NULL){
        NixDbgThreadState_destroy(state);
        free(state);
    }
}

void NixDbgThreadState_createDataOnce_(void){
#   ifdef _WIN32
    {
        _nixThreadStateIdx = TlsAlloc();
        if(TLS_OUT_OF_INDEXES == _nixThreadStateIdx){
            _nixThreadStateIdx = TLS_OUT_OF_INDEXES;
        } else {
            STNixDbgThreadState* state = (STNixDbgThreadState*)malloc(sizeof(STNixDbgThreadState));
            NixDbgThreadState_init(state);
            if(!TlsSetValue(_nixThreadStateIdx, NULL)){
                NixDbgThreadState_destroy(state);
                free(state);
            }
        }
    }
#   else
    {
        if(0 != pthread_key_create(&_nixThreadStatekey, NixDbgThreadState_destroyData_)){
            //
        } else {
            STNixDbgThreadState* state = (STNixDbgThreadState*)malloc(sizeof(STNixDbgThreadState));
            NixDbgThreadState_init(state);
            if(0 != pthread_setspecific(_nixThreadStatekey, state)){
                NixDbgThreadState_destroy(state);
                free(state);
            }
        }
    }
#   endif
}

STNixDbgThreadState* NixDbgThreadState_get(void){
    STNixDbgThreadState* r = NULL;
#   ifdef _WIN32
    if (_nixThreadStateIdx == TLS_OUT_OF_INDEXES) {
        (*NixDbgThreadState_createDataOnce_)();
    }
    if (_nixThreadStateIdx != TLS_OUT_OF_INDEXES) {
        r = (STNixDbgThreadState*)TlsGetValue(_nixThreadStateIdx);
    }
#   else
    pthread_once(&_nixThreadStatekeyOnce, NixDbgThreadState_createDataOnce_);
    r = (STNixDbgThreadState*)pthread_getspecific(_nixThreadStatekey);
#   endif
    return r;
}

void NixDbgThreadState_push(const char* className){
    STNixDbgThreadState* obj = NixDbgThreadState_get();
    if(obj != NULL){
        //resize (if necesary)
        if(obj->stack.use >= obj->stack.sz){
            NixUI32 szN = obj->stack.use + 32;
            STNixDbgStackRec* arrN = realloc(obj->stack.arr, sizeof(obj->stack.arr[0]) * szN);
            if(arrN != NULL){
                obj->stack.arr = arrN;
                obj->stack.sz = szN;
            }
        }
        if(obj->stack.use < obj->stack.sz){
            STNixDbgStackRec* rec = &obj->stack.arr[obj->stack.use++];
            NixDbgStackRec_init(rec);
            NixDbgStr_set(&rec->className, className);
        }
    }
}

void NixDbgThreadState_pop(void){
    STNixDbgThreadState* obj = NixDbgThreadState_get();
    if(obj != NULL){
        NIX_ASSERT(obj->stack.use > 0)
        if(obj->stack.use > 0){
            STNixDbgStackRec* rec = &obj->stack.arr[obj->stack.use - 1];
            NixDbgStackRec_destroy(rec);
            obj->stack.use--;
        }
    }
}

