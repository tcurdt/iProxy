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

#import "MainViewController.h"
#import "InfoViewController.h"
#import "HTTPServer.h"
#import "PacFileResponse.h"

int polipo_main(int argc, char **argv);
void polipo_exit();

int srelay_main(int ac, char **av);
void srelay_exit();

@interface MainViewController ()
- (void) ping;
@end

@implementation MainViewController

@synthesize httpSwitch;
@synthesize httpAddressLabel;
@synthesize httpPacLabel;
@synthesize socksSwitch;
@synthesize socksAddressLabel;
@synthesize socksPacLabel;
@synthesize connectView;
@synthesize runningView;
@synthesize proxyHttpRunning;
@synthesize proxySocksRunning;
@synthesize httpRunning;
@synthesize ip;

- (void) viewDidLoad
{
    // connectView.layer.cornerRadius = 15;
    // runningView.layer.cornerRadius = 15;

    proxyHttpRunning = NO;
    proxySocksRunning = NO;

    httpSwitch.on = [[NSUserDefaults standardUserDefaults] boolForKey: KEY_HTTP_ON];
    socksSwitch.on = [[NSUserDefaults standardUserDefaults] boolForKey: KEY_SOCKS_ON];

    connectView.backgroundColor = [UIColor clearColor];
    runningView.backgroundColor = [UIColor clearColor];

    CAGradientLayer *gradient = [CAGradientLayer layer];
    gradient.frame = self.view.bounds;
    gradient.colors = [NSArray arrayWithObjects:
        (id)[RGB(241,231,165) CGColor],
        (id)[RGB(208,180,35) CGColor],
        nil];
    [self.view.layer insertSublayer:gradient atIndex:0];

    [NSTimer scheduledTimerWithTimeInterval:1
        target:self
        selector:@selector(ping)
        userInfo:nil 
        repeats:YES];
    [self ping];
}

- (void) ping
{
#if TARGET_IPHONE_SIMULATOR
    self.ip = [[UIDevice currentDevice] IPAddressForInterface:@"en1"];
#else
    self.ip = [[UIDevice currentDevice] IPAddressForInterface:@"en0"];
#endif

    if (self.ip != nil) {
        
        [PacFileResponse setIP:self.ip];
        
        httpAddressLabel.text = [NSString stringWithFormat:@"%@:%d", self.ip, PROXY_PORT_HTTP];
        httpPacLabel.text = [NSString stringWithFormat:@"http://%@:%d/http.pac", self.ip, HTTP_SERVER_PORT];

        socksAddressLabel.text = [NSString stringWithFormat:@"%@:%d", self.ip, PROXY_PORT_SOCKS];
        socksPacLabel.text = [NSString stringWithFormat:@"http://%@:%d/socks.pac", self.ip, HTTP_SERVER_PORT];

        if (httpSwitch.on) {
            [self proxyHttpStart];

            httpAddressLabel.alpha = 1.0;
            httpPacLabel.alpha = 1.0;

        } else {
            [self proxyHttpStop];

            httpAddressLabel.alpha = 0.1;
            httpPacLabel.alpha = 0.1;
        }

        if (socksSwitch.on) {
            [self proxySocksStart];

            socksAddressLabel.alpha = 1.0;
            socksPacLabel.alpha = 1.0;

        } else {
            [self proxySocksStop];

            socksAddressLabel.alpha = 0.1;
            socksPacLabel.alpha = 0.1;
        }
        
        if (httpSwitch.on || socksSwitch.on) {
            [self httpStart];
        } else {
            [self httpStop];
        }

        [self.view addTaggedSubview:runningView];

    } else {

        [self httpStop];
        [self proxyHttpStop];
        [self proxySocksStop];
        
        [self.view addTaggedSubview:connectView];

    }
}

- (IBAction) switchedHttp:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setBool: httpSwitch.on forKey: KEY_HTTP_ON];
}

- (IBAction) switchedSocks:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setBool: socksSwitch.on forKey: KEY_SOCKS_ON];
}

- (IBAction) showInfo
{
    InfoViewController *viewController = [[InfoViewController alloc] init];
    UINavigationController *navigationConroller = [[UINavigationController alloc] initWithRootViewController:viewController];
    [self presentModalViewController:navigationConroller animated:YES];
    [navigationConroller release];
    [viewController release];
}

#pragma mark http server

- (void) httpStart
{
    if (self.httpRunning) {
        return;
    }

    NSLog(@"http server start");

    [[HTTPServer sharedHTTPServer] start];

    self.httpRunning = YES;
}

- (void) httpStop
{
    if (!self.httpRunning) {
        return;
    }

    NSLog(@"http server stop");

    [[HTTPServer sharedHTTPServer] stop];

    self.httpRunning = NO;
}


#pragma mark http proxy

- (void) proxyHttpStart
{
    if (self.proxyHttpRunning) {
        return;
    }

    [NSThread detachNewThreadSelector:@selector(proxyHttpRun) toTarget:self withObject:nil];
}

- (void) proxyHttpStop
{
    if (!self.proxyHttpRunning) {
        return;
    }

    polipo_exit();
}

- (void) proxyHttpRun
{
    self.proxyHttpRunning = YES;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"http proxy start");

    NSString *configuration = [[NSBundle mainBundle] pathForResource:@"polipo" ofType:@"config"];

    char *args[5] = {
        "test",
        "-c",
        (char*)[configuration UTF8String],
        (char*)[[NSString stringWithFormat:@"proxyAddress=%@", ip] UTF8String],
        (char*)[[NSString stringWithFormat:@"proxyPort=%d", PROXY_PORT_HTTP] UTF8String],
    };

    polipo_main(5, args);

    NSLog(@"http proxy stop");

    [pool drain];

    self.proxyHttpRunning = NO;
}

#pragma mark socks proxy

- (void) proxySocksStart
{
    if (self.proxySocksRunning) {
        return;
    }

    [NSThread detachNewThreadSelector:@selector(proxySocksRun) toTarget:self withObject:nil];
}

- (void) proxySocksStop
{
    if (!self.proxySocksRunning) {
        return;
    }

    srelay_exit();
}

- (void) proxySocksRun
{
    self.proxySocksRunning = YES;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"socks proxy start");

    NSString *connect = [NSString stringWithFormat:@"%@:%d", self.ip, PROXY_PORT_SOCKS];

    char *args[4] = {
        "srelay",
        "-i",
        (char*)[connect UTF8String],
        "-f",
    };

    srelay_main(4, args);

    NSLog(@"socks proxy stop");

    [pool drain];

    self.proxySocksRunning = NO;
}

@end
