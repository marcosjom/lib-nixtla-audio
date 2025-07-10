//
//  NixtlaDemo
//
//  Created by Marcos Ortega on 11/02/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#ifndef NIXTLA_UTIL_LOAD_WAV_ANDROID_H
#define NIXTLA_UTIL_LOAD_WAV_ANDROID_H

NixUI8 loadDataFromWavFile(JNIEnv *env, jobject assetManager, const char* pathToWav, STNix_audioDesc* audioDesc, NixUI8** audioData, NixUI32* audioDataBytes){
	NixUI8 success = 0;
	AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
	AAsset* wavFile = AAssetManager_open(mgr, pathToWav, AASSET_MODE_UNKNOWN);
	//FILE* wavFile = fopen(pathToWav, "rb");
	if(wavFile==NULL){
		PRINTF_ERROR("WAV fopen failed: '%s'\n", pathToWav);
	} else {
		char chunckID[4]; AAsset_read(wavFile, chunckID, sizeof(char) * 4);
		if(chunckID[0]!='R' || chunckID[1]!='I' || chunckID[2]!='F' || chunckID[3]!='F'){
			PRINTF_ERROR("WAV chunckID not valid: '%s'\n", pathToWav);
		} else {
			NixUI8 continuarLectura	= 1;
			NixUI8 errorOpeningFile = 0;
			NixUI32 chunckSize; AAsset_read(wavFile, &chunckSize, sizeof(chunckSize) * 1);
			char waveID[4]; AAsset_read(wavFile, waveID, sizeof(char) * 4);
			if(waveID[0]!='W' || waveID[1]!='A' || waveID[2]!='V' || waveID[3]!='E'){
				PRINTF_ERROR("WAV::WAVE chunckID not valid: '%s'\n", pathToWav);
			} else {
				//Leer los subchuncks de WAVE
				char bufferPadding[64]; NixSI32 tamBufferPadding = 64; //Optimizacion para evitar el uso de fseek
				char subchunckID[4]; NixUI32 bytesReadedID = 0;
				NixUI8 formatChunckPresent = 0, chunckDataReaded = 0;
				do {
					bytesReadedID = (NixUI32)AAsset_read(wavFile, subchunckID, sizeof(char) * 4);
					if(bytesReadedID==4){
						NixUI32 subchunckBytesReaded = 0;
						NixUI32 subchunckSize; AAsset_read(wavFile, &subchunckSize, sizeof(subchunckSize) * 1);  //subchunckBytesReaded += sizeof(subchunckSize);
						NixUI8 tamanoChunckEsImpar = ((subchunckSize % 2) != 0);
						if(subchunckID[0]=='f' && subchunckID[1]=='m' && subchunckID[2]=='t' && subchunckID[3]==' '){
							if(!formatChunckPresent){
								//
								NixUI16 formato; AAsset_read(wavFile, &formato, sizeof(formato) * 1); subchunckBytesReaded += sizeof(formato);
								if(formato!=1 && formato!=3){ //WAVE_FORMAT_PCM=1 WAVE_FORMAT_IEEE_FLOAT=3
									errorOpeningFile = 1;
									PRINTF_ERROR("Wav format(%d) is not WAVE_FORMAT_PCM(1) or WAVE_FORMAT_IEEE_FLOAT(3)\n", formato);
								} else {
									NixUI16	canales;					AAsset_read(wavFile, &canales, sizeof(canales) * 1);									subchunckBytesReaded += sizeof(canales);
									NixUI32	muestrasPorSegundo;			AAsset_read(wavFile, &muestrasPorSegundo, sizeof(muestrasPorSegundo) * 1);				subchunckBytesReaded += sizeof(muestrasPorSegundo);
									NixUI32	bytesPromedioPorSegundo;	AAsset_read(wavFile, &bytesPromedioPorSegundo, sizeof(bytesPromedioPorSegundo) * 1);	subchunckBytesReaded += sizeof(bytesPromedioPorSegundo);
									NixUI16	alineacionBloques;			AAsset_read(wavFile, &alineacionBloques, sizeof(alineacionBloques) * 1);				subchunckBytesReaded += sizeof(alineacionBloques);
									NixUI16	bitsPorMuestra;				AAsset_read(wavFile, &bitsPorMuestra, sizeof(bitsPorMuestra) * 1);						subchunckBytesReaded += sizeof(bitsPorMuestra);
									//if((canales!=1 && canales!=2) || (bitsPorMuestra!=8 && bitsPorMuestra!=16 && bitsPorMuestra!=32) ||  (muestrasPorSegundo!=8000 && muestrasPorSegundo!=11025 && muestrasPorSegundo!=22050 && muestrasPorSegundo!=44100)){
									//	errorOpeningFile = 1;
									//	PRINTF_ERROR("Wav format not supported\n");
									//} else {
										//
										audioDesc->samplesFormat		= (formato==3 ? ENNix_sampleFormat_float : ENNix_sampleFormat_int);
										audioDesc->channels				= canales;
										audioDesc->bitsPerSample		= bitsPorMuestra;
										audioDesc->samplerate			= muestrasPorSegundo;
										audioDesc->blockAlign			= alineacionBloques;
									//}
									formatChunckPresent = 1;
								}
							}
						} else if(subchunckID[0]=='d' && subchunckID[1]=='a' && subchunckID[2]=='t' && subchunckID[3]=='a') {
							if(!formatChunckPresent){
								//WARNING
							} else if(chunckDataReaded){
								//WARNING
							} else {
								NixUI32 pcmDataBytes		= subchunckSize;
								*audioDataBytes			= pcmDataBytes; //printf("Tamano chunck PCM: %u\n", pcmDataBytes);
								if(*audioData!=NULL) free(*audioData);
								*audioData				= (NixUI8*)malloc(pcmDataBytes);
								NixUI32 bytesReaded		= (NixUI32)AAsset_read(wavFile, *audioData, sizeof(NixUI8) * pcmDataBytes);
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
								AAsset_seek(wavFile, subchunckSize, SEEK_CUR);
								subchunckBytesReaded += subchunckSize;
							}
						}
						//Validar la cantidad de bytes leidos y el tamano del subchunck
						if(!errorOpeningFile && continuarLectura && subchunckBytesReaded!=subchunckSize){
							if(subchunckBytesReaded<subchunckSize){
								NixSI32 bytesPaddear = (subchunckSize-subchunckBytesReaded);
								if(bytesPaddear<=tamBufferPadding){
									AAsset_read(wavFile, bufferPadding, sizeof(char) * bytesPaddear); //Optimizacion para evitar el uso de fseek
								} else {
									AAsset_seek(wavFile, subchunckSize-subchunckBytesReaded, SEEK_CUR);
								}
							} else {
								errorOpeningFile = 1;
							}
						}
						//padding para tamano par de los subchuncks
						if(!errorOpeningFile && continuarLectura && tamanoChunckEsImpar) {
							char charPadding; AAsset_read(wavFile, &charPadding, sizeof(char) * 1);
						}
					}
				} while(bytesReadedID==4 && !errorOpeningFile && continuarLectura);
				success = (formatChunckPresent && chunckDataReaded && !errorOpeningFile) ? 1 : 0;
				if(!formatChunckPresent) PRINTF_WARNING("formatChunckPresent no leido\n");
				if(!chunckDataReaded) PRINTF_WARNING("chunckDataReaded no leido\n");
				if(errorOpeningFile) PRINTF_WARNING("errorOpeningFile error presente\n");
			}
		}
		AAsset_close(wavFile);
	}
	return success;
}

#endif
