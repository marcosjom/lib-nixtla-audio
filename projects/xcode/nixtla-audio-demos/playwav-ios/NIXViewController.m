//
//  NIXViewController.m
//  playwav-ios
//
//  Created by Marcos Ortega on 10/03/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#import "NIXViewController.h"
#include "../utils/utilLoadWav.c"

typedef struct STFileStatus_ {
	const char* fileName;
	NixUI16		source;
} STFileStatus;

STFileStatus files[] = {
	{"beat_mono_08_08000.wav", 0},
	{"beat_mono_08_11025.wav", 0},
	{"beat_mono_08_22050.wav", 0},
	{"beat_mono_08_44100.wav", 0},
	{"beat_mono_16_08000.wav", 0},
	{"beat_mono_16_11025.wav", 0},
	{"beat_mono_16_22050.wav", 0},
	{"beat_mono_16_44100.wav", 0},
	{"beat_mono_24_08000.wav", 0},
	{"beat_mono_24_11025.wav", 0},
	{"beat_mono_24_22050.wav", 0},
	{"beat_mono_24_44100.wav", 0},
	{"beat_mono_32_08000.wav", 0},
	{"beat_mono_32_11025.wav", 0},
	{"beat_mono_32_22050.wav", 0},
	{"beat_mono_32_44100.wav", 0},
	{"beat_stereo_08_08000.wav", 0},
	{"beat_stereo_08_11025.wav", 0},
	{"beat_stereo_08_22050.wav", 0},
	{"beat_stereo_08_44100.wav", 0},
	{"beat_stereo_16_08000.wav", 0},
	{"beat_stereo_16_11025.wav", 0},
	{"beat_stereo_16_22050.wav", 0},
	{"beat_stereo_16_44100.wav", 0},
	{"beat_stereo_24_08000.wav", 0},
	{"beat_stereo_24_11025.wav", 0},
	{"beat_stereo_24_22050.wav", 0},
	{"beat_stereo_24_44100.wav", 0},
	{"beat_stereo_32_08000.wav", 0},
	{"beat_stereo_32_11025.wav", 0},
	{"beat_stereo_32_22050.wav", 0},
	{"beat_stereo_32_44100.wav", 0}
};

@interface NIXViewController ()

@end

@implementation NIXViewController

- (void)viewDidLoad {
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
	_arrAudioChannels = [[NSArray alloc] initWithObjects:@"mono", @"stereo", nil];
	_arrAudioSampleRates = [[NSArray alloc] initWithObjects:@"08000", @"11025", @"22050", @"44100", nil];
	_arrAudioBitsPerSample = [[NSArray alloc] initWithObjects:@"08 bits", @"16 bits", @"24 bits", @"32 bits", nil];
	//
	[[self btnAction] addTarget:self action:@selector(touchUpInside:) forControlEvents: UIControlEventTouchUpInside];
	_nixInited	= NO;
}

- (void)enterForeground {
	if(!_nixInited){
        //
        STNixContextItf ctxItf;
        memset(&ctxItf, 0, sizeof(ctxItf));
        NixContextItf_fillMissingMembers(&ctxItf);
        STNixContextRef ctx = NixContext_alloc(&ctxItf);
        //
		if(!nixInit(ctx, &_nix, 0 /*reusable sources*/)){
			printf("ERROR nixInit failed.\n");
		} else {
			printf("nixInit success.\n");
			_nixInited = YES;
		}
        //The context is retained by the engine and other objects.
        //It wil be freed by the last object using it.
        NixContext_release(&ctx);
        NixContext_null(&ctx);
	}
	if(!_timerOneScreenTick){
		_timerOneScreenTick = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(tickOneScreenRefresh)];
		[_timerOneScreenTick setFrameInterval:1]; //1=cada refrescamiento de pantalla (este numero es la cantidad de ciclos necesarios para disparar el evento)
		[_timerOneScreenTick addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
	}
	if(!_timerOneSecond) _timerOneSecond = [NSTimer scheduledTimerWithTimeInterval: (1.0) target:self selector:@selector(tickOneSecond) userInfo:nil repeats:YES];
}

