//
//  NixTestSamplesConverter.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 20/07/25.
//  Copyright (c) 2014 Marcos Ortega. All rights reserved.
//

//
// This test loads a WAV's PCM samples, and switches between playing
// the original sound and playing a randomly converted version of it.
//

#include "NixTestSamplesConverter.h"
#include "../utils/utilFilesList.h"
#include "../utils/utilLoadWav.h"
//
#include <stdio.h>  //printf
#include <stdlib.h> //rand

NixBOOL NixTestSamplesConverter_isNull(STNixTestSamplesConverter* obj){
    return NixSource_isNull(obj->org.src);
}

NixBOOL NixTestSamplesConverter_init(
                                     STNixTestSamplesConverter* obj, STNixDemosCommon* common
#                                     ifdef __ANDROID__
                                     , JNIEnv *env
                                     , jobject assetManager
#                                    endif
                                     )
{
    NixBOOL r = NIX_FALSE;
    //randomly select a wav from the list
    const char* strWavPath = _nixUtilFilesList[rand() % (sizeof(_nixUtilFilesList) / sizeof(_nixUtilFilesList[0]))];
    if(!nixUtilLoadDataFromWavFile(
#                                  ifdef __ANDROID__
                                   env,
                                   assetManager,
#                                  endif
                                   strWavPath, &obj->org.audioDesc, &obj->org.data, &obj->org.dataSz
                                   )
       )
    {
        NIX_PRINTF_ERROR("ERROR, loading WAV file: '%s'.\n", strWavPath);
    } else {
        NIX_PRINTF_INFO("WAV file loaded: '%s'.\n", strWavPath);
        STNixSourceRef iSrcTmp = NixEngine_allocSource(common->eng);
        if(NixSource_isNull(iSrcTmp)){
            NIX_PRINTF_ERROR("Source assign failed.\n");
        } else {
            STNixBufferRef iBufferWav = NixEngine_allocBuffer(common->eng, &obj->org.audioDesc, obj->org.data, obj->org.dataSz);
            if(NixBuffer_isNull(iBufferWav)){
                NIX_PRINTF_ERROR("Buffer assign failed.\n");
            } else {
                if(!NixSource_setBuffer(iSrcTmp, iBufferWav)){
                    NIX_PRINTF_ERROR("Buffer-to-source linking failed.\n");
                } else {
                    NIX_PRINTF_INFO("origin playing %u time(s).\n", obj->org.playCount + 1);
                    //retain source
                    NixSource_set(&obj->org.src, iSrcTmp);
                    NixSource_setVolume(iSrcTmp, 1.0f);
                    NixSource_play(iSrcTmp);
                    r = NIX_TRUE;
                }
                //release buffer (already retained by source if success)
                NixBuffer_release(&iBufferWav);
                NixBuffer_null(&iBufferWav);
            }
            //release source
            NixSource_release(&iSrcTmp);
            NixSource_null(&iSrcTmp);
        }
    }
    return r;
}

void NixTestSamplesConverter_destroy(STNixTestSamplesConverter* obj, STNixDemosCommon* common){
    if(obj->org.data!=NULL){
        free(obj->org.data);
        obj->org.data = NULL;
    }
    if(obj->conv.data != NULL){
        NixContext_mfree(common->ctx, obj->conv.data);
        obj->conv.data = NULL;
    }
    if(!NixSource_isNull(obj->org.src)){
        NixSource_release(&obj->org.src);
        NixSource_null(&obj->org.src);
    }
    if(!NixSource_isNull(obj->conv.src)){
        NixSource_release(&obj->conv.src);
        NixSource_null(&obj->conv.src);
    }
    *obj = (STNixTestSamplesConverter)STNixTestSamplesConverter_Zero;
}

