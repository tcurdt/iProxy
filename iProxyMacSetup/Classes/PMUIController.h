//
//  PMUIController.h
//  iProxy
//
//  Created by Jérôme Lebel on 18/09/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class iProxyMacSetupAppDelegate;

@interface PMUIController : NSObject
{
	IBOutlet iProxyMacSetupAppDelegate *appDelegate;
    IBOutlet NSPopUpButton *proxyPopUpButton;
    IBOutlet NSButton *automaticButton;
    IBOutlet NSProgressIndicator *progressIndicator;
    IBOutlet NSButton *startButton;
}

@property(readonly)iProxyMacSetupAppDelegate *appDelegate;

- (void)updateProgressIndicator;
- (void)updateProxyPopUpButton;
- (void)updateStartButton;
- (IBAction)startButtonAction:(id)sender;
- (IBAction)proxyPopUpButtonAction:(id)sender;

@end
