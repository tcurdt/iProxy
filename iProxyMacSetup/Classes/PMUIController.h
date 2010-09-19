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
    IBOutlet NSPopUpButton *interfacePopUpButton;
    IBOutlet NSProgressIndicator *progressIndicator;
    IBOutlet NSButton *startButton;
}

@property(readonly)iProxyMacSetupAppDelegate *appDelegate;

- (void)updateProgressIndicator;
- (void)updateProxyPopUpButton;
- (void)updateInterfacePopUpButton;
- (void)updateStartButton;
- (IBAction)startButtonAction:(id)sender;
- (IBAction)interfacePopUpButtonAction:(id)sender;
- (IBAction)proxyPopUpButtonAction:(id)sender;

@end
