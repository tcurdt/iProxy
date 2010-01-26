//
//  InstructionsViewController.m
//  iProxy
//
//  Created by Jon Maddox on 1/22/10.
//  Copyright 2010 Fanzter, Inc.. All rights reserved.
//

#import "InstructionsViewController.h"


@implementation InstructionsViewController

- (void)dealloc {
  [super dealloc];
}

- (void)loadView{
  [super loadView];
  
  [self setTitle:@"Instructions"];

  UIImage *instructionsImage = [UIImage imageNamed:@"instructions.jpg"];
	UIImageView *imageView = [[[UIImageView alloc] initWithImage:instructionsImage] autorelease];

  UIScrollView *scrollView = [[[UIScrollView alloc] initWithFrame:CGRectZero] autorelease];
  [scrollView setBackgroundColor:[UIColor blackColor]];
	[scrollView setFrame:self.view.bounds];
	scrollView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	scrollView.contentSize = instructionsImage.size;

  [scrollView addSubview:imageView];
  [self.view addSubview:scrollView];
  
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(close)];
}

- (void)close{
  [self dismissModalViewControllerAnimated:YES];
}

/*
- (void)viewDidLoad {
    [super viewDidLoad];

    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}
*/

/*
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
}
*/
/*
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
}
*/
/*
- (void)viewWillDisappear:(BOOL)animated {
	[super viewWillDisappear:animated];
}
*/
/*
- (void)viewDidDisappear:(BOOL)animated {
	[super viewDidDisappear:animated];
}
*/

/*
// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}
*/

- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}


@end

