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
#import "MainViewController.h"

@interface UIApplication (PrivateAPI)
- (void)_terminateWithStatus:(int)status;
@end


@implementation AppDelegate

@synthesize window;
@synthesize statusViewController;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    [window addSubview:statusViewController.view];
    [window makeKeyAndVisible];

    application.idleTimerDisabled = YES;

    // setup fake audio
    
    NSError *error = nil;

    AVAudioSession *session = [AVAudioSession sharedInstance];
    session.delegate = self;

    if (![session setCategory:AVAudioSessionCategoryPlayback error:&error]) {
        NSLog(@"ERROR: audio category %@", error);
    }
    
    if (![session setActive:YES error:&error]) {
        NSLog(@"ERROR: audio active %@", error);
    }

    NSString *sample = [[NSBundle mainBundle] pathForResource:@"silence" ofType:@"wav"];
    NSURL *url = [NSURL URLWithString:sample];
    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    player.numberOfLoops = -1;
    player.volume = 0.00;
    [player prepareToPlay];
    [player play];
    
    return YES;
}


- (void)beginInterruption
{
    NSLog(@"audio interruption started");
}

- (void)endInterruption
{
    NSLog(@"audio interruption ended");

    AVAudioSession *session = [AVAudioSession sharedInstance];

    NSError *error = nil;

    if (![session setActive:YES error:&error]) {
        NSLog(@"ERROR: audio active %@", error);
    }
    
    // resume playback
}

- (void)applicationWillResignActive:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	if (!statusViewController.proxyHttpRunning && !statusViewController.proxySocksRunning) {
    	[[UIApplication sharedApplication] _terminateWithStatus:0];
    }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

- (void)dealloc
{
    [statusViewController release];
    [window release];
    [super dealloc];
}

@end
