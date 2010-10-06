#import "PMUIController.h"
#import "AppDelegate.h"

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
	    } else if ([keyPath isEqualToString:@"proxyEnabled"]) {
    		[self updateStartButton];
	        [self updateProxyPopUpButton];
	    } else if ([keyPath isEqualToString:@"automatic"]) {
    		[self updateStartButton];
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
	if ([appDelegate.proxyServiceList count] > 0) {
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
    	if ([appDelegate isProxyReady:proxy]) {
            [proxyPopUpButton addItemWithTitle:title];
        } else {
            [proxyPopUpButton addItemWithTitle:[NSString stringWithFormat:@"%@ (disabled)", title]];
        }
        [title release];
    }
    [proxyPopUpButton setEnabled:!appDelegate.proxyEnabled && !appDelegate.automatic];
    [self updateStartButton];
}

- (IBAction)startButtonAction:(id)sender
{
    NSDictionary *proxy;
	
    [self updateProxyPopUpButton];
    proxy = [appDelegate.proxyServiceList objectAtIndex:[proxyPopUpButton indexOfSelectedItem]];
	if (appDelegate.proxyEnabled) {
    	[appDelegate disableCurrentProxy];
    } else {
    	[appDelegate enableProxy:proxy];
    }
}

- (IBAction)proxyPopUpButtonAction:(id)sender
{
}

@end
