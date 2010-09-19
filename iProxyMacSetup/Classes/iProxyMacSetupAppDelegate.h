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

@interface iProxyMacSetupAppDelegate : NSObject <NSApplicationDelegate, NSNetServiceBrowserDelegate, NSNetServiceDelegate>
{
	NSMutableArray *proxyServiceList;
    NSMutableArray *interfaceList;
    
    BOOL browsing;
    BOOL automatic;
    NSUInteger resolvingServiceCount;
    
    NSString *defaultInterface;
    NSString *defaultProxy;
    NSString *proxyEnabledInterfaceName;
}

@property(readonly) BOOL browsing;
@property(nonatomic) BOOL automatic;
@property(readonly) BOOL proxyEnabled;
@property(readonly) NSUInteger resolvingServiceCount;
@property(readonly) NSArray *proxyServiceList;
@property(readonly) NSArray *interfaceList;
@property(retain, nonatomic) NSString *defaultInterface;
@property(retain, nonatomic) NSString *defaultProxy;

- (void)startBrowsingServices;
- (void)enableForInterface:(NSString *)interfaceName withProxy:(NSNetService *)proxy;
- (void)disableProxyForInterface:(NSString *)interface;

@end
