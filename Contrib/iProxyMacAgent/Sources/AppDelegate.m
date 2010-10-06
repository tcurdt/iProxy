#import "AppDelegate.h"

#define NETWORKSETUP_PATH @"/usr/sbin/networksetup"
#define ROUTE_PATH @"/sbin/route"

@interface AppDelegate ()

- (NSString *)_getDeviceNameForIP:(NSString *)ip;
- (void)fetchDeviceList;

@end

@implementation AppDelegate

@synthesize browsing, resolvingServiceCount, proxyServiceList, proxyEnabled;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	proxyServiceList = [[NSMutableArray alloc] init];
    deviceList = [[NSMutableDictionary alloc] init];
    browsing = NO;
    resolvingServiceCount = 0;
    self.automatic = [[[NSUserDefaults standardUserDefaults] valueForKey:@"AUTOMATIC"] boolValue];
    [self startBrowsingServices];
    [self fetchDeviceList];
    [self addObserver:self forKeyPath:@"proxyServiceList" options:NSKeyValueObservingOptionNew context:nil];
    [self addObserver:self forKeyPath:@"resolvingServiceCount" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	if (proxyEnabledInterfaceName) {
        [self disableCurrentProxy];
    }
}

- (void)_updateAutomatic
{
	if (automatic) {
        NSDictionary *proxy = nil;
        NSUInteger ii, count = [proxyServiceList count];
        
        for (ii = 0; ii < count; ii++) {
        	if ([self isProxyReady:[proxyServiceList objectAtIndex:ii]]) {
				proxy = [proxyServiceList objectAtIndex:ii];
                break;
            }
        }
        if (!proxy && self.proxyEnabled) {
        	[self disableCurrentProxy];
        }
        if (proxy && !self.proxyEnabled) {
        	[self enableProxy:proxy];
        }
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	if (object == self) {
    	if ([keyPath isEqualToString:@"proxyServiceList"] || [keyPath isEqualToString:@"resolvingServiceCount"]) {
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
        	[self disableCurrentProxy];
        }
        [self _updateAutomatic];
    }
}

- (NSTask *)taskWithLaunchPath:(NSString *)launchPath arguments:(NSArray *)arguments
{
	NSTask *task;
    NSPipe *outputPipe = [[NSPipe alloc] init];
    
    task = [[NSTask alloc] init];
    [task setLaunchPath:launchPath];
    [task setArguments:arguments];
    [task setStandardOutput:outputPipe];
    [task setStandardInput:[NSFileHandle fileHandleWithNullDevice]];
    [task setStandardError:[NSFileHandle fileHandleWithNullDevice]];
    [outputPipe release];
    return task;
}

- (void)fetchDeviceList
{
	NSTask *task;
    
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-listnetworkserviceorder", nil]];
	[[[task standardOutput] fileHandleForReading] readToEndOfFileInBackgroundAndNotify];
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

NSString *parseDevice(NSString *line)
{
	NSRange range;
    NSString *result = nil;
    
	range = [line rangeOfString:@"Device:"];
    if (range.location != NSNotFound) {
    	NSUInteger ii, charCount;
        NSCharacterSet *deviceCharacters;
        
        deviceCharacters = [NSCharacterSet characterSetWithCharactersInString:@"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890-"];
    	line = [[line substringFromIndex:range.location + range.length] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        charCount = [line length];
        for (ii = 0; ii < charCount; ii++) {
        	if (![deviceCharacters characterIsMember:[line characterAtIndex:ii]]) {
            	break;
            }
        }
        if (ii > 0) {
        	result = [line substringToIndex:ii];
        }
    }
    return result;
}

- (void)servicesFromTask:(NSNotification *)notification
{
    NSData *data = [[notification userInfo] objectForKey:NSFileHandleNotificationDataItem];
    NSString *result = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    
    [self willChangeValueForKey:@"devicelist"];
    NSUInteger index = 0;
    NSUInteger endOfData = 0;
    NSUInteger endLine = 0;
    while (index < [result length]) {
    	BOOL enabled;
        NSString *interfaceName;
        
    	[result getLineStart:&index end:&endLine contentsEnd:&endOfData forRange:NSMakeRange(index, 1)];
        interfaceName = parseInterface([result substringWithRange:NSMakeRange(index, endOfData - index)], &enabled);
        index = endLine + 1;
        if (interfaceName) {
            NSMutableDictionary *interfaceInfo;
            NSString *deviceName = nil;
            
            [result getLineStart:&index end:&endLine contentsEnd:&endOfData forRange:NSMakeRange(index, 1)];
            deviceName = parseDevice([result substringWithRange:NSMakeRange(index, endOfData - index)]);
            index = endLine + 1;
            if (deviceName) {
                interfaceInfo = [[NSMutableDictionary alloc] init];
                [interfaceInfo setObject:interfaceName forKey:INTERFACE_NAME];
                [interfaceInfo setObject:[NSNumber numberWithBool:enabled] forKey:INTERFACE_ENABLED];
                [interfaceInfo setObject:deviceName forKey:INTERFACE_DEVICE_NAME];
                [deviceList setObject:interfaceInfo forKey:deviceName];
                [interfaceInfo release];
            }
        }
    };
    [self didChangeValueForKey:@"devicelist"];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:nil object:[notification object]];
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
    [serviceBrowser searchForServicesOfType:@"_iproxysocksproxy._tcp" inDomain:@""];
    [self _setBrowsing:YES];
}

- (NSInteger)indexForDomain:(NSString *)domain name:(NSString *)name type:(NSString *)type
{
	NSUInteger result = NSNotFound;
	NSUInteger i, count = [proxyServiceList count];
    
    for (i = 0; i < count; i++) {
      	NSNetService *service = [[proxyServiceList objectAtIndex:i] objectForKey:PROXY_SERVICE_KEY];
      	
        if ([[service domain] isEqualToString:domain] && [[service name] isEqualToString:name] && [[service type] isEqualToString:type]) {
        	result = i;
            break;
        }
    }
    return result;
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
	NSString *ip;
    NSMutableDictionary *proxy;
    
    [self willChangeValueForKey:@"proxyServiceList"];
    proxy = [[NSMutableDictionary alloc] init];
    [proxy setObject:aNetService forKey:PROXY_SERVICE_KEY];
    ip = [[NSString alloc] initWithFormat:@"%@.%@", [aNetService name], [aNetService domain]];
    [proxy setObject:ip forKey:PROXY_IP_KEY];
    [proxy setObject:[NSNumber numberWithBool:YES] forKey:PROXY_RESOLVING_KEY];
    [proxyServiceList addObject:proxy];
    [proxy release];
    [ip release];
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
    	if ([[[proxyServiceList objectAtIndex:index] objectForKey:PROXY_RESOLVING_KEY] boolValue]) {
            [self _changeResolvingServiceWithOffset:-1];
        }
        [self willChangeValueForKey:@"proxyServiceList"];
    	[proxyServiceList removeObjectAtIndex:index];
        [self didChangeValueForKey:@"proxyServiceList"];
    }
}

- (void)netServiceDidResolveAddress:(NSNetService *)sender
{
	NSUInteger index = [self indexForDomain:[sender domain] name:[sender name] type:[sender type]];
    
    if (index != NSNotFound) {
        NSString *device;
        NSMutableDictionary *proxy;
        
        [self willChangeValueForKey:@"proxyServiceList"];
        proxy = [proxyServiceList objectAtIndex:index];
        [proxy removeObjectForKey:PROXY_RESOLVING_KEY];
        device = [self _getDeviceNameForIP:[proxy objectForKey:PROXY_IP_KEY]];
        if (device) {
            [proxy setObject:device forKey:PROXY_DEVICE_KEY];
        }
        [self didChangeValueForKey:@"proxyServiceList"];
        [self _changeResolvingServiceWithOffset:-1];
    }
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict
{
	[self _changeResolvingServiceWithOffset:-1];
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

- (void)enableProxy:(NSDictionary *)proxy
{
	if (!proxyEnabled && [self isProxyReady:proxy]) {
    	NSNetService *proxyService;
        NSString *interfaceName;
        
        proxyService = [proxy objectForKey:PROXY_SERVICE_KEY];
        interfaceName = [[deviceList objectForKey:[proxy objectForKey:PROXY_DEVICE_KEY]] objectForKey:INTERFACE_NAME];
        if ([proxyService port] != -1 && interfaceName) {
            [self willChangeValueForKey:@"proxyEnabled"];
            [self _enableProxyForInterface:interfaceName server:[proxy objectForKey:PROXY_IP_KEY] port:[proxyService port]];
            proxyEnabled = YES;
            proxyEnabledInterfaceName = [interfaceName retain];
            [self didChangeValueForKey:@"proxyEnabled"];
        }
    }
}

- (void)_disableProxyForInterface:(NSString *)interface
{
	NSTask *task;
    
    task = [self taskWithLaunchPath:NETWORKSETUP_PATH arguments:[NSArray arrayWithObjects:@"-setsocksfirewallproxystate", interface, @"off", nil]];
    [task launch];
    [task waitUntilExit];
}

- (void)disableCurrentProxy
{
	if (proxyEnabled) {
        [self willChangeValueForKey:@"proxyEnabled"];
        [self _disableProxyForInterface:proxyEnabledInterfaceName];
        proxyEnabled = NO;
        [proxyEnabledInterfaceName release];
        proxyEnabledInterfaceName = nil;
        [self didChangeValueForKey:@"proxyEnabled"];
    }
}

- (NSString *)_getDeviceNameForIP:(NSString *)ip
{
	NSTask *task;
    NSString *result = nil;
    NSFileHandle *outputFileHandle;
    
    task = [self taskWithLaunchPath:ROUTE_PATH arguments:[NSArray arrayWithObjects:@"get", ip, nil]];
	outputFileHandle = [[task standardOutput] fileHandleForReading];
    [outputFileHandle waitForDataInBackgroundAndNotify];
    [task launch];
    [task waitUntilExit];
    
    NSString *allLines = [[NSString alloc] initWithData:[outputFileHandle readDataToEndOfFile] encoding:NSUTF8StringEncoding];
    NSUInteger index = 0;
    NSUInteger endOfData = 0;
    NSUInteger endLine = 0;
    while (index < [allLines length]) {
        NSString *line;
        
    	[allLines getLineStart:&index end:&endLine contentsEnd:&endOfData forRange:NSMakeRange(index, 1)];
        line = [[allLines substringWithRange:NSMakeRange(index, endOfData - index)] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if ([line hasPrefix:@"interface:"]) {
        	result = [[line substringFromIndex:[@"interface:" length]] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            break;
        }
        index = endLine + 1;
    };
    [allLines release];
    return result;
}

- (BOOL)isProxyReady:(NSDictionary *)proxy
{
	NSNetService *proxyService;
    
    proxyService = [proxy objectForKey:PROXY_SERVICE_KEY];
	return ([proxyService port] != -1 || [proxyService port] != 0) && [proxy objectForKey:PROXY_DEVICE_KEY] && [deviceList objectForKey:[proxy objectForKey:PROXY_DEVICE_KEY]];
}

@end

