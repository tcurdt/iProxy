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

#import "StatusViewController.h"
#import "InstructionsViewController.h"
#import <MobileCoreServices/MobileCoreServices.h>

@implementation StatusViewController

@synthesize ipLabel;
@synthesize portLabel;
@synthesize pacLabel;
@synthesize uploadLabel;
@synthesize downloadLabel;
@synthesize currentIp;
@synthesize proxyPort;
@synthesize pacURL;

- (IBAction)showInstructions
{
    InstructionsViewController *viewController = [[InstructionsViewController alloc] init];
    UINavigationController *navigationConroller = [[UINavigationController alloc] initWithRootViewController:viewController];

    [self presentModalViewController:navigationConroller animated:YES];

    [navigationConroller release];
    [viewController release];
}

- (NSInteger)tableView:(UITableView *)table numberOfRowsInSection:(NSInteger)section
{
	return 3;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	UITableViewCell *cell;
    NSString *identifier;
    
    if (indexPath.row == 2) {
    	identifier = @"selectable";
    } else {
    	identifier = @"non selectable";
    }
	cell = [tableView dequeueReusableCellWithIdentifier:identifier];
	if (cell == nil) {
		cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:identifier] autorelease];
        cell.textLabel.font = [UIFont systemFontOfSize:16];
        cell.textLabel.textAlignment = UITextAlignmentRight;
        if (indexPath.row != 2) {
            cell.selectionStyle = UITableViewCellSelectionStyleNone;
        }
    }
    switch(indexPath.row) {
    	case 0:
        	cell.textLabel.text = currentIp;
        	break;
    	case 1:
        	cell.textLabel.text = proxyPort;
        	break;
    	case 2:
        	cell.textLabel.text = pacURL;
        	break;
    }
    return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
	return NO;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	[tableView selectRowAtIndexPath:nil animated:YES scrollPosition:UITableViewScrollPositionNone];
    
    UIActionSheet *test;
    
    test = [[UIActionSheet alloc] initWithTitle:@"Pac URL" delegate:self cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil otherButtonTitles:@"Send by Email", @"Copy URL", nil];
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
                [messageController setMessageBody:[NSString stringWithFormat:@"pac url : %@\n", pacURL] isHTML:NO];
                [self presentModalViewController:messageController animated:YES];
                [messageController release];
            }
            break;
        case 1:
        	{
				NSDictionary *items;
                
				items = [NSDictionary dictionaryWithObjectsAndKeys:pacURL, kUTTypePlainText, pacURL, kUTTypeText, pacURL, kUTTypeUTF8PlainText, [NSURL URLWithString:pacURL], kUTTypeURL, nil];
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
