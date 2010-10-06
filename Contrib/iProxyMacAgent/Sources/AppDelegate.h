#import <Cocoa/Cocoa.h>

#define INTERFACE_NAME @"name"
#define INTERFACE_DEVICE_NAME @"device"
#define INTERFACE_ENABLED @"enabled"

#define PROXY_SERVICE_KEY @"service"
#define PROXY_IP_KEY @"ip"
#define PROXY_DEVICE_KEY @"device"
#define PROXY_RESOLVING_KEY @"resolving"

@interface AppDelegate : NSObject <NSApplicationDelegate, NSNetServiceBrowserDelegate, NSNetServiceDelegate>
{
	NSMutableArray *proxyServiceList;
    NSMutableDictionary *deviceList;
    
    BOOL browsing;
    BOOL automatic;
    NSUInteger resolvingServiceCount;
    
    NSString *proxyEnabledInterfaceName;
    BOOL proxyEnabled;
}

@property(readonly) BOOL browsing;
@property(nonatomic) BOOL automatic;
@property(readonly) BOOL proxyEnabled;
@property(readonly) NSUInteger resolvingServiceCount;
@property(readonly) NSArray *proxyServiceList;

- (void)startBrowsingServices;
- (void)enableProxy:(NSDictionary *)proxy;
- (void)disableCurrentProxy;

- (BOOL)isProxyReady:(NSDictionary *)proxy;

@end
