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
#import <MessageUI/MFMailComposeViewController.h>

@interface MainViewController : UIViewController <UIActionSheetDelegate, MFMailComposeViewControllerDelegate, NSNetServiceDelegate> {

    IBOutlet UISwitch *httpSwitch;
    IBOutlet UILabel *httpAddressLabel;
    IBOutlet UILabel *httpPacLabel;
    IBOutlet UIButton *httpPacButton;

    IBOutlet UISwitch *socksSwitch;
    IBOutlet UILabel *socksAddressLabel;
    IBOutlet UILabel *socksPacLabel;
    IBOutlet UIButton *socksPacButton;

    IBOutlet UIView *connectView;
    IBOutlet UIView *runningView;

    BOOL proxyHttpRunning;
    BOOL proxySocksRunning;
    BOOL httpRunning;
	
	NSString *emailBody;
	NSString *emailURL;

    NSString *ip;
    
    NSNetService *socksProxyNetService;
    NSNetService *httpProxyNetService;
}

- (void) proxyHttpStart;
- (void) proxyHttpStop;

- (void) proxySocksStart;
- (void) proxySocksStop;

- (void) httpStart;
- (void) httpStop;

- (IBAction) switchedHttp:(id)sender;
- (IBAction) switchedSocks:(id)sender;
- (IBAction) httpURLAction:(id)sender;
- (IBAction) socksURLAction:(id)sender;
- (IBAction) showInfo;

@property (nonatomic, retain) UISwitch *httpSwitch;
@property (nonatomic, retain) UILabel *httpAddressLabel;
@property (nonatomic, retain) UILabel *httpPacLabel;
@property (nonatomic, retain) UISwitch *socksSwitch;
@property (nonatomic, retain) UILabel *socksAddressLabel;
@property (nonatomic, retain) UILabel *socksPacLabel;
@property (nonatomic, retain) UIView *connectView;
@property (nonatomic, retain) UIView *runningView;
@property (assign) BOOL proxyHttpRunning;
@property (assign) BOOL proxySocksRunning;
@property (assign) BOOL httpRunning;
@property (nonatomic, retain) NSString *ip;

@end