- (void)leaveForeground {
	if(_timerOneScreenTick) [_timerOneScreenTick invalidate]; _timerOneScreenTick = nil;
	if(_timerOneSecond) [_timerOneSecond invalidate]; _timerOneSecond = nil;
	if(_nixInited){
		nixFinalize(&_nix);
	}
}

-(void) tickOneScreenRefresh {
	if(_nixInited){
		nixTick(&_nix);
	}
	UITextView* txtView = [self txtLog];
	if(!_nixInited){
		[txtView setText:@"Nixtla is not inited"];
	} else {
		STNix_StatusDesc nixStatusDesc;
		nixGetStatusDesc(&_nix, &nixStatusDesc);
		NSMutableString* strTmp = [[NSMutableString alloc] init];
		[strTmp appendFormat:@"%d sources (%d assigned)\n", nixStatusDesc.countSources, nixStatusDesc.countSourcesAssigned];
		[strTmp appendFormat:@"%d play buffers", nixStatusDesc.countPlayBuffers];
		if(nixStatusDesc.sizePlayBuffers != 0){
			[strTmp appendFormat:@" (%d KB", (nixStatusDesc.sizePlayBuffers / 1024)];
			if(nixStatusDesc.sizePlayBuffersAtSW == 0){
				[strTmp appendFormat:@" at HW)\n"];
			} else if (nixStatusDesc.sizePlayBuffersAtSW == nixStatusDesc.sizePlayBuffers){
				[strTmp appendFormat:@" at SW)\n"];
			} else {
				[strTmp appendFormat:@")\n"];
				[strTmp appendFormat:@"   (%d KBs SW, %d KBs HW)\n", (nixStatusDesc.sizePlayBuffersAtSW / 1024),  ((nixStatusDesc.sizePlayBuffers - nixStatusDesc.sizePlayBuffersAtSW) / 1024)];
			}
		}
		if(nixStatusDesc.countRecBuffers != 0){
			[strTmp appendFormat:@"%d rec buffers (%d KB", nixStatusDesc.countRecBuffers, (nixStatusDesc.sizeRecBuffers / 1024)];
			if(nixStatusDesc.sizeRecBuffersAtSW == 0){
				[strTmp appendFormat:@" all HW)\n"];
			} else if(nixStatusDesc.sizeRecBuffersAtSW == nixStatusDesc.sizeRecBuffers){
				[strTmp appendFormat:@" all SW)\n"];
			} else {
				[strTmp appendFormat:@")\n"];
				[strTmp appendFormat:@"   (%d KBs SW, %d KBs HW)\n", (nixStatusDesc.sizeRecBuffersAtSW / 1024),  ((nixStatusDesc.sizeRecBuffers - nixStatusDesc.sizeRecBuffersAtSW) / 1024)];
			}
		}
		[txtView setText:strTmp];
		//[strTmp release];
	}
}

