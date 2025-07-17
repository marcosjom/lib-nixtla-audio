//
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#include "utilLoadWav.h"

#include <stdlib.h>

#ifdef __ANDROID__
#   include <android/asset_manager.h>
#   include <android/asset_manager_jni.h> //for AAssetManager_fromJava
#   include <android/log.h>    //for __android_log_print()
#else
#   include <stdio.h>
#endif

#ifdef __ANDROID__
#   define  NIX_FILE_READ(F, DST, SZ)       AAsset_read(F, DST, SZ)
#   define  NIX_FILE_CLOSE(F)               AAsset_close(F)
#   define  NIX_FILE_SEEK_CUR(F, V)         AAsset_seek(F, V, SEEK_CUR)
#else
#   define  NIX_FILE_READ(F, DST, SZ)       fread(DST, sizeof(char), SZ, F)
#   define  NIX_FILE_CLOSE(F)               fclose(F)
#   define  NIX_FILE_SEEK_CUR(F, V)         fseek(F, V, SEEK_CUR)
#endif

#ifdef __ANDROID__
#   ifndef PRINTF_ERROR
#       define PRINTF_ERROR(STR_FMT, ...)   __android_log_print(ANDROID_LOG_ERROR, "Nixtla", "ERROR, "STR_FMT, ##__VA_ARGS__)
#   endif
#   ifndef PRINTF_WARNING
#       define PRINTF_WARNING(STR_FMT, ...) __android_log_print(ANDROID_LOG_WARN, "Nixtla", "WARNING, "STR_FMT, ##__VA_ARGS__)
#   endif
#else
#   ifndef PRINTF_ERROR
#       define PRINTF_ERROR(STR_FMT, ...)   printf("ERROR, "STR_FMT, ##__VA_ARGS__)
#   endif
#   ifndef PRINTF_WARNING
#       define PRINTF_WARNING(STR_FMT, ...) printf("WARNING, "STR_FMT, ##__VA_ARGS__)
#   endif
#endif

