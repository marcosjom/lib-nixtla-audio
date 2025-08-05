//
//  NIXViewController.m
//  captureeco-ios
//
//  Created by Marcos Ortega on 10/03/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#import "NIXViewController.h"
#include <AudioToolbox/AudioToolbox.h> //AudioSessionInitialize

#define NIX_DEMO_BUFFERS_PER_SECOND 3
NixUI16 sourceStream = 0;

void bufferCapturedCallback(STNix_Engine* eng, void* userdata, const STNixAudioDesc audioDesc, const NixUI8* audioData, const NixUI32 audioDataBytes, const NixUI32 blocksCount){
	const NixUI16 iSource = *((NixUI16*)userdata);
	const NixUI16 iBuffer = NixEngine_allocBuffer(eng, &audioDesc, audioData, audioDataBytes);
	if(iBuffer == 0){
		printf("bufferCapturedCallback, NixEngine_allocBuffer failed for iSource(%d)\n", iSource);
	} else {
		if(nixSourceStreamAppendBuffer(eng, iSource, iBuffer)){
			//PRINTF_INFO("bufferCapturedCallback, source(%d) new buffer(%d) attached\n", iSource, iBuffer);
		} else {
			printf("bufferCapturedCallback, ERROR attaching new buffer buffer(%d) to source(%d)\n", iBuffer, iSource);
		}
		NixBuffer_release(eng, iBuffer);
	}
}

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
	//
	if(AudioSessionInitialize(NULL, NULL, NULL/*sessionCallback*/, NULL) != 0){ //run loop, run loop mode, metodo escuchador de interrupciones, datos que son pasados al metodo escuchador
		printf("ERROR: AudioSessionInitialize failed.\n");
	} else {
		UInt32 audioCat = kAudioSessionCategory_PlayAndRecord;
		if(AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCat), &audioCat) != 0){
			printf("ERROR, AudioSessionSetProperty(kAudioSessionCategory_PlayAndRecord) failed\n");
		} else {
			printf("iOS audion session inited,\n");
		}
	}
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
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
			if(AudioSessionSetActive(true) != 0){
				printf("ERROR, AudioSessionSetActive(true) failed\n");
			}
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
		_nixInited = NIX_FALSE;
		if(AudioSessionSetActive(false) != 0){
			printf("ERROR, AudioSessionSetActive(false) failed\n");
		}
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
		} else {
			[strTmp appendFormat:@"\n"];
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
	if(nixCaptureIsOnProgress(&_nix)){
		printf("Capture stopping\n");
		nixCaptureStop(&_nix);
		nixCaptureFinalize(&_nix);
		if(sourceStream != 0){
			nixSourceStop(&_nix, sourceStream);
			NixSource_release(&_nix, sourceStream);
			sourceStream = 0;
		}
		printf("Capture stopped\n");
	} else {
		sourceStream = nixSourceAssignStream(&_nix, NIX_TRUE, 0, NULL, NULL, NIX_DEMO_BUFFERS_PER_SECOND, NULL, NULL);
		if(sourceStream == 0){
			printf("ERROR, nixSourceAssignStream failed\n");
		} else {
			STNixAudioDesc audDesc;
			UIPickerView* aud = [self cmbAudioProps];
			NixSource_play(&_nix, sourceStream);
			audDesc.samplesFormat	= ENNixSampleFmt_Int;
			audDesc.channels		= [aud selectedRowInComponent:0] + 1;
			audDesc.bitsPerSample	= [[[_arrAudioBitsPerSample objectAtIndex: [aud selectedRowInComponent:2]] substringWithRange: NSMakeRange(0, 2)] intValue];
			audDesc.samplerate		= [[_arrAudioSampleRates objectAtIndex: [aud selectedRowInComponent:1]] intValue];
			audDesc.blockAlign		= (audDesc.bitsPerSample / 8) * audDesc.channels;
			if(!nixCaptureInit(&_nix, &audDesc, NIX_DEMO_BUFFERS_PER_SECOND, audDesc.samplerate / NIX_DEMO_BUFFERS_PER_SECOND, bufferCapturedCallback, &sourceStream)){
				printf("ERROR, nixCaptureInit failed\n");
			} else {
				if(!nixCaptureStart(&_nix)){
					printf("ERROR, nixCaptureStart failed\n");
					nixCaptureFinalize(&_nix);
				} else {
					printf("Capture started\n");
				}
			}
		}
	}
}


@end
