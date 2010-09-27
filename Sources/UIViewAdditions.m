#import "UIViewAdditions.h"

@implementation UIView (UIViewAdditions)

- (UIView*) addTaggedSubview:(UIView*)theView
{
    for(UIView *view in [self subviews]) {
    
        if (view.tag == theView.tag && view != theView) {
            UIView *removedView = view;
            [self insertSubview:theView aboveSubview:view];
            [view removeFromSuperview];
            // NSLog(@"replaced %@ with %@", removedView, theView);
            return removedView;
        }
    }

    [self addSubview:theView];    
    return nil;
}

@end
