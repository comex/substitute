//
//  AutoGrid.h
//  SafetyDance
//
//  Created by Nicholas Allegra on 1/26/15.
//  Copyright (c) 2015 Nicholas Allegra. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface AutoGrid : UIView {
    NSArray *views;
    UIScrollView *scrollView;
}
- (void)setViews:(NSArray *)views;

@end
