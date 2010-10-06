#import <Cocoa/Cocoa.h>

@class AppDelegate;

@interface PMUIController : NSObject
{
	IBOutlet AppDelegate *appDelegate;
    IBOutlet NSPopUpButton *proxyPopUpButton;
    IBOutlet NSButton *automaticButton;
    IBOutlet NSProgressIndicator *progressIndicator;
    IBOutlet NSButton *startButton;
}

@property(readonly)AppDelegate *appDelegate;

- (void)updateProgressIndicator;
- (void)updateProxyPopUpButton;
- (void)updateStartButton;
- (IBAction)startButtonAction:(id)sender;
- (IBAction)proxyPopUpButtonAction:(id)sender;

@end
