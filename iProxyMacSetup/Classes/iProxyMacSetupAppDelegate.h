//
//  iProxyMacSetupAppDelegate.h
//  iProxyMacSetup
//
//  Created by Jérôme Lebel on 18/09/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#define INTERFACE_NAME @"name"
#define INTERFACE_ENABLED @"enabled"

#define PROXY_SERVICE_KEY @"service"
#define PROXY_IP_KEY @"ip"
#define PROXY_INTERFACE_KEY @"interface"
#define PROXY_RESOLVING_KEY @"resolving"

@interface iProxyMacSetupAppDelegate : NSObject <NSApplicationDelegate, NSNetServiceBrowserDelegate, NSNetServiceDelegate>
{
	NSMutableArray *proxyServiceList;
    NSMutableArray *interfaceList;
    
    BOOL browsing;
    BOOL automatic;
    NSUInteger resolvingServiceCount;
    
    NSString *defaultInterface;
    NSString *proxyEnabledInterfaceName;
    BOOL proxyEnabled;
}

@property(readonly) BOOL browsing;
@property(nonatomic) BOOL automatic;
@property(readonly) BOOL proxyEnabled;
@property(readonly) NSUInteger resolvingServiceCount;
@property(readonly) NSArray *proxyServiceList;
@property(readonly) NSArray *interfaceList;
@property(retain, nonatomic) NSString *defaultInterface;

- (void)startBrowsingServices;
- (void)enableForInterface:(NSString *)interfaceName withProxy:(NSDictionary *)proxy;
- (void)disableProxyForInterface:(NSString *)interface;

+ (BOOL)isProxyEnabled:(NSDictionary *)proxy;

@end
