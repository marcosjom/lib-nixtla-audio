# lib-nixtla-audio

Nixtla ("time lapse" in Nahuatl) is a C library for audio playback and capture that abstracts the implementation of AVFAudio, OpenAL and OpenSL.

This abstraction allows you to create code that can be compiled and run on iOS, Android, BB10, Windows, Linux, MacOSX, and other operating systems.

# Licence

MIT, see LICENSE file

# Origin

Published on 15/Mar/2014.

This library was initially developed by Marcos Ortega based on experiences in the development of multimedia applications.

# Features

- Memory management using a RETAIN_COUNT model (RETAIN/RELEASE).
- OpenAL-based sources and buffers model.
- OpenSL-based callback model.
- Only two files (nixtla-audio.h and nixtla-audio.c)

Additionally:

- Memory management is customizable using the NIX_MALLOC and NIX_FREE macros.
- The point of invocation of callbacks is controlled by the user.

# Utility

Mainly in native multiplatform projects, including video games.

# How to integrate in your project?

Nixtla's design prioritizes ease of integration with other projects through simple steps such as:

- Point to or copy the nixtla-audio.h and nixtla-audio.c files to the project.
- Link the project with OpenSL (Android) or OpenAL (iOS, BB10, windows, MacOSx, Linux and others)

# Examples and Demos

This repository includes demo projects for the following environments:

- XCode
- Eclipse (android)
- Eclipse (linux)
- Visual Sutido

## Cross-platform audio recording

This code records audio in PCM format:
- 1 channel (mono)
- 16 bits signed integer per sample
- 22050Hz (samples per second)
- each buffer contains 1/10 second of audio

```
...
#include "nixtla-audio.h"
...
void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNix_audioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 audioDataSamples){
    //process your captured samples
}
...
int main(int argc, const char * argv[]){
    STNix_Engine nix;
    const NixUI16 ammPregeneratedSources = 0;
    if(!nixInit(&nix, ammPregeneratedSources)){
        //error
        return -1;
    } else {
        //Init the capture
        STNix_audioDesc audioDesc;
        audioDesc.samplesFormat = ENNix_sampleFormat_int;
        audioDesc.channels      = 1;
        audioDesc.bitsPerSample = 16;
        audioDesc.samplerate    = 22050;
        audioDesc.blockAlign    = (audioDesc.bitsPerSample / 8) * audioDesc.channels;
        const NixUI16 buffersCount = 15;
        const NixUI16 samplesPerBuffer = (audioDesc.samplerate / 10);
        if(!nixCaptureInit(&nix, &audioDesc, buffersCount, samplesPerBuffer, &bufferCapturedCallback, NULL)){
            return -1;
        } else {
            if(!nixCaptureStart(&nix)){
                return -1;
            } else {
                printf("Capturing audio...\n");
                while(1){ //Infinite loop for this demo
                    nixTick(&nix);
                    DEMO_SLEEP_MILLISEC(1000 / 30); //30 ticks per second for this demo
                }
                nixCaptureStop(&nix);
            }
            nixCaptureFinalize(&nix);
        }
        nixFinalize(&nix);
    }
    return 0;
}

```

## Cross-platform audio playing

This code loads a short-sound into memory and plays it in repeat:

```
...
#include "nixtla-audio.h"
#include "utilLoadWav.c"
...
int main(int argc, const char * argv[]){
    STNix_Engine nix;
    if(!nixInit(&nix, 8)){
        return -1;
    } else {
        NixUI16 iSourcePlay = 0;
        const char* strWavPath = "./res/beat_stereo_16_22050.wav";
        NixUI8* audioData = NULL;
        NixUI32 audioDataBytes = 0;
        STNix_audioDesc audioDesc;
        if(!loadDataFromWavFile(strWavPath, &audioDesc, &audioData, &audioDataBytes)){
            return -1;
        } else {
            //static sources own and keep one buffer, usually short-sounds
            iSourcePlay = nixSourceAssignStatic(&nix, NIX_TRUE, 0, NULL, NULL);
            if(iSourcePlay == 0){
                return -1;
            } else {
                NixUI16 iBufferWav = nixBufferWithData(&nix, &audioDesc, audioData, audioDataBytes);
                if(iBufferWav == 0){
                    return -1;
                } else {
                    if(!nixSourceSetBuffer(&nix, iSourcePlay, iBufferWav)){
                        return -1;
                    } else {
                        nixSourceSetRepeat(&nix, iSourcePlay, NIX_TRUE);
                        nixSourceSetVolume(&nix, iSourcePlay, 1.0f);
                        nixSourcePlay(&nix, iSourcePlay);
                    }
                }
            }
        }
        //audio data is loaded into buffer
        if(audioData != NULL){
            free(audioData);
            audioData = NULL;
        }
        //
        //Infinite loop, usually sync with your program main loop, or in a independent thread
        //
        while(1){
            nixTick(&nix);
            DEMO_SLEEP_MILLISEC(1000 / 30); //30 ticks per second for this demo
        }
        //release source
        if(iSourcePlay != 0){
            nixSourceRelease(&nix, iSourcePlay);
            iSourcePlay = 0;
        }
        nixFinalize(&nix);
    }
    return 0;
}
```
