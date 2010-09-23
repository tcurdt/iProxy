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
#import "AppTextFileResponse.h"
#import "HTTPServer.h"

#import <ifaddrs.h>
#import <arpa/inet.h>

#define ONE_KiB (1024)
#define ONE_MiB (ONE_KiB * ONE_KiB)
#define ONE_GiB (ONE_KiB * ONE_MiB)

// Breakout functions from relay.c to track relay data amount 

void upload_amount(ssize_t len)
{
    ((AppDelegate*) [UIApplication sharedApplication].delegate).upBytes += len;
}

void download_amount(ssize_t len)
{
    ((AppDelegate*) [UIApplication sharedApplication].delegate).downBytes += len;
}

@implementation AppDelegate

@synthesize window;
@synthesize statusViewController;
@synthesize upBytes;
@synthesize downBytes;

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

- (void)updateBytesTransferred:(NSTimer*)tmr {
    AppDelegate *delegate = [UIApplication sharedApplication].delegate;
    [delegate performSelectorOnMainThread:@selector(setUploadLabel:)
                               withObject:[NSNumber numberWithUnsignedLongLong:upBytes]
                            waitUntilDone:NO];
    [delegate performSelectorOnMainThread:@selector(setDownloadLabel:)
                               withObject:[NSNumber numberWithUnsignedLongLong:downBytes]
                            waitUntilDone:NO];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    bytesTransferredUpdater = [NSTimer scheduledTimerWithTimeInterval:1 
                                                               target:self
                                                             selector:@selector(updateBytesTransferred:)
                                                             userInfo:nil 
                                                              repeats:YES];

    [window addSubview:statusViewController.view];
    [window makeKeyAndVisible];

    application.idleTimerDisabled = YES;

#if TARGET_IPHONE_SIMULATOR
    NSString *ip;
    ip = [self getIPAddressForInterface:@"en1"];
    if (!ip) {
	    ip = [self getIPAddressForInterface:@"en2"];
    }
#else
    NSString *ip = [self getIPAddressForInterface:@"en0"];
#endif

    if (ip == nil) {
        statusViewController.ipLabel.text = @"";
        statusViewController.portLabel.text = @"";
        return;
    }
    
    // NSLog(@"ip = %@", ip);
    // NSLog(@"port = %d", port);

    statusViewController.ipLabel.text = ip;
    statusViewController.portLabel.text = [NSString stringWithFormat:@"%d", [HTTPServer sharedHTTPServer].proxyPort];

    NSString *pacUrl = [NSString stringWithFormat:@"http://%@:%d/socks.pac", ip, [HTTPServer sharedHTTPServer].httpPort];
    statusViewController.pacLabel.text = pacUrl;

    NSString *connect = [NSString stringWithFormat:@"%@:%d", ip, [HTTPServer sharedHTTPServer].proxyPort];

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

- (NSString *)formatBytes:(NSNumber *)amount {
    unsigned long long amt = [amount unsignedLongLongValue];
    if (amt >= 0 && amt < ONE_KiB) {
        return [NSString stringWithFormat:@"%llu B", amt];
    } else if (amt >= ONE_KiB && amt < ONE_MiB) {
        return [NSString stringWithFormat:@"%.1f KiB", amt / (double) ONE_KiB];
    } else if (amt >= ONE_MiB && amt < ONE_GiB) {
        return [NSString stringWithFormat:@"%.3f MiB", amt / (double) ONE_MiB];
    } else {
        return [NSString stringWithFormat:@"%.5f GiB", amt / (double) ONE_GiB];
    }
}

- (void)setUploadLabel:(NSNumber*)amount
{
    statusViewController.uploadLabel.text = [self formatBytes:amount];
}

- (void)setDownloadLabel:(NSNumber*)amount
{
    statusViewController.downloadLabel.text = [self formatBytes:amount];
}

@end