NixUI8 loadDataFromWavFile(
#                           ifdef __ANDROID__
                            JNIEnv *env, jobject assetManager,
#                           endif
                            const char* pathToWav, STNix_audioDesc* audioDesc, NixUI8** audioData, NixUI32* audioDataBytes
                           )
{
    NixUI8 success = 0;
#   ifdef __ANDROID__
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    AAsset* wavFile = AAssetManager_open(mgr, pathToWav, AASSET_MODE_UNKNOWN);
#   else
    FILE* wavFile = fopen(pathToWav, "rb");
#   endif
    if(wavFile==NULL){
        PRINTF_ERROR("WAV fopen failed: '%s'\n", pathToWav);
    } else {
        char chunckID[4]; NIX_FILE_READ(wavFile, chunckID, 4);
        if(chunckID[0] != 'R' || chunckID[1] != 'I' || chunckID[2] != 'F' || chunckID[3] != 'F'){
            PRINTF_ERROR("WAV chunckID not valid: '%s'\n", pathToWav);
        } else {
            NixUI8 continuarLectura    = 1;
            NixUI8 errorOpeningFile = 0;
            NixUI32 chunckSize; NIX_FILE_READ(wavFile, &chunckSize, sizeof(chunckSize) * 1);
            char waveID[4]; NIX_FILE_READ(wavFile, waveID, 4);
            if(waveID[0] != 'W' || waveID[1] != 'A' || waveID[2] != 'V' || waveID[3] != 'E'){
                PRINTF_ERROR("WAV::WAVE chunckID not valid: '%s'\n", pathToWav);
            } else {
                //Leer los subchuncks de WAVE
                char bufferPadding[64]; NixSI32 tamBufferPadding = 64; //Optimizacion para evitar el uso de fseek
                char subchunckID[4]; NixUI32 bytesReadedID = 0;
                NixUI8 formatChunckPresent = 0, chunckDataReaded = 0;
                do {
                    bytesReadedID = (NixUI32)NIX_FILE_READ(wavFile, subchunckID, 4);
                    if(bytesReadedID==4){
                        NixUI32 subchunckBytesReaded = 0;
                        NixUI32 subchunckSize; NIX_FILE_READ(wavFile, &subchunckSize, sizeof(subchunckSize) * 1);  //subchunckBytesReaded += sizeof(subchunckSize);
                        NixUI8 tamanoChunckEsImpar = ((subchunckSize % 2) != 0);
                        if(subchunckID[0] == 'f' && subchunckID[1] == 'm' && subchunckID[2] == 't' && subchunckID[3] == ' '){
                            if(!formatChunckPresent){
                                //
                                NixUI16 formato; NIX_FILE_READ(wavFile, &formato, sizeof(formato) * 1); subchunckBytesReaded += sizeof(formato);
                                if(formato!=1 && formato!=3){ //WAVE_FORMAT_PCM=1 WAVE_FORMAT_IEEE_FLOAT=3
                                    errorOpeningFile = 1;
                                    PRINTF_ERROR("Wav format(%d) is not WAVE_FORMAT_PCM(1) or WAVE_FORMAT_IEEE_FLOAT(3)\n", formato);
                                } else {
                                    NixUI16    canales;                    NIX_FILE_READ(wavFile, &canales, sizeof(canales) * 1);                                    subchunckBytesReaded += sizeof(canales);
                                    NixUI32    muestrasPorSegundo;         NIX_FILE_READ(wavFile, &muestrasPorSegundo, sizeof(muestrasPorSegundo) * 1);                subchunckBytesReaded += sizeof(muestrasPorSegundo);
                                    NixUI32    bytesPromedioPorSegundo;    NIX_FILE_READ(wavFile, &bytesPromedioPorSegundo, sizeof(bytesPromedioPorSegundo) * 1);    subchunckBytesReaded += sizeof(bytesPromedioPorSegundo);
                                    NixUI16    alineacionBloques;          NIX_FILE_READ(wavFile, &alineacionBloques, sizeof(alineacionBloques) * 1);                subchunckBytesReaded += sizeof(alineacionBloques);
                                    NixUI16    bitsPorMuestra;             NIX_FILE_READ(wavFile, &bitsPorMuestra, sizeof(bitsPorMuestra) * 1);                        subchunckBytesReaded += sizeof(bitsPorMuestra);
                                    //if((canales!=1 && canales!=2) || (bitsPorMuestra!=8 && bitsPorMuestra!=16 && bitsPorMuestra!=32) ||  (muestrasPorSegundo!=8000 && muestrasPorSegundo!=11025 && muestrasPorSegundo!=22050 && muestrasPorSegundo!=44100)){
                                    //    errorOpeningFile = 1;
                                    //    PRINTF_ERROR("Wav format not supported\n");
                                    //} else {
                                        //
                                        audioDesc->samplesFormat    = (formato==3 ? ENNix_sampleFormat_float : ENNix_sampleFormat_int);
                                        audioDesc->channels         = canales;
                                        audioDesc->bitsPerSample    = bitsPorMuestra;
                                        audioDesc->samplerate       = muestrasPorSegundo;
                                        audioDesc->blockAlign       = alineacionBloques;
                                    //}
                                    formatChunckPresent = 1;
                                }
                            }
                        } else if(subchunckID[0] == 'd' && subchunckID[1] == 'a' && subchunckID[2] == 't' && subchunckID[3] == 'a') {
                            if(!formatChunckPresent){
                                //WARNING
                            } else if(chunckDataReaded){
                                //WARNING
                            } else {
                                NixUI32 pcmDataBytes        = subchunckSize;
                                *audioDataBytes            = pcmDataBytes; //printf("Tamano chunck PCM: %u\n", pcmDataBytes);
                                if(*audioData!=NULL) free(*audioData);
                                *audioData                = (NixUI8*)malloc(pcmDataBytes);
                                NixUI32 bytesReaded        = (NixUI32)NIX_FILE_READ(wavFile, *audioData, sizeof(NixUI8) * pcmDataBytes);
                                subchunckBytesReaded    += bytesReaded;
                                if(bytesReaded!=pcmDataBytes){
                                    //WARNING
                                }
                                chunckDataReaded = 1;
                            }
                        } else {
                            if(chunckDataReaded){
                                continuarLectura = 0;
                            } else {
                                NIX_FILE_SEEK_CUR(wavFile, subchunckSize);
                                subchunckBytesReaded += subchunckSize;
                            }
                        }
                        //Validar la cantidad de bytes leidos y el tamano del subchunck
                        if(!errorOpeningFile && continuarLectura && subchunckBytesReaded!=subchunckSize){
                            if(subchunckBytesReaded<subchunckSize){
                                NixSI32 bytesPaddear = (subchunckSize-subchunckBytesReaded);
                                if(bytesPaddear<=tamBufferPadding){
                                    NIX_FILE_READ(wavFile, bufferPadding, bytesPaddear); //Optimizacion para evitar el uso de fseek
                                } else {
                                    NIX_FILE_SEEK_CUR(wavFile, subchunckSize-subchunckBytesReaded);
                                }
                            } else {
                                errorOpeningFile = 1;
                            }
                        }
                        //padding para tamano par de los subchuncks
                        if(!errorOpeningFile && continuarLectura && tamanoChunckEsImpar) {
                            char charPadding; NIX_FILE_READ(wavFile, &charPadding, 1);
                        }
                    }
                } while(bytesReadedID == 4 && !errorOpeningFile && continuarLectura);
                success = (formatChunckPresent && chunckDataReaded && !errorOpeningFile) ? 1 : 0;
                if(!formatChunckPresent) PRINTF_WARNING("formatChunckPresent no leido\n");
                if(!chunckDataReaded) PRINTF_WARNING("chunckDataReaded no leido\n");
                if(errorOpeningFile) PRINTF_WARNING("errorOpeningFile error presente\n");
            }
        }
        NIX_FILE_CLOSE(wavFile);
    }
    return success;
}
