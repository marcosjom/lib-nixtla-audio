//
//  NIXViewController.h
//  captureeco-ios
//
//  Created by Marcos Ortega on 10/03/14.
//  Copyright (c) 2014 NIBSA. All rights reserved.
//

#import <UIKit/UIKit.h>
#include "nixaudio/nixtla-audio.h"

@interface NIXViewController : UIViewController {
	BOOL			_nixInited;
	STNix_Engine	_nix;
	//
	NSArray*		_arrAudioChannels;		//UIPickerView data
	NSArray*		_arrAudioSampleRates;	//UIPickerView data
	NSArray*		_arrAudioBitsPerSample;	//UIPickerView data
	//
	NSTimer* 		_timerOneSecond;
	CADisplayLink*	_timerOneScreenTick;
}

@property (strong, nonatomic) IBOutlet UIPickerView* cmbAudioProps;
@property (strong, nonatomic) IBOutlet UIButton* btnAction;
@property (strong, nonatomic) IBOutlet UITextView* txtLog;

- (void)enterForeground;
- (void)leaveForeground;

@end
