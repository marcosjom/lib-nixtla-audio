//
//  nixtla.h
//  NixtlaAudioLib
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//
//  This entire notice must be retained in this source code.
//  This source code is under MIT Licence.
//
//  This software is provided "as is", with absolutely no warranty expressed
//  or implied. Any use is at your own risk.
//
//  Latest fixes enhancements and documentation at https://github.com/marcosjom/lib-nixtla-audio
//

#ifndef NixtlaAudioLib_nixtla_h
#define NixtlaAudioLib_nixtla_h

#ifdef __cplusplus
extern "C" {
#endif

// Data types

typedef unsigned char 		NixBOOL;	//BOOL, Unsigned 8-bit integer value
typedef unsigned char 		NixBYTE;	//BYTE, Unsigned 8-bit integer value
typedef char 				NixSI8;		//NIXSI8, Signed 8-bit integer value
typedef	short int 			NixSI16;	//NIXSI16, Signed 16-bit integer value
typedef	int 				NixSI32;	//NIXSI32, Signed 32-bit integer value
typedef	long long 			NixSI64;	//NixSI64, Signed 64-bit integer value
typedef unsigned char 		NixUI8;		//NixUI8, Unsigned 8-bit integer value
typedef	unsigned short int 	NixUI16;	//NixUI16, Unsigned 16-bit integer value
typedef	unsigned int 		NixUI32;	//NixUI32, Unsigned 32-bit integer value
typedef	unsigned long long	NixUI64;	//NixUI64[n], Unsigned 64-bit array—n is the number of array elements
typedef float				NixFLOAT;	//float
typedef double              NixDOUBLE;  //double

#define NX_INLN             static inline

#define NIX_FALSE			0
#define NIX_TRUE			1

//NULL
#if !defined(__cplusplus) && !defined(NULL)
#    define NULL            ((void*)0)
#endif

// Capabilities mask

#define NIX_CAP_AUDIO_CAPTURE           1
#define NIX_CAP_AUDIO_STATIC_BUFFERS    2
#define NIX_CAP_AUDIO_SOURCE_OFFSETS    4

// ENNixSampleFmt

typedef enum ENNixSampleFmt_ {
    ENNixSampleFmt_Unknown = 0,
    ENNixSampleFmt_Int,         //unsigned byte for 8-bits, signed short/int/long for 16/24/32/64 bits
    ENNixSampleFmt_Float,       //float 32 bits
    //
    ENNixSampleFmt_Count
} ENNixSampleFmt;

// ENNixOffsetType

typedef enum ENNixOffsetType_ {
    ENNixOffsetType_Blocks = 0, //audio blocks/frames
    ENNixOffsetType_Msecs,      //audio milliseconds
    ENNixOffsetType_Bytes,      //audio bytes (will be rounded to block)
    //
    ENNixOffsetType_Count
} ENNixOffsetType;

// STNixAudioDesc

#define STNixAudioDesc_Zero    { 0, 0, 0, 0, 0 }

typedef struct STNixAudioDesc_ {
    NixUI8  samplesFormat;  //ENNixSampleFmt
    NixUI8  channels;
    NixUI8  bitsPerSample;
    NixUI16 samplerate;
    NixUI16 blockAlign;     //bytes per sample-frame: (bitsPerSample / 8) * channels <= for interlaced byte-aligned sizes usually
} STNixAudioDesc;

NixBOOL STNixAudioDesc_isEqual(const STNixAudioDesc* obj, const STNixAudioDesc* other);

//------------
// Context (core)
//------------

struct STNixSharedPtr_;
struct STNixMutexRef_;
struct STNixContextRef_;

struct STNixMemoryItf_;
struct STNixMutexItf_;
struct STNixContextItf_;

//STNixMemoryItf

typedef struct STNixMemoryItf_ {
    void*   (*malloc)(const NixUI32 newSz, const char* dbgHintStr);
    void*   (*realloc)(void* ptr, const NixUI32 newSz, const char* dbgHintStr);
    void    (*free)(void* ptr);
} STNixMemoryItf;

// Links NULL methods to a DEFAULT implementation,
// this reduces the need to check for functions NULL pointers.
void NixMemoryItf_fillMissingMembers(STNixMemoryItf* itf);

// STNixSharedPtr (provides the retain/release model)

struct STNixSharedPtr_* NixSharedPtr_alloc(struct STNixContextItf_* ctx, void* opq, const char* dbgHintStr);     //can be casted to void** to obtain the 'opq' value back
void    NixSharedPtr_free(struct STNixSharedPtr_* obj);
void*   NixSharedPtr_getOpq(struct STNixSharedPtr_* obj);
void    NixSharedPtr_retain(struct STNixSharedPtr_* obj);
NixSI32 NixSharedPtr_release(struct STNixSharedPtr_* obj); //returns the retainCount

// STNixMutexRef

#define STNixMutexRef_Zero { NULL, NULL }

typedef struct STNixMutexRef_ {
    void*                   opq;
    struct STNixMutexItf_*  itf;
} STNixMutexRef;

void NixMutex_free(STNixMutexRef* ref);
void NixMutex_lock(STNixMutexRef ref);
void NixMutex_unlock(STNixMutexRef ref);

//STNixMutexItf

typedef struct STNixMutexItf_ {
    STNixMutexRef (*alloc)(struct STNixContextItf_* ctx);
    void        (*free)(STNixMutexRef* obj);
    void        (*lock)(STNixMutexRef obj);
    void        (*unlock)(STNixMutexRef obj);
} STNixMutexItf;

//Links NULL methods to a DEFAULT implementation,
//this reduces the need to check for functions NULL pointers.
void NixMutexItf_fillMissingMembers(STNixMutexItf* itf);

//STNixContextRef

#define STNixContextRef_Zero    { NULL, NULL }

typedef struct STNixContextRef_ {
    struct STNixSharedPtr_*  ptr;
    struct STNixContextItf_* itf;
} STNixContextRef;

STNixContextRef NixContext_alloc(struct STNixContextItf_* ctx);
void            NixContext_retain(STNixContextRef ref);
void            NixContext_release(STNixContextRef* ref);
void            NixContext_set(STNixContextRef* ref, STNixContextRef other);
NX_INLN NixBOOL NixContext_isSame(STNixContextRef ref, STNixContextRef other) { return (ref.ptr == other.ptr); }
NX_INLN NixBOOL NixContext_isNull(STNixContextRef ref) { return (ref.ptr == NULL); }
void            NixContext_null(STNixContextRef* ref);
//context (memory)
void*           NixContext_malloc(STNixContextRef ref, const NixUI32 newSz, const char* dbgHintStr);
void*           NixContext_mrealloc(STNixContextRef ref, void* ptr, const NixUI32 newSz, const char* dbgHintStr);
void            NixContext_mfree(STNixContextRef ref, void* ptr);
//context (mutex)
STNixMutexRef   NixContext_mutex_alloc(STNixContextRef ref);

//STNixContextItf

typedef struct STNixContextItf_ {
    STNixMemoryItf  mem;
    STNixMutexItf   mutex;
} STNixContextItf;

STNixContextItf NixContextItf_getDefault(void);

//Links NULL methods to a DEFAULT implementation,
//this reduces the need to check for functions NULL pointers.
void NixContextItf_fillMissingMembers(STNixContextItf* itf);

//------------
// API (audio)
//------------

struct STNixApiItf_;
struct STNixEngineItf_;
struct STNixBufferItf_;
struct STNixSourceItf_;
struct STNixRecorderItf_;

struct STNixEngineRef_;
struct STNixBufferRef_;
struct STNixSourceRef_;
struct STNixRecorderRef_;

//Callbacks

typedef void (*NixSourceCallbackFnc)(struct STNixSourceRef_* src, struct STNixBufferRef_* buffs, const NixUI32 buffsSz, void* userdata);
typedef void (*NixRecorderCallbackFnc)(struct STNixEngineRef_* eng, struct STNixRecorderRef_* rec, const STNixAudioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 blocksCount, void* userdata);

//STNixBufferRef (shared pointer)

#define STNixBufferRef_Zero     { NULL, NULL }

typedef struct STNixBufferRef_ {
    struct STNixSharedPtr_*  ptr;
    struct STNixBufferItf_*  itf;
} STNixBufferRef;

void            NixBuffer_retain(STNixBufferRef obj);
void            NixBuffer_release(STNixBufferRef* obj);
NX_INLN NixBOOL NixBuffer_isSame(STNixBufferRef ref, STNixBufferRef other) { return (ref.ptr == other.ptr); }
NX_INLN NixBOOL NixBuffer_isNull(STNixBufferRef ref) { return (ref.ptr == NULL); }
NX_INLN void    NixBuffer_null(STNixBufferRef* ref){ if(ref != NULL) ref->ptr = NULL; }
NX_INLN void    NixBuffer_set(STNixBufferRef* ref, STNixBufferRef other){ if(!NixBuffer_isNull(other)){ NixBuffer_retain(other); } if(!NixBuffer_isNull(*ref)){ NixBuffer_release(ref); } *ref = other; }
NixBOOL         NixBuffer_setData(STNixBufferRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL         NixBuffer_fillWithZeroes(STNixBufferRef ref);

//STNixSourceRef (shared pointer)

#define STNixSourceRef_Zero     { NULL, NULL }

typedef struct STNixSourceRef_ {
    struct STNixSharedPtr_*  ptr;
    struct STNixSourceItf_*  itf;
} STNixSourceRef;

void            NixSource_retain(STNixSourceRef obj);
void            NixSource_release(STNixSourceRef* obj);
NX_INLN NixBOOL NixSource_isSame(STNixSourceRef ref, STNixSourceRef other) { return (ref.ptr == other.ptr); }
NX_INLN NixBOOL NixSource_isNull(STNixSourceRef ref) { return (ref.ptr == NULL); }
NX_INLN void    NixSource_null(STNixSourceRef* ref){ if(ref != NULL) ref->ptr = NULL; }
NX_INLN void    NixSource_set(STNixSourceRef* ref, STNixSourceRef other){ if(!NixSource_isNull(other)){ NixSource_retain(other); } if(!NixSource_isNull(*ref)){ NixSource_release(ref); } *ref = other; }
void            NixSource_setCallback(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData);
NixBOOL         NixSource_setVolume(STNixSourceRef ref, const NixFLOAT vol);
NixBOOL         NixSource_setRepeat(STNixSourceRef ref, const NixBOOL isRepeat);
void            NixSource_play(STNixSourceRef ref);
void            NixSource_pause(STNixSourceRef ref);
void            NixSource_stop(STNixSourceRef ref);
NixBOOL         NixSource_isPlaying(STNixSourceRef ref);
NixBOOL         NixSource_isPaused(STNixSourceRef ref);
NixBOOL         NixSource_isRepeat(STNixSourceRef ref);
NixFLOAT        NixSource_getVolume(STNixSourceRef ref);
NixBOOL         NixSource_setBuffer(STNixSourceRef ref, STNixBufferRef buff);  //static-source
NixBOOL         NixSource_queueBuffer(STNixSourceRef ref, STNixBufferRef buff); //stream-source
NixBOOL         NixSource_setBufferOffset(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset); //relative to first buffer in queue
NixUI32         NixSource_getBuffersCount(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);   //all buffer queue
NixUI32         NixSource_getBlocksOffset(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //relative to first buffer in queue

//STNixRecorderRef (shared pointer)

#define STNixRecorderRef_Zero     { NULL, NULL }

typedef struct STNixRecorderRef_ {
    struct STNixSharedPtr_*   ptr;
    struct STNixRecorderItf_* itf;
} STNixRecorderRef;


void            NixRecorder_retain(STNixRecorderRef obj);
void            NixRecorder_release(STNixRecorderRef* obj);
NX_INLN NixBOOL NixRecorder_isSame(STNixRecorderRef ref, STNixRecorderRef other) { return (ref.ptr == other.ptr); }
NX_INLN NixBOOL NixRecorder_isNull(STNixRecorderRef ref) { return (ref.ptr == NULL); }
NX_INLN void    NixRecorder_null(STNixRecorderRef* ref){ if(ref != NULL) ref->ptr = NULL; }
NX_INLN void    NixRecorder_set(STNixRecorderRef* ref, STNixRecorderRef other){ if(!NixRecorder_isNull(other)){ NixRecorder_retain(other); } if(!NixRecorder_isNull(*ref)){ NixRecorder_release(ref); } *ref = other; }
NixBOOL         NixRecorder_setCallback(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData);
NixBOOL         NixRecorder_start(STNixRecorderRef ref);
NixBOOL         NixRecorder_stop(STNixRecorderRef ref);
NixBOOL         NixRecorder_flush(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying);
NixBOOL         NixRecorder_isCapturing(STNixRecorderRef ref);
NixUI32         NixRecorder_getBuffersFilledCount(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);

//STNixEngineRef (shared pointer)

#define STNixEngineRef_Zero     { NULL, NULL }

typedef struct STNixEngineRef_ {
    struct STNixSharedPtr_*  ptr;
    struct STNixEngineItf_*  itf;
} STNixEngineRef;

STNixEngineRef  NixEngine_alloc(STNixContextRef ctx, struct STNixApiItf_* apiItf);
void            NixEngine_retain(STNixEngineRef obj);
void            NixEngine_release(STNixEngineRef* obj);
NX_INLN NixBOOL NixEngine_isSame(STNixEngineRef ref, STNixEngineRef other) { return (ref.ptr == other.ptr); }
NX_INLN NixBOOL NixEngine_isNull(STNixEngineRef ref) { return (ref.ptr == NULL); }
NX_INLN void    NixEngine_null(STNixEngineRef* ref){ if(ref != NULL) ref->ptr = NULL; }
NX_INLN void    NixEngine_set(STNixEngineRef* ref, STNixEngineRef other){ if(!NixEngine_isNull(other)){ NixEngine_retain(other); } if(!NixEngine_isNull(*ref)){ NixEngine_release(ref); } *ref = other; }
//
void            NixEngine_printCaps(STNixEngineRef ref);
NixBOOL         NixEngine_ctxIsActive(STNixEngineRef ref);
NixBOOL         NixEngine_ctxActivate(STNixEngineRef ref);
NixBOOL         NixEngine_ctxDeactivate(STNixEngineRef ref);
void            NixEngine_tick(STNixEngineRef ref);
//Factory
STNixSourceRef  NixEngine_allocSource(STNixEngineRef ref);
STNixBufferRef  NixEngine_allocBuffer(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
STNixRecorderRef NixEngine_allocRecorder(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);

//STNixEngineItf (API)

typedef struct STNixEngineItf_ {
    STNixEngineRef  (*alloc)(STNixContextRef ctx);
    void            (*free)(STNixEngineRef ref);
    void            (*printCaps)(STNixEngineRef ref);
    NixBOOL         (*ctxIsActive)(STNixEngineRef ref);
    NixBOOL         (*ctxActivate)(STNixEngineRef ref);
    NixBOOL         (*ctxDeactivate)(STNixEngineRef ref);
    void            (*tick)(STNixEngineRef ref);
    //Factory
    STNixSourceRef  (*allocSource)(STNixEngineRef ref);
    STNixBufferRef  (*allocBuffer)(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
    STNixRecorderRef (*allocRecorder)(STNixEngineRef ref, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
} STNixEngineItf;

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixEngineItf_fillMissingMembers(STNixEngineItf* itf);

//STNixBufferItf (API)

typedef struct STNixBufferItf_ {
    STNixBufferRef  (*alloc)(STNixContextRef ctx, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
    void            (*free)(STNixBufferRef ref);
    NixBOOL         (*setData)(STNixBufferRef ref, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
    NixBOOL         (*fillWithZeroes)(STNixBufferRef ref);
} STNixBufferItf;

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixBufferItf_fillMissingMembers(STNixBufferItf* itf);

//STNixSourceItf (API)

typedef struct STNixSourceItf_ {
    STNixSourceRef  (*alloc)(STNixEngineRef eng);
    void            (*free)(STNixSourceRef ref);
    void            (*setCallback)(STNixSourceRef ref, NixSourceCallbackFnc callback, void* callbackData);
    NixBOOL         (*setVolume)(STNixSourceRef ref, const float vol);
    NixBOOL         (*setRepeat)(STNixSourceRef ref, const NixBOOL isRepeat);
    void            (*play)(STNixSourceRef ref);
    void            (*pause)(STNixSourceRef ref);
    void            (*stop)(STNixSourceRef ref);
    NixBOOL         (*isPlaying)(STNixSourceRef ref);
    NixBOOL         (*isPaused)(STNixSourceRef ref);
    NixBOOL         (*isRepeat)(STNixSourceRef ref);
    NixFLOAT        (*getVolume)(STNixSourceRef ref);
    NixBOOL         (*setBuffer)(STNixSourceRef ref, STNixBufferRef buff);  //static-source
    NixBOOL         (*queueBuffer)(STNixSourceRef ref, STNixBufferRef buff); //stream-source
    NixBOOL         (*setBufferOffset)(STNixSourceRef ref, const ENNixOffsetType type, const NixUI32 offset); //relative to first buffer in queue
    NixUI32         (*getBuffersCount)(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);   //all buffer queue
    NixUI32         (*getBlocksOffset)(STNixSourceRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);  //relative to first buffer in queue
} STNixSourceItf;

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixSourceItf_fillMissingMembers(STNixSourceItf* itf);

//STNixRecorderItf (API)

typedef struct STNixRecorderItf_ {
    STNixRecorderRef (*alloc)(STNixEngineRef eng, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer);
    void            (*free)(STNixRecorderRef ref);
    NixBOOL         (*setCallback)(STNixRecorderRef ref, NixRecorderCallbackFnc callback, void* callbackData);
    NixBOOL         (*start)(STNixRecorderRef ref);
    NixBOOL         (*stop)(STNixRecorderRef ref);
    NixBOOL         (*flush)(STNixRecorderRef ref, const NixBOOL includeCurrentPartialBuff, const NixBOOL discardWithoutNotifying);
    NixBOOL         (*isCapturing)(STNixRecorderRef ref);
    NixUI32         (*getBuffersFilledCount)(STNixRecorderRef ref, NixUI32* optDstBytesCount, NixUI32* optDstBlocksCount, NixUI32* optDstMsecsCount);
} STNixRecorderItf;

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixRecorderItf_fillMissingMembers(STNixRecorderItf* itf);

//STNixApiItf (API)

typedef struct STNixApiItf_ {
    STNixEngineItf   engine;
    STNixBufferItf   buffer;
    STNixSourceItf   source;
    STNixRecorderItf recorder;
} STNixApiItf;

//Links NULL methods to a NOP implementation,
//this reduces the need to check for functions NULL pointers.
void NixApiItf_fillMissingMembers(STNixApiItf* itf);

//------
//PCMBuffer
//------

typedef struct STNixPCMBuffer_ {
    STNixContextRef ctx;
    NixUI8*         ptr;
    NixUI32         use;
    NixUI32         sz;
    STNixAudioDesc  desc;
} STNixPCMBuffer;

void NixPCMBuffer_init(STNixContextRef ctx, STNixPCMBuffer* obj);
void NixPCMBuffer_destroy(STNixPCMBuffer* obj);
NixBOOL NixPCMBuffer_setData(STNixPCMBuffer* obj, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL NixPCMBuffer_fillWithZeroes(STNixPCMBuffer* obj);

//------
//Notif (internal)
//------

typedef struct STNixSourceCallback_ {
    NixSourceCallbackFnc    func;
    void*                   data;
} STNixSourceCallback;

#ifdef NIX_DEBUG
#   define NIX_SOURCE_NOTIF_BUFFS_MAX  2
#else
#   define NIX_SOURCE_NOTIF_BUFFS_MAX  16
#endif

typedef struct STNixSourceNotif_ {
    STNixSourceRef      source;
    STNixSourceCallback callback;
    STNixBufferRef      buffs[NIX_SOURCE_NOTIF_BUFFS_MAX];  //this struct is used internally only, changing this value should not affect already compile code.
    NixUI32             buffsUse;
} STNixSourceNotif;

void NixSourceNotif_init(STNixSourceNotif* obj, STNixSourceRef src, const STNixSourceCallback callback);
void NixSourceNotif_destroy(STNixSourceNotif* obj);
NixBOOL NixSourceNotif_addBuff(STNixSourceNotif* obj, STNixBufferRef buff);

//------
//NotifQueue (internal)
//------

typedef struct STNixNotifQueue_ {
    STNixContextRef     ctx;
    STNixSourceNotif*   arr;
    NixUI32             use;
    NixUI32             sz;
    STNixSourceNotif    arrEmbedded[32];
} STNixNotifQueue;

void NixNotifQueue_init(STNixContextRef ctx, STNixNotifQueue* obj);
void NixNotifQueue_destroy(STNixNotifQueue* obj);
//
NixBOOL NixNotifQueue_addBuff(STNixNotifQueue* obj, STNixSourceRef src, const STNixSourceCallback callback, STNixBufferRef buff);

//------
//PCMBuffer (API)
//------

NixBOOL NixPCMBuffer_getApiItf(STNixBufferItf* dst);

//------
//PCMFormat converter
//------
// * 1 or 2 channels only
// * float32, int32, int16 or uint8 sample-types only
// * from-to any frequency
//
void*   NixFmtConverter_alloc(STNixContextRef ctx);
void    NixFmtConverter_free(void* obj);
NixBOOL NixFmtConverter_prepare(void* obj, const STNixAudioDesc* srcDesc, const STNixAudioDesc* dstDesc);
NixBOOL NixFmtConverter_setPtrAtSrc(void* obj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign); //one channel
NixBOOL NixFmtConverter_setPtrAtDst(void* obj, const NixUI32 iChannel, void* ptr, const NixUI32 sampleAlign); //one channel
NixBOOL NixFmtConverter_setPtrAtSrcInterlaced(void* obj, const STNixAudioDesc* desc, void* ptr, const NixUI32 iFirstSample); //all channels at once
NixBOOL NixFmtConverter_setPtrAtDstInterlaced(void* obj, const STNixAudioDesc* desc, void* ptr, const NixUI32 iFirstSample); //all channels at once
NixBOOL NixFmtConverter_convert(void* obj, const NixUI32 srcBlocks, NixUI32 dstBlocks, NixUI32* dstAmmBlocksRead, NixUI32* dstAmmBlocksWritten);
//
NixUI32 NixFmtConverter_maxChannels(void); //= 2, defined at compile-time
NixUI32 NixFmtConverter_blocksForNewFrequency(const NixUI32 ammSampesOrg, const NixUI32 freqOrg, const NixUI32 freqNew); //ammount of output samples from one frequeny to another, +1 for safety

//Default API

NixBOOL NixApiItf_getDefaultApiForCurrentOS(STNixApiItf* dst);


/*
//-------------------------------
//-- ENGINES
//-------------------------------
	
//Engine object
typedef struct STNix_Engine_ {
	void* o; //Abstract objects pointer
} STNix_Engine;

//Engine status
typedef struct STNix_StatusDesc_ {
	//Sources
	NixUI16	countSources;			//All sources
	NixUI16	countSourcesReusable;	//Only reusable sources. Not-reusable = (countSources - countSourcesReusable);
	NixUI16	countSourcesAssigned;	//Only sources retained by ussers. Not-assigned = (countSources - countSourcesAssigned);
	NixUI16	countSourcesStatic;		//Only static sounds sources.
	NixUI16	countSourcesStream;		//Only stream sounds sources. Undefined-sources = (countSources - countSourcesStatic - countSourcesStream);
	//PLay buffers
	NixUI16	countPlayBuffers;		//All play-buffers
	NixUI32	sizePlayBuffers;		//Bytes of all play-buffers
	NixUI32	sizePlayBuffersAtSW;	//Only bytes of play-buffers that resides in nixtla's memory. sizeBuffersAtExternal = (sizeBuffers - sizeBuffersAtSW); //this includes Hardware-buffers
	//Record buffers
	NixUI16	countRecBuffers;		//All rec-buffers
	NixUI32	sizeRecBuffers;			//Bytes of all rec-buffers
	NixUI32	sizeRecBuffersAtSW;		//Only bytes of rec-buffers that resides in nixtla's memory. sizeBuffersAtExternal = (sizeBuffers - sizeBuffersAtSW); //this includes Hardware-buffers
} STNix_StatusDesc;

	
//Callbacks
typedef void (*PTRNIX_SourceReleaseCallback)(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex);
typedef void (*PTRNIX_StreamBufferUnqueuedCallback)(STNix_Engine* engAbs, void* userdata, const NixUI32 sourceIndex, const NixUI16 buffersUnqueuedCount);
typedef void (*PTRNIX_CaptureBufferFilledCallback)(STNix_Engine* engAbs, void* userdata, const STNixAudioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 blocksCount);

//Engine
NixBOOL		nixInit(STNixContextRef ctx, STNix_Engine* engAbs, const NixUI16 pregeneratedSources);
//NixBOOL   nixInitWithOpenAL(STNix_Engine* engAbs, const NixUI16 pregeneratedSources);     //Cross-platform
//NixBOOL   nixInitWithAVFAudio(STNix_Engine* engAbs, const NixUI16 pregeneratedSources);   //Mac/iOS
//NixBOOL   nixInitWithAAudio(STNix_Engine* engAbs, const NixUI16 pregeneratedSources);     //Android
void		nixFinalize(STNix_Engine* engAbs);
NixBOOL		nixContextIsActive(STNix_Engine* engAbs);
NixBOOL		nixctxActivate(STNix_Engine* engAbs);
NixBOOL		nixctxDeactivate(STNix_Engine* engAbs);
//void		nixGetContext(STNix_Engine* engAbs, void* dest); //deprecated and removed: 2025-07-16
void		nixGetStatusDesc(STNix_Engine* engAbs, STNix_StatusDesc* dest);
NixUI32		nixCapabilities(STNix_Engine* engAbs);
void		nixPrintCaps(STNix_Engine* engAbs);
void		nixTick(STNix_Engine* engAbs);

//Buffers
NixUI16		NixEngine_allocBuffer(STNix_Engine* engAbs, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL		nixBufferSetData(STNix_Engine* engAbs, const NixUI16 buffIndex, const STNixAudioDesc* audioDesc, const NixUI8* audioDataPCM, const NixUI32 audioDataPCMBytes);
NixBOOL     nixBufferFillZeroes(STNix_Engine* engAbs, const NixUI16 buffIndex); //the unused range of the bufer will be zeroed
NixUI32		nixBufferRetainCount(STNix_Engine* engAbs, const NixUI16 buffIndex);
void		nixBufferRetain(STNix_Engine* engAbs, const NixUI16 buffIndex);
void		NixBuffer_release(STNix_Engine* engAbs, const NixUI16 buffIndex);
float		nixBufferSeconds(STNix_Engine* engAbs, const NixUI16 buffIndex);
STNixAudioDesc nixBufferAudioDesc(STNix_Engine* engAbs, const NixUI16 buffIndex);

//Sources
NixUI16		NixEngine_allocSource(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData);
NixUI16		nixSourceAssignStream(STNix_Engine* engAbs, NixUI8 lookIntoReusable, NixUI8 audioGroupIndex, PTRNIX_SourceReleaseCallback releaseCallBack, void* releaseCallBackUserData, const NixUI16 queueSize, PTRNIX_StreamBufferUnqueuedCallback bufferUnqueueCallback, void* bufferUnqueueCallbackData);
NixUI32		nixSourceRetainCount(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		nixSourceRetain(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		NixSource_release(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		nixSourceSetRepeat(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixBOOL repeat);
N
 ixUI32		nixSourceGetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixUI32		nixSourceGetBytes(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixFLOAT	nixSourceGetSeconds(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixFLOAT	nixSourceGetVoume(STNix_Engine* engAbs, const NixUI16 sourceIndex);
 
void		NixSource_setVolume(STNix_Engine* engAbs, const NixUI16 sourceIndex, const float volume);
 
NixUI16		nixSourceGetBuffersCount(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixUI32		nixSourceGetOffsetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixUI32		nixSourceGetOffsetBytes(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		nixSourceSetOffsetSamples(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI32 offsetSamples);
 
void		NixSource_play(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixBOOL		nixSourceIsPlaying(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		nixSourcePause(STNix_Engine* engAbs, const NixUI16 sourceIndex);
void		nixSourceStop(STNix_Engine* engAbs, const NixUI16 sourceIndex);

 void		nixSourceRewind(STNix_Engine* engAbs, const NixUI16 sourceIndex);
N
 ixBOOL		NixSource_setBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 bufferIndex);
NixBOOL		nixSourceStreamAppendBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 streamBufferIndex);
NixBOOL		nixSourceEmptyQueue(STNix_Engine* engAbs, const NixUI16 sourceIndex);
NixBOOL		nixSourceHaveBuffer(STNix_Engine* engAbs, const NixUI16 sourceIndex, const NixUI16 bufferIndex);
	
//Audio groups
NixBOOL		nixSrcGroupIsEnabled(STNix_Engine* engAbs, const NixUI8 groupIndex);
NixFLOAT	nixSrcGroupGetVolume(STNix_Engine* engAbs, const NixUI8 groupIndex);
void		nixSrcGroupSetEnabled(STNix_Engine* engAbs, const NixUI8 groupIndex, const NixBOOL enabled);
void		nixSrcGroupSetVolume(STNix_Engine* engAbs, const NixUI8 groupIndex, const NixFLOAT volume);

//Capture
NixBOOL		nixCaptureInit(STNix_Engine* engAbs, const STNixAudioDesc* audioDesc, const NixUI16 buffersCount, const NixUI16 blocksPerBuffer, PTRNIX_CaptureBufferFilledCallback bufferCaptureCallback, void* bufferCaptureCallbackUserData);
void		nixCaptureFinalize(STNix_Engine* engAbs);
NixBOOL		nixCaptureIsOnProgress(STNix_Engine* engAbs);
NixBOOL		nixCaptureStart(STNix_Engine* engAbs);
void		nixCaptureStop(STNix_Engine* engAbs);
NixUI32		nixCaptureFilledBuffersCount(STNix_Engine* engAbs);
NixUI32		nixCaptureFilledBuffersSamples(STNix_Engine* engAbs);
float		nixCaptureFilledBuffersSeconds(STNix_Engine* engAbs);
//Note: when a BufferFilledCallback is defined, the buffers are automatically released after invoking the callback.
void		nixCaptureFilledBuffersRelease(STNix_Engine* engAbs, NixUI32 quantBuffersToRelease);

//Debug
void		nixDbgPrintSourcesStatus(STNix_Engine* engAbs);
*/

#ifdef __cplusplus
}
#endif
		
#endif
