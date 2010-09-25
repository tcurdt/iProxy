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

#import "InfoViewController.h"

@implementation InfoViewController

- (void) viewDidLoad
{
    CAGradientLayer *gradient = [CAGradientLayer layer];
    gradient.frame = self.view.bounds;
    gradient.colors = [NSArray arrayWithObjects:
        (id)[RGB(241,231,165) CGColor],
        (id)[RGB(208,180,35) CGColor],
        nil];
    [self.view.layer insertSublayer:gradient atIndex:0];

    self.title = @"Info";  
    self.navigationItem.rightBarButtonItem = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(dismiss)] autorelease];
}

- (IBAction) actionLike;
{
    NSString *anonymizedUnique = [[[UIDevice currentDevice] uniqueIdentifier] md5];

#if TARGET_IPHONE_SIMULATOR
    NSURL *url = [NSURL URLWithString: [NSString stringWithFormat: @"http://localhost?%@", anonymizedUnique]];
#else
    NSURL *url = [NSURL URLWithString: [NSString stringWithFormat: URL_LIKE, anonymizedUnique]];
#endif

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:url];
    [[NSURLConnection alloc] initWithRequest:request delegate:self startImmediately:YES];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    if ([(NSHTTPURLResponse*)response statusCode] == 200) {

        [connection release];

        UIAlertView *view = [[UIAlertView alloc] initWithTitle: @"Like It!"
            message: @"Thanks for the feedback!"
            delegate: self cancelButtonTitle: @"OK" otherButtonTitles: nil];
     
        [view show];
        [view release];

        return;
    }

    [self connection:connection didFailWithError:nil];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    [connection release];

    UIAlertView *view = [[UIAlertView alloc] initWithTitle: @"Like It!"
        message: @"That somehow did not get through. But thanks for trying!"
        delegate: self cancelButtonTitle: @"OK" otherButtonTitles: nil];
 
    [view show];
    [view release];
}


- (IBAction) actionHelp
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString: URL_HELP]];
}

- (IBAction) actionIssues
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString: URL_ISSUES]];
}

- (IBAction) actionDonate
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString: URL_DONATE]];
}

- (void) dismiss
{
    [self dismissModalViewControllerAnimated:YES];
}

@end
