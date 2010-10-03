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
#import <MobileCoreServices/MobileCoreServices.h>

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
	self.ip = [[NSProcessInfo processInfo] hostName];
    if (self.ip != nil) {
        
        httpAddressLabel.text = [NSString stringWithFormat:@"%@:%d", self.ip, HTTP_PROXY_PORT];
        httpPacLabel.text = [NSString stringWithFormat:@"http://%@:%d/http.pac", self.ip, [HTTPServer sharedHTTPServer].httpServerPort];

        socksAddressLabel.text = [NSString stringWithFormat:@"%@:%d", self.ip, SOCKS_PROXY_PORT];
        socksPacLabel.text = [NSString stringWithFormat:@"http://%@:%d/socks.pac", self.ip, [HTTPServer sharedHTTPServer].httpServerPort];

        if (httpSwitch.on) {
            [self proxyHttpStart];

            httpAddressLabel.alpha = 1.0;
            httpPacLabel.alpha = 1.0;
            httpPacButton.enabled = YES;

        } else {
            [self proxyHttpStop];

            httpAddressLabel.alpha = 0.1;
            httpPacLabel.alpha = 0.1;
            httpPacButton.enabled = NO;
        }

        if (socksSwitch.on) {
            [self proxySocksStart];

            socksAddressLabel.alpha = 1.0;
            socksPacLabel.alpha = 1.0;
            socksPacButton.enabled = YES;

        } else {
            [self proxySocksStop];

            socksAddressLabel.alpha = 0.1;
            socksPacLabel.alpha = 0.1;
            socksPacButton.enabled = NO;
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

    httpProxyNetService = [[NSNetService alloc] initWithDomain:@"" type:@"_iproxyhttpproxy._tcp." name:@"" port:HTTP_PROXY_PORT];
    httpProxyNetService.delegate = self;
    [httpProxyNetService publish];
    [NSThread detachNewThreadSelector:@selector(proxyHttpRun) toTarget:self withObject:nil];
}

- (void) proxyHttpStop
{
    if (!self.proxyHttpRunning) {
        return;
    }

    [httpProxyNetService stop];
    [httpProxyNetService release];
    httpProxyNetService = nil;
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
        "proxyAddress=0.0.0.0",
        (char*)[[NSString stringWithFormat:@"proxyPort=%d", HTTP_PROXY_PORT] UTF8String],
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

    socksProxyNetService = [[NSNetService alloc] initWithDomain:@"" type:@"_iproxysocksproxy._tcp." name:@"" port:SOCKS_PROXY_PORT];
    socksProxyNetService.delegate = self;
    [socksProxyNetService publish];
    [NSThread detachNewThreadSelector:@selector(proxySocksRun) toTarget:self withObject:nil];
}

- (void) proxySocksStop
{
    if (!self.proxySocksRunning) {
        return;
    }

    [socksProxyNetService stop];
    [socksProxyNetService release];
    socksProxyNetService = nil;
    srelay_exit();
}

- (void) proxySocksRun
{
    self.proxySocksRunning = YES;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"socks proxy start");

    NSString *connect = [NSString stringWithFormat:@":%d", SOCKS_PROXY_PORT];

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

- (void) httpURLAction:(id)sender
{
    UIActionSheet *test;
    
    test = [[UIActionSheet alloc] initWithTitle:@"HTTP Pac URL Action" delegate:self cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil otherButtonTitles:@"Send by Email", @"Copy URL", nil];
	[emailBody release];
	emailBody = [[NSString alloc] initWithFormat:@"http pac URL : %@\n", httpPacLabel.text];
	[emailURL release];
	emailURL = [httpPacLabel.text retain];
    [test showInView:self.view];
    [test release];
}

- (void) socksURLAction:(id)sender
{
    UIActionSheet *test;
    
    test = [[UIActionSheet alloc] initWithTitle:@"SOCKS Pac URL Action" delegate:self cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil otherButtonTitles:@"Send by Email", @"Copy URL", nil];
	[emailBody release];
	emailBody = [[NSString alloc] initWithFormat:@"socks pac URL : %@\n", socksPacLabel.text];
	[emailURL release];
	emailURL = [socksPacLabel.text retain];
    [test showInView:self.view];
    [test release];
}

- (void)actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
	switch (buttonIndex) {
        case 0:
        	{
                MFMailComposeViewController*	messageController = [[MFMailComposeViewController alloc] init];
                
                if ([messageController respondsToSelector:@selector(setModalPresentationStyle:)])	// XXX not available in 3.1.3
                    messageController.modalPresentationStyle = UIModalPresentationFormSheet;
                    
                messageController.mailComposeDelegate = self;
                [messageController setMessageBody:emailBody isHTML:NO];
                [self presentModalViewController:messageController animated:YES];
                [messageController release];
            }
            break;
        case 1:
        	{
				NSDictionary *items;
                
				items = [NSDictionary dictionaryWithObjectsAndKeys:emailURL, kUTTypePlainText, emailURL, kUTTypeText, emailURL, kUTTypeUTF8PlainText, [NSURL URLWithString:emailURL], kUTTypeURL, nil];
                [UIPasteboard generalPasteboard].items = [NSArray arrayWithObjects:items, nil];
            }
        	break;
        default:
            break;
    }
}


- (void)mailComposeController:(MFMailComposeViewController*)controller didFinishWithResult:(MFMailComposeResult)result error:(NSError*)error 
{
	[self dismissModalViewControllerAnimated:YES];
}

@end
