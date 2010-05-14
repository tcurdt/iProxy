/*
 * Copyright 2010, Torsten Curdt
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import "AppDelegate.h"
#import "StatusViewController.h"
#import "HTTPServer.h"

#import <ifaddrs.h>
#import <arpa/inet.h>

@implementation AppDelegate

@synthesize window;
@synthesize statusViewController;

int serv_loop();
int serv_init(char *ifs);
int queue_init();
int local_main(int ac, char **av);

- (NSString *)getIPAddressForInterface:(NSString*)ifname
{
    NSString *address = nil;

    struct ifaddrs *interfaces = NULL;

    int success = getifaddrs(&interfaces);

    if (success == 0) {
        struct ifaddrs *temp_addr = NULL;

        temp_addr = interfaces;
        while(temp_addr != NULL) {

            if(temp_addr->ifa_addr->sa_family == AF_INET) {

                if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:ifname]) {

                    address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
                }
            }

            temp_addr = temp_addr->ifa_next;
        }
    }

    freeifaddrs(interfaces);

    return address;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {    
          
    [window addSubview:statusViewController.view];
    [window makeKeyAndVisible];

    application.idleTimerDisabled = YES;

#if TARGET_IPHONE_SIMULATOR
    NSString *ip = [self getIPAddressForInterface:@"en1"];
#else
    NSString *ip = [self getIPAddressForInterface:@"en0"];
#endif

    if (ip == nil) {
        statusViewController.ipLabel.text = @"";
        statusViewController.portLabel.text = @"";
        return;
    }
    
    NSUInteger port = 8888;

    NSLog(@"ip = %@", ip);
    NSLog(@"port = %d", port);

    statusViewController.ipLabel.text = ip;
    statusViewController.portLabel.text = [NSString stringWithFormat:@"%d", port];

    NSString *connect = [NSString stringWithFormat:@"%@:%d", ip, port];

    char *args[4] = {
        "local",
        "-i",
        (char*)[connect UTF8String],
        "-f",
    };

    local_main(4, args);
    
	[[HTTPServer sharedHTTPServer] start];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	[[HTTPServer sharedHTTPServer] stop];
}

- (void)dealloc {
    [statusViewController release];
    [window release];
    [super dealloc];
}


@end
