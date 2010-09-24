//
//  iProxyMacSetupAppDelegate.m
//  iProxyMacSetup
//
//  Created by Jérôme Lebel on 18/09/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "iProxyMacSetupAppDelegate.h"

#define NETWORKSETUP_PATH @"/usr/sbin/networksetup"

@interface iProxyMacSetupAppDelegate ()

- (void)fetchInterfaceList;

@end

@implementation iProxyMacSetupAppDelegate

@synthesize browsing, resolvingServiceCount, proxyServiceList, interfaceList, proxyEnabled;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	proxyServiceList = [[NSMutableArray alloc] init];
    interfaceList = [[NSMutableArray alloc] init];
    browsing = NO;
    resolvingServiceCount = 0;
    self.automatic = [[[NSUserDefaults standardUserDefaults] valueForKey:@"AUTOMATIC"] boolValue];
    [self fetchInterfaceList];
    [self startBrowsingServices];
    [self addObserver:self forKeyPath:@"proxyServiceList" options:NSKeyValueObservingOptionNew context:nil];
    [self addObserver:self forKeyPath:@"interfaceList" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	if (proxyEnabledInterfaceName) {
        [self disableProxyForInterface:proxyEnabledInterfaceName];
    }
}

- (void)_updateAutomatic
{
	if (automatic) {
        NSNetService *proxy = nil;
        NSDictionary *currentInterface = nil;
        NSUInteger ii, count = [proxyServiceList count];
        
        for (ii = 0; ii < count; ii++) {
        	if ([(NSNetService *)[proxyServiceList objectAtIndex:ii] port] != -1) {
				proxy = [proxyServiceList objectAtIndex:ii];
                break;
            }
        }
        for (NSDictionary *interface in interfaceList) {
        	if ([self.defaultInterface isEqualToString:[interface objectForKey:INTERFACE_NAME]]) {
            	currentInterface = interface;
            	break;
            }
        }
        if ((!proxy || !currentInterface) && self.proxyEnabled) {
        	[self disableProxyForInterface:proxyEnabledInterfaceName];
        }
        if (proxy && currentInterface && !self.proxyEnabled) {
        	[self enableForInterface:[currentInterface objectForKey:INTERFACE_NAME] withProxy:proxy];
        }
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	if (object == self) {
    	if ([keyPath isEqualToString:@"proxyServiceList"] || [keyPath isEqualToString:@"interfaceList"]) {
        	[self _updateAutomatic];
        }
    }
}

- (BOOL)automatic
{
	return automatic;
}

- (void)setAutomatic:(BOOL)value
{
	if (automatic != value) {
    	BOOL shouldStop = automatic;
        
    	[self willChangeValueForKey:@"automatic"];
        automatic = value;
        [[NSUserDefaults standardUserDefaults] setValue:[NSNumber numberWithBool:automatic] forKey:@"AUTOMATIC"];
        [[NSUserDefaults standardUserDefaults] synchronize];
    	[self didChangeValueForKey:@"automatic"];
        
        if (shouldStop && self.proxyEnabled) {
        	[self disableProxyForInterface:proxyEnabledInterfaceName];
        }
        [self _updateAutomatic];
    }
}

- (NSTask *)taskWithLaunchPath:(NSString *)launchPath arguments:(NSArray *)arguments
{
	NSTask *task;
    NSPipe *outputPipe = [[NSPipe alloc] init];
    
    [[outputPipe fileHandleForReading] readToEndOfFileInBackgroundAndNotify];
    task = [[NSTask alloc] init];
    [task setLaunchPath:launchPath];
    [task setArguments:arguments];
    [task setStandardOutput:outputPipe];
    [outputPipe release];
    return task;
}

- (void)fetchInterfaceList
{
	NSTask *task;
    
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-listnetworkserviceorder", nil]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(servicesFromTask:) name:NSFileHandleReadToEndOfFileCompletionNotification object:[[task standardOutput] fileHandleForReading]];
    [task launch];
}

NSString *parseInterface(NSString *line, BOOL *enabled)
{
	assert(enabled != NULL);
    NSUInteger ii, charCount = [line length];
    
    if ([line characterAtIndex:0] != '(') {
    	return NULL;
    }
    for (ii = 1; ii < charCount; ii++) {
    	if ([line characterAtIndex:ii] == ')') {
        	break;
        } else if (([line characterAtIndex:ii] < '0' || [line characterAtIndex:ii] > '9') && [line characterAtIndex:ii] != '*') {
        	return NULL;
        }
    }
    if (ii == charCount) {
    	return NULL;
    }
    *enabled = ![[line substringWithRange:NSMakeRange(1, ii - 1)] isEqualToString:@"*"];
    return [[line substringFromIndex:ii + 1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (void)servicesFromTask:(NSNotification *)notification
{
    NSData *data = [[notification userInfo] objectForKey:NSFileHandleNotificationDataItem];
    NSString *result = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    
    [self willChangeValueForKey:@"interfaceList"];
    NSUInteger index = 0;
    NSUInteger endOfData = 0;
    NSUInteger endLine = 0;
    while (index < [result length]) {
    	BOOL enabled;
        NSString *interfaceName;
        
    	[result getLineStart:&index end:&endLine contentsEnd:&endOfData forRange:NSMakeRange(index, 1)];
        interfaceName = parseInterface([result substringWithRange:NSMakeRange(index, endOfData - index)], &enabled);
        if (interfaceName) {
            NSMutableDictionary *interfaceInfo;
            
            interfaceInfo = [[NSMutableDictionary alloc] init];
            [interfaceInfo setObject:interfaceName forKey:INTERFACE_NAME];
            [interfaceInfo setObject:[NSNumber numberWithBool:enabled] forKey:INTERFACE_ENABLED];
            [interfaceList addObject:interfaceInfo];
            [interfaceInfo release];
        }
        index = endLine + 1;
    };
    [self didChangeValueForKey:@"interfaceList"];
}

- (void)_setBrowsing:(BOOL)value
{
	if (browsing != value) {
        [self willChangeValueForKey:@"browsing"];
        browsing = value;
        [self didChangeValueForKey:@"browsing"];
    }
}

- (void)_changeResolvingServiceWithOffset:(int)offset
{
    [self willChangeValueForKey:@"resolvingServiceCount"];
    resolvingServiceCount += offset;
    [self didChangeValueForKey:@"resolvingServiceCount"];
}

- (void)startBrowsingServices
{
    NSNetServiceBrowser *serviceBrowser;
     
    serviceBrowser = [[NSNetServiceBrowser alloc] init];
    [serviceBrowser setDelegate:self];
    [serviceBrowser searchForServicesOfType:@"_socks5._tcp" inDomain:@""];
    [self _setBrowsing:YES];
}

- (NSInteger)indexForDomain:(NSString *)domain name:(NSString *)name type:(NSString *)type
{
	NSUInteger result = NSNotFound;
	NSUInteger i, count = [proxyServiceList count];
    
    for (i = 0; i < count; i++) {
      	NSNetService *service = [proxyServiceList objectAtIndex:i];
      	
        if ([[service domain] isEqualToString:domain] && [[service name] isEqualToString:name] && [[service type] isEqualToString:type]) {
        	result = i;
            break;
        }
    }
    return result;
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
	[self willChangeValueForKey:@"proxyServiceList"];
    [proxyServiceList addObject:aNetService];
	[self didChangeValueForKey:@"proxyServiceList"];
	[aNetService setDelegate:self];
    [aNetService resolveWithTimeout:20.0];
    [self _changeResolvingServiceWithOffset:1];
    [self _setBrowsing:moreComing];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
	NSUInteger index = [self indexForDomain:[aNetService domain] name:[aNetService name] type:[aNetService type]];
    
    if (index != NSNotFound) {
        [self willChangeValueForKey:@"proxyServiceList"];
    	[proxyServiceList removeObjectAtIndex:index];
        [self didChangeValueForKey:@"proxyServiceList"];
    }
}

- (void)netServiceWillResolve:(NSNetService *)sender
{
}

- (void)netServiceDidResolveAddress:(NSNetService *)sender
{
	[self _changeResolvingServiceWithOffset:-1];
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict
{
	[self _changeResolvingServiceWithOffset:-1];
}

- (NSString *)defaultInterface
{
	if (!defaultInterface) {
    	defaultInterface = [[[NSUserDefaults standardUserDefaults] objectForKey:@"DEFAULT_INTERFACE"] retain];
        if (!defaultInterface) {
        	defaultInterface = [@"AirPort" retain];
        }
    }
    return defaultInterface;
}

- (void)setDefaultInterface:(NSString *)interface
{
	[defaultInterface release];
    defaultInterface = [interface retain];
    [[NSUserDefaults standardUserDefaults] setObject:defaultInterface forKey:@"DEFAULT_INTERFACE"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)_enableProxyForInterface:(NSString *)interface server:(NSString *)server port:(NSUInteger)port
{
	NSTask *task;
    
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-setsocksfirewallproxy", interface, server, [NSString stringWithFormat:@"%d", port], @"off", nil]];
    [task launch];
    [task waitUntilExit];
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-setsocksfirewallproxystate", interface, @"on", nil]];
    [task launch];
    [task waitUntilExit];
}

- (void)enableForInterface:(NSString *)interfaceName withProxy:(NSNetService *)proxy
{
	if (!proxyEnabled) {
        [self willChangeValueForKey:@"proxyEnabled"];
        if ([proxy port] != -1) {
            [self _enableProxyForInterface:interfaceName server:[NSString stringWithFormat:@"%@.%@", [proxy name], [proxy domain]] port:[proxy port]];
            proxyEnabled = YES;
            proxyEnabledInterfaceName = [interfaceName retain];
        }
        [self didChangeValueForKey:@"proxyEnabled"];
    }
}

- (void)_disableProxyForInterface:(NSString *)interface
{
	NSTask *task;
    
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-setsocksfirewallproxystate", interface, @"off", nil]];
    [task launch];
    [task waitUntilExit];
}

- (void)disableProxyForInterface:(NSString *)interface
{
	if (proxyEnabled) {
        [self willChangeValueForKey:@"proxyEnabled"];
        [self _disableProxyForInterface:interface];
        proxyEnabled = NO;
        [proxyEnabledInterfaceName release];
        proxyEnabledInterfaceName = nil;
        [self didChangeValueForKey:@"proxyEnabled"];
    }
}

@end