void NixTestSamplesConverter_tick(STNixTestSamplesConverter* obj, STNixDemosCommon* common){
    //analyze converted playback
    if(!NixSource_isNull(obj->conv.src)){
        if(!NixSource_isPlaying(obj->conv.src)){
            obj->conv.playCount++;
            if(obj->conv.playCount < 2){
                //play again
                NIX_PRINTF_INFO("converted played %u time(s).\n", obj->conv.playCount + 1);
                NixSource_play(obj->conv.src);
            } else {
                obj->conv.playCount = 0;
                //release
                NixSource_release(&obj->conv.src);
                NixSource_null(&obj->conv.src);
                //play original
                obj->org.playCount = 0;
                NIX_PRINTF_INFO("origin playing %u time(s).\n", obj->org.playCount + 1);
                NixSource_play(obj->org.src);
            }
        }
    } else {
        //analyze original playback
        if(!NixSource_isPlaying(obj->org.src)){
            obj->org.playCount++;
            if(obj->org.playCount < 2){
                //play again
                NIX_PRINTF_INFO("origin playing %u time(s).\n", obj->org.playCount + 1);
                NixSource_play(obj->org.src);
            } else {
                obj->org.playCount = 0;
                //create random conversion
                if(!NixSource_isNull(obj->conv.src)){
                    NixSource_release(&obj->conv.src);
                    NixSource_null(&obj->conv.src);
                }
                switch(rand() % 4){
                    case 0:
                        obj->conv.audioDesc.samplesFormat = ENNixSampleFmt_Int;
                        obj->conv.audioDesc.bitsPerSample = 8;
                        break;
                    case 1:
                        obj->conv.audioDesc.samplesFormat = ENNixSampleFmt_Int;
                        obj->conv.audioDesc.bitsPerSample = 16;
                        break;
                    case 2:
                        obj->conv.audioDesc.samplesFormat = ENNixSampleFmt_Int;
                        obj->conv.audioDesc.bitsPerSample = 32;
                        break;
                    default:
                        obj->conv.audioDesc.bitsPerSample = 32;
                        obj->conv.audioDesc.samplesFormat = ENNixSampleFmt_Float;
                        break;
                }
                obj->conv.audioDesc.channels = 1 + (rand() % 2);
                obj->conv.audioDesc.samplerate = (rand() % 10) == 0 ? obj->org.audioDesc.samplerate : 800 + (rand() % 92000);
                obj->conv.audioDesc.blockAlign = (obj->conv.audioDesc.bitsPerSample / 8) * obj->conv.audioDesc.channels;
                const NixUI32 blocksReq = NixFmtConverter_blocksForNewFrequency(obj->org.dataSz / obj->org.audioDesc.blockAlign, obj->org.audioDesc.samplerate, obj->conv.audioDesc.samplerate);
                const NixUI32 bytesReq = blocksReq * obj->conv.audioDesc.blockAlign;
                if(obj->conv.dataSz < bytesReq){
                    if(obj->conv.data != NULL){
                        NixContext_mfree(common->ctx, obj->conv.data);
                        obj->conv.data = NULL;
                    }
                    obj->conv.dataSz = 0;
                    obj->conv.data = (NixUI8*)NixContext_malloc(common->ctx, bytesReq, "obj->conv.data");
                    if(obj->conv.data != NULL){
                        obj->conv.dataSz = bytesReq;
                        NIX_PRINTF_INFO("Conversion buffer resized to %u bytes.\n", obj->conv.dataSz);
                    }
                }
                if(obj->conv.data != NULL){
                    void* conv = NixFmtConverter_alloc(common->ctx);
                    if(conv != NULL){
                        const NixUI32 srcBlocks = (obj->org.dataSz / obj->org.audioDesc.blockAlign);
                        const NixUI32 dstBlocks = blocksReq;
                        NixUI32 ammBlocksRead = 0;
                        NixUI32 ammBlocksWritten = 0;
                        if(!NixFmtConverter_prepare(conv, &obj->org.audioDesc, &obj->conv.audioDesc)){
                            NIX_PRINTF_ERROR("NixFmtConverter_prepare failed.\n");
                        } else if(!NixFmtConverter_setPtrAtSrcInterlaced(conv, &obj->org.audioDesc, obj->org.data, 0)){
                            NIX_PRINTF_ERROR("NixFmtConverter_setPtrAtSrcInterlaced failed.\n");
                        } else if(!NixFmtConverter_setPtrAtDstInterlaced(conv, &obj->conv.audioDesc, obj->conv.data, 0)){
                            NIX_PRINTF_ERROR("NixFmtConverter_setPtrAtDstInterlaced failed.\n");
                        } else if(!NixFmtConverter_convert(conv, srcBlocks, dstBlocks, &ammBlocksRead, &ammBlocksWritten)){
                            NIX_PRINTF_ERROR("NixFmtConverter_convert failed.\n");
                        } else {
                            NIX_PRINTF_INFO("NixFmtConverter_convert transformed %u of %u samples (%u%%) (%u hz, %d bits, %d channels) to %u of %u samples(%u%%) (%u hz, %d bits, %d channels, %s).\n", ammBlocksRead, srcBlocks, ammBlocksRead * 100 / srcBlocks, obj->org.audioDesc.samplerate, obj->org.audioDesc.bitsPerSample, obj->org.audioDesc.channels, ammBlocksWritten, dstBlocks, ammBlocksWritten * 100 / dstBlocks, obj->conv.audioDesc.samplerate, obj->conv.audioDesc.bitsPerSample, obj->conv.audioDesc.channels, obj->conv.audioDesc.samplesFormat == ENNixSampleFmt_Float ? "float" : "int");
                            //obj->conv.src
                            STNixSourceRef iSrcConv = NixEngine_allocSource(common->eng);
                            if(NixSource_isNull(iSrcConv)){
                                NIX_PRINTF_ERROR("Source assign failed.\n");
                            } else {
                                STNixBufferRef iBufferWav = NixEngine_allocBuffer(common->eng, &obj->conv.audioDesc, obj->conv.data, ammBlocksWritten * obj->conv.audioDesc.blockAlign);
                                if(NixBuffer_isNull(iBufferWav)){
                                    NIX_PRINTF_ERROR("Buffer assign failed.\n");
                                } else {
                                    if(!NixSource_setBuffer(iSrcConv, iBufferWav)){
                                        NIX_PRINTF_ERROR("Buffer-to-source linking failed.\n");
                                    } else {
                                        obj->conv.playCount = 0;
                                        //assign as new source
                                        NixSource_set(&obj->conv.src, iSrcConv);
                                        //
                                        NIX_PRINTF_INFO("converted played %u time(s).\n", obj->conv.playCount + 1);
                                        NixSource_setVolume(iSrcConv, 1.0f);
                                        NixSource_play(iSrcConv);
                                    }
                                    //release buffer (already retained by source if success)
                                    NixBuffer_release(&iBufferWav);
                                    NixBuffer_null(&iBufferWav);
                                }
                                NixSource_release(&iSrcConv);
                                NixSource_null(&iSrcConv);
                            }
                        }
                        NixFmtConverter_free(conv);
                        conv = NULL;
                    }
                }
            }
        }
    }
}
