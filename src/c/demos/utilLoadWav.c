//
//  main.c
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#ifndef NIXTLA_UTIL_LOAD_WAV_H
#define NIXTLA_UTIL_LOAD_WAV_H

#include "nixtla-audio.h"
#include <stdio.h>
#include <malloc.h>

int loadDataFromWavFile(const char* pathToWav, STNix_audioDesc* audioDesc, NixUI8** audioData, NixUI32* audioDataBytes){
	int success = 0;
	FILE* wavFile = fopen(pathToWav, "rb");
	if(wavFile==NULL){
		printf("ERROR, fopen failed for: '%s'\n", pathToWav);
	} else {
		char chunckID[4]; fread(chunckID, sizeof(char), 4, wavFile);
		if(!(chunckID[0]!='R' || chunckID[1]!='I' || chunckID[2]!='F' || chunckID[3]!='F')){
			NixUI8 continuarLectura	= 1;
			NixUI8 errorOpeningFile = 0;
			NixUI32 chunckSize;
			char waveID[4];
			NixUI16 formato;
			fread(&chunckSize, sizeof(chunckSize), 1, wavFile);
			fread(waveID, sizeof(char), 4, wavFile);
			if(!(waveID[0]!='W' || waveID[1]!='A' || waveID[2]!='V' || waveID[3]!='E')){
				//Leer los subchuncks de WAVE
				char bufferPadding[64]; int tamBufferPadding = 64; //Optimizacion para evitar el uso de fseek
				char subchunckID[4]; NixUI32 bytesReadedID = 0;
				NixUI8 formatChunckPresent = 0, chunckDataReaded = 0;
				do {
					bytesReadedID = (NixUI32)fread(subchunckID, sizeof(char), 4, wavFile);
					if(bytesReadedID==4){
						NixUI32 subchunckBytesReaded = 0;
						NixUI32 subchunckSize;
						NixUI8 tamanoChunckEsImpar;
						fread(&subchunckSize, sizeof(subchunckSize), 1, wavFile);  //subchunckBytesReaded += sizeof(subchunckSize);
						tamanoChunckEsImpar = ((subchunckSize % 2)!=0);
						if(subchunckID[0]=='f' && subchunckID[1]=='m' && subchunckID[2]=='t' && subchunckID[3]==' '){
							//printf("WAV: Procesando subchunck: '%c%c%c%c' (%u bytes)\n", subchunckID[0], subchunckID[1], subchunckID[2], subchunckID[3], subchunckSize);
							if(!formatChunckPresent){
								//
								fread(&formato, sizeof(formato), 1, wavFile); subchunckBytesReaded += sizeof(formato);
								if(formato!=1 && formato!=3){ //WAVE_FORMAT_PCM=1 WAVE_FORMAT_IEEE_FLOAT=3
									errorOpeningFile = 1;
									printf("ERROR, Wav format(%d) is not WAVE_FORMAT_PCM(1) or WAVE_FORMAT_IEEE_FLOAT(3)\n", formato);
								} else {
									NixUI16	canales;
									NixUI32	muestrasPorSegundo;
									NixUI32	bytesPromedioPorSegundo;
									NixUI16	alineacionBloques;
									NixUI16	bitsPorMuestra;
									fread(&canales, sizeof(canales), 1, wavFile);									subchunckBytesReaded += sizeof(canales);
									fread(&muestrasPorSegundo, sizeof(muestrasPorSegundo), 1, wavFile);				subchunckBytesReaded += sizeof(muestrasPorSegundo);
									fread(&bytesPromedioPorSegundo, sizeof(bytesPromedioPorSegundo), 1, wavFile);	subchunckBytesReaded += sizeof(bytesPromedioPorSegundo);
									fread(&alineacionBloques, sizeof(alineacionBloques), 1, wavFile);				subchunckBytesReaded += sizeof(alineacionBloques);
									fread(&bitsPorMuestra, sizeof(bitsPorMuestra), 1, wavFile);						subchunckBytesReaded += sizeof(bitsPorMuestra);
									if(formato==3 && bitsPorMuestra!=32){
										errorOpeningFile = 1;
										printf("ERROR, WAVE_FORMAT_IEEE_FLOAT expected 32 bits per sample (%d bits instaed)\n", bitsPorMuestra);
									} else {
										//if((canales!=1 && canales!=2) || (bitsPorMuestra!=8 && bitsPorMuestra!=16 && bitsPorMuestra!=32) ||  (muestrasPorSegundo!=11025 && muestrasPorSegundo!=22050 && muestrasPorSegundo!=44100)){
										//	errorOpeningFile = 1;
										//	printf("ERROR, Wav format not supported\n");
										//} else {
										//
											audioDesc->samplesFormat	= (formato==3 ? ENNix_sampleFormat_float : ENNix_sampleFormat_int);
											audioDesc->channels			= canales;
											audioDesc->bitsPerSample	= bitsPorMuestra;
											audioDesc->samplerate		= muestrasPorSegundo;
											audioDesc->blockAlign		= alineacionBloques;
										//}
										formatChunckPresent = 1;
									}
								}
							}
						} else if(subchunckID[0]=='d' && subchunckID[1]=='a' && subchunckID[2]=='t' && subchunckID[3]=='a') {
							if(!formatChunckPresent){
								//WARNING
							} else if(chunckDataReaded){
								//WARNING
							} else {
								NixUI32 pcmDataBytes;
								NixUI32 bytesReaded;
								pcmDataBytes			= subchunckSize;
								*audioDataBytes			= pcmDataBytes; //printf("Tamano chunck PCM: %u\n", pcmDataBytes);
								if(*audioData!=NULL) free(*audioData);
								*audioData				= (NixUI8*)malloc(pcmDataBytes);
								bytesReaded				= (NixUI32)fread(*audioData, sizeof(NixUI8), pcmDataBytes, wavFile);
								subchunckBytesReaded	+= bytesReaded;
								if(bytesReaded!=pcmDataBytes){
									//WARNING
								}
								chunckDataReaded = 1;
							}
						} else {
							if(chunckDataReaded){
								continuarLectura = 0;
							} else {
								fseek(wavFile, subchunckSize, SEEK_CUR);
								subchunckBytesReaded += subchunckSize;
							}
						}
						//Validar la cantidad de bytes leidos y el tamano del subchunck
						if(!errorOpeningFile && continuarLectura && subchunckBytesReaded!=subchunckSize){
							if(subchunckBytesReaded<subchunckSize){
								int bytesPaddear = (subchunckSize-subchunckBytesReaded);
								if(bytesPaddear<=tamBufferPadding){
									fread(bufferPadding, sizeof(char), bytesPaddear, wavFile); //Optimizacion para evitar el uso de fseek
								} else {
									fseek(wavFile, subchunckSize-subchunckBytesReaded, SEEK_CUR);
								}
							} else {
								errorOpeningFile = 1;
							}
						}
						//padding para tamano par de los subchuncks
						if(!errorOpeningFile && continuarLectura && tamanoChunckEsImpar) {
							char charPadding; fread(&charPadding, sizeof(char), 1, wavFile);
						}
					}
				} while(bytesReadedID==4 && !errorOpeningFile && continuarLectura);
				success = (formatChunckPresent && chunckDataReaded && !errorOpeningFile) ? 1 : 0;
				//if(!formatChunckPresent) printf("formatChunckPresent no leido\n");
				//if(!chunckDataReaded) printf("chunckDataReaded no leido\n");
				//if(errorOpeningFile) printf("errorOpeningFile error presente\n");
			}
		}
		fclose(wavFile);
	}
	return success;
}

#endif
