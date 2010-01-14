//
//  iProxyAppDelegate.m
//  iProxy
//
//  Created by Torsten Curdt on 23.12.09.
//  Copyright vafer.org 2009. All rights reserved.
//

#import "iProxyAppDelegate.h"
#import "iProxyViewController.h"


@implementation iProxyAppDelegate

@synthesize window;
@synthesize viewController;

int serv_loop();
int serv_init(char *ifs);
int queue_init();
int local_main(int ac, char **av);

- (void)applicationDidFinishLaunching:(UIApplication *)application {    
          
    // Override point for customization after app launch    
    [window addSubview:viewController.view];
    [window makeKeyAndVisible];

    char *args[4] = {
        "local",
        "-i",
        "127.0.0.1:8888",
        "-f"
    };

    local_main(4, args);
    
//    if (serv_init("127.0.0.1:8888") < 0) {
//        NSLog(@"failed to init server");
//        return;
//	}
//
//    if (queue_init() != 0) {
//        NSLog(@"failed to init queue");
//        return;
//    }
//    
//    NSLog(@"starting server loop");
//    serv_loop();
    
}


- (void)dealloc {
    [viewController release];
    [window release];
    [super dealloc];
}


@end