- (void) tickOneSecond {
	//
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView {
	//UIPickerView columns
	return 3;
}

-(NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component {
	//UIPickerView component rows
	if(pickerView==[self cmbAudioProps]){
		switch (component) {
			case 0: return _arrAudioChannels.count;
			case 1: return _arrAudioSampleRates.count;
			case 2: return _arrAudioBitsPerSample.count;
			default: break;
		}
	}
	return 0;
}

-(NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component {
	//UIPickerView component row item
	if(pickerView==[self cmbAudioProps]){
		switch (component) {
			case 0: return [_arrAudioChannels objectAtIndex:row];
			case 1: return [_arrAudioSampleRates objectAtIndex:row];
			case 2: return [_arrAudioBitsPerSample objectAtIndex:row];
			default: break;
		}
	}
	return @"?";
}

-(void)touchUpInside:(id)sender{
	//Action button
	BOOL repeat		= [[self chkRepeat] isOn];
	float volume	= [[self barVolume] value];
	UIPickerView* aud = [self cmbAudioProps];
	//
	
	[self actionForWav: [NSString stringWithFormat:@"beat_%@_%@_%@.wav",
						 [_arrAudioChannels objectAtIndex: [aud selectedRowInComponent:0]],
						 [[_arrAudioBitsPerSample objectAtIndex: [aud selectedRowInComponent:2]] substringWithRange: NSMakeRange(0, 2)],
						[_arrAudioSampleRates objectAtIndex: [aud selectedRowInComponent:1]]
						] repeat:repeat volume:volume];
 }

-(void)actionForWav:(NSString*)wavFile repeat:(BOOL)repeat volume:(float)volume {
	NSLog(@"Action for wav('%s'): volume(%f) repeat(%d).\n", [wavFile UTF8String], volume, repeat);
	if(!_nixInited){
		printf("ERROR, nix is not inited.\n");
	} else {
		STNixAudioDesc audioDesc;
		NixUI8* audioData = NULL;
        NixUI32 audioDataBytes = 0;
		//Look for STFileStatus index
		NixSI32 i, indexFound = -1; const NixSI32 size = (sizeof(files) / sizeof(STFileStatus));
		for(i=0; i<size; i++){
			//Compare wavfiles names
			const char* str = files[i].fileName;
			const char* str2 = [wavFile UTF8String];
			while((*str) != 0 && (*str2) != 0){
				if((*str)!=(*str2)) break;
				str++; str2++;
			}
			if((*str)==(*str2)){
				indexFound = i;
				break;
			}
		}
		//Retrieve STFileStatus
		if(indexFound==-1){
			printf("ERROR, couldnt retrieve status data for: '%s'\n", [wavFile UTF8String]);
		} else {
			NixUI16 iSourceWav, iBuffer;
			STFileStatus* status = &files[indexFound];
			//Stop previous source
			if(status->source != 0){
				nixSourceStop(&_nix, status->source);
				NixSource_release(&_nix, status->source);
				status->source = 0;
			}
			//Load and play wav
			if(!nixUtilLoadDataFromWavFile([[NSString stringWithFormat:@"%@/%@", [[NSBundle mainBundle] bundlePath], wavFile] UTF8String], &audioDesc, &audioData, &audioDataBytes)){
				printf("ERROR, WAV file load failed: '%s'.\n", [wavFile UTF8String]);
			} else {
				printf("WAV file loaded: '%s'.\n", [wavFile UTF8String]);
				iSourceWav = NixEngine_allocSource(&_nix, 1, 0, NULL, NULL);
				if(iSourceWav == 0){
					printf("ERROR, Source assign failed.\n");
				} else {
					//printf("Source(%d) assigned and retained.\n", iSourceWav);
					iBuffer = NixEngine_allocBuffer(&_nix, &audioDesc, audioData, audioDataBytes);
					if(iBuffer == 0){
						printf("ERROR, Buffer assign failed.\n");
					} else {
						//printf("Buffer(%d) loaded with data and retained.\n", iBuffer);
						if(!NixSource_setBuffer(&_nix, iSourceWav, iBuffer)){
							printf("ERROR, Buffer-to-source linking failed.\n");
						} else {
							//printf("Buffer(%d) linked with source(%d).\n", iBuffer, iSourceWav);
							nixSourceSetRepeat(&_nix, iSourceWav, repeat);
							NixSource_setVolume(&_nix, iSourceWav, volume);
							NixSource_play(&_nix, iSourceWav);
							nixSourceRetain(&_nix, iSourceWav);
							status->source = iSourceWav;
						}
						NixBuffer_release(&_nix, iBuffer);
					}
					NixSource_release(&_nix, iSourceWav);
				}
			}
			if(audioData!=NULL) free(audioData); audioData = NULL;
		}
	}
}

@end
