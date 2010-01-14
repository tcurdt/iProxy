//
//  iProxyAppDelegate.h
//  iProxy
//
//  Created by Torsten Curdt on 23.12.09.
//  Copyright vafer.org 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

@class iProxyViewController;

@interface iProxyAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
    iProxyViewController *viewController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet iProxyViewController *viewController;

@end

