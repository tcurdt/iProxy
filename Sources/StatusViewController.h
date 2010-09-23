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

#import <UIKit/UIKit.h>
#import <MessageUI/MFMailComposeViewController.h>

@interface StatusViewController : UIViewController <UIActionSheetDelegate, MFMailComposeViewControllerDelegate> {

    IBOutlet UILabel *ipLabel;
    IBOutlet UILabel *portLabel;
    IBOutlet UILabel *pacLabel;
    IBOutlet UILabel *uploadLabel;
    IBOutlet UILabel *downloadLabel;

}

@property (nonatomic, retain) UILabel *ipLabel;
@property (nonatomic, retain) UILabel *portLabel;
@property (nonatomic, retain) UILabel *pacLabel;
@property (nonatomic, retain) UILabel *uploadLabel;
@property (nonatomic, retain) UILabel *downloadLabel;

-(IBAction)showInstructions;
-(IBAction)pacURLAction:(id)sender;

@end

