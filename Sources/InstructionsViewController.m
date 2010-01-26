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

#import "InstructionsViewController.h"


@implementation InstructionsViewController

- (void)loadView
{
    [super loadView];
    
    self.title = @"Instructions";

    UIImage *instructionsImage = [UIImage imageNamed:@"Instructions.jpg"];
    UIImageView *imageView = [[[UIImageView alloc] initWithImage:instructionsImage] autorelease];

    UIScrollView *scrollView = [[[UIScrollView alloc] initWithFrame:CGRectZero] autorelease];
    [scrollView setBackgroundColor:[UIColor blackColor]];
    [scrollView setFrame:self.view.bounds];
    scrollView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    scrollView.contentSize = instructionsImage.size;

    [scrollView addSubview:imageView];
    [self.view addSubview:scrollView];
  
    self.navigationItem.rightBarButtonItem = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(close)] autorelease];
}

- (void)close
{
    [self dismissModalViewControllerAnimated:YES];
}

@end

