//
//  PMUIController.m
//  iProxy
//
//  Created by Jérôme Lebel on 18/09/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "PMUIController.h"
#import "iProxyMacSetupAppDelegate.h"

@implementation PMUIController

@synthesize appDelegate;

- (void)awakeFromNib
{
    [self updateProxyPopUpButton];
    [self updateStartButton];
    [self updateProgressIndicator];
    [appDelegate addObserver:self forKeyPath:@"browsing" options:NSKeyValueObservingOptionNew context:nil];
    [appDelegate addObserver:self forKeyPath:@"resolvingServiceCount" options:NSKeyValueObservingOptionNew context:nil];
    [appDelegate addObserver:self forKeyPath:@"proxyServiceList" options:NSKeyValueObservingOptionNew context:nil];
    [appDelegate addObserver:self forKeyPath:@"interfaceList" options:NSKeyValueObservingOptionNew context:nil];
    [appDelegate addObserver:self forKeyPath:@"proxyEnabled" options:NSKeyValueObservingOptionNew context:nil];
    [appDelegate addObserver:self forKeyPath:@"automatic" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	if (object == appDelegate) {
		if ([keyPath isEqualToString:@"browsing"]) {
    		[self updateProgressIndicator];
	    } else if ([keyPath isEqualToString:@"resolvingServiceCount"]) {
    		[self updateProgressIndicator];
	    } else if ([keyPath isEqualToString:@"proxyServiceList"]) {
    		[self updateStartButton];
        	[self updateProxyPopUpButton];
	    } else if ([keyPath isEqualToString:@"interfaceList"]) {
    		[self updateStartButton];
        	[self updateInterfacePopUpButton];
	    } else if ([keyPath isEqualToString:@"proxyEnabled"]) {
    		[self updateStartButton];
        	[self updateInterfacePopUpButton];
	        [self updateProxyPopUpButton];
	    } else if ([keyPath isEqualToString:@"automatic"]) {
    		[self updateStartButton];
    	    [self updateInterfacePopUpButton];
        	[self updateProxyPopUpButton];
        }
    }
}

- (void)updateProgressIndicator
{
	if (appDelegate.browsing || appDelegate.resolvingServiceCount > 0) {
    	[progressIndicator startAnimation:nil];
    } else {
        [progressIndicator stopAnimation:nil];
    }
}

- (void)updateStartButton
{
	if ([appDelegate.proxyServiceList count] > 0 && [appDelegate.interfaceList count] > 0) {
    	[startButton setEnabled:!appDelegate.automatic];
    } else {
    	[startButton setEnabled:NO];
    }
    if (appDelegate.proxyEnabled) {
        [startButton setTitle:@"Stop"];
    } else {
        [startButton setTitle:@"Start"];
    }
}

- (void)updateProxyPopUpButton
{
	[proxyPopUpButton removeAllItems];
    for (NSDictionary *proxy in appDelegate.proxyServiceList) {
    	NSString *title;
        NSNetService *proxyService;
        
        proxyService = [proxy objectForKey:PROXY_SERVICE_KEY];
        title = [[NSString alloc] initWithFormat:@"%@.%@", [proxyService name], [proxyService domain]];
    	if ([proxyService port] != -1 || [proxyService port] != 0) {
            [proxyPopUpButton addItemWithTitle:title];
        } else {
            [proxyPopUpButton addItemWithTitle:[NSString stringWithFormat:@"%@ (disabled)", title]];
        }
        [title release];
    }
    [proxyPopUpButton setEnabled:!appDelegate.proxyEnabled && !appDelegate.automatic];
    [self updateStartButton];
}

- (void)updateInterfacePopUpButton
{
	NSString *defaultInterface = appDelegate.defaultInterface;
    
	[interfacePopUpButton removeAllItems];
    for (NSDictionary *service in appDelegate.interfaceList) {
    	if ([[service objectForKey:INTERFACE_ENABLED] boolValue]) {
            [interfacePopUpButton addItemWithTitle:[service objectForKey:INTERFACE_NAME]];
        } else {
            [interfacePopUpButton addItemWithTitle:[NSString stringWithFormat:@"%@ (disabled)", [service objectForKey:INTERFACE_NAME]]];
        }
        if ([defaultInterface isEqualToString:[service objectForKey:INTERFACE_NAME]]) {
        	[interfacePopUpButton selectItem:[interfacePopUpButton lastItem]];
        }
    }
    [interfacePopUpButton setEnabled:!appDelegate.proxyEnabled && !appDelegate.automatic];
    [self updateStartButton];
}

- (IBAction)startButtonAction:(id)sender
{
    NSDictionary *interfaceInfo;
    NSDictionary *proxy;
	
    [self updateProxyPopUpButton];
    [self updateInterfacePopUpButton];
    interfaceInfo = [appDelegate.interfaceList objectAtIndex:[interfacePopUpButton indexOfSelectedItem]];
    proxy = [appDelegate.proxyServiceList objectAtIndex:[proxyPopUpButton indexOfSelectedItem]];
	if (appDelegate.proxyEnabled) {
    	[appDelegate disableProxyForInterface:[interfaceInfo objectForKey:INTERFACE_NAME]];
    } else {
    	[appDelegate enableForInterface:[interfaceInfo objectForKey:INTERFACE_NAME] withProxy:proxy];
    }
}

- (IBAction)interfacePopUpButtonAction:(id)sender
{
	appDelegate.defaultInterface = [[appDelegate.interfaceList objectAtIndex:[interfacePopUpButton indexOfSelectedItem]] objectForKey:INTERFACE_NAME];
}

- (IBAction)proxyPopUpButtonAction:(id)sender
{
}

@end
