//
//  AutoGrid.m
//  SafetyDance
//
//  Created by Nicholas Allegra on 1/26/15.
//  Copyright (c) 2015 Nicholas Allegra. All rights reserved.
//

#import "AutoGrid.h"

@implementation AutoGrid
- (void)setViews:(NSArray *)_views {
    views = _views;
    [scrollView removeFromSuperview];
    scrollView = [[UIScrollView alloc] init];
    [self addSubview:scrollView];
    for (UIView *view in views)
        [scrollView addSubview:view];
    [self setNeedsLayout];
}

- (void)layoutSubviews {
    scrollView.frame = self.bounds;
    CGFloat paddingX = 22, paddingY = 10;
    NSUInteger nviews = [views count];
    CGSize *sizes = malloc(sizeof(*sizes) * nviews);

    for (NSUInteger i = 0; i < nviews; i++)
        sizes[i] = [[views objectAtIndex:i] intrinsicContentSize];

    CGFloat availableWidth = self.bounds.size.width;
    /* try to lay out using an increasing number of columns */
    NSUInteger cols;
    CGSize contentSize;
    CGFloat *colWidths = NULL;
    for (cols = 1; ; cols++) {
        free(colWidths);
        colWidths = malloc(sizeof(*colWidths) * cols);
        for (NSUInteger col = 0; col < cols; col++)
            colWidths[col] = 0;
        CGFloat tentativeHeight = 0;
        CGFloat tentativeWidth = 0;
        for (NSUInteger row = 0; row < nviews / cols; row++) {
            CGFloat totalWidth = 0;
            CGFloat maxHeight = 0;
            for (NSUInteger col = 0; col < cols; col++) {
                NSUInteger i = row * cols + col;
                if (i >= nviews)
                    goto done1;
                CGSize size = sizes[i];
                if (size.width > colWidths[col])
                    colWidths[col] = size.width;
                if (col != 0)
                    totalWidth += paddingX;
                totalWidth += size.width;
                if (size.height > maxHeight)
                    maxHeight = size.height;
            }
            if (totalWidth > tentativeWidth)
                tentativeWidth = totalWidth;
            tentativeHeight += maxHeight + paddingY;
        }
    done1:
        if (cols > 1 && tentativeWidth > availableWidth) {
            cols--;
            break;
        }
        contentSize = CGSizeMake(tentativeWidth, tentativeHeight);
        /* NSLog(@"%f", contentSize.height); */
        if (contentSize.width == 0)
            break;

    }
    scrollView.contentSize = contentSize;
    CGFloat y = 0;
    for (NSUInteger row = 0; ; row++) {
        CGFloat x = 0;
        CGFloat maxHeight = 0;
        for (NSUInteger col = 0; col < cols; col++) {
            NSUInteger i = row * cols + col;
            if (i >= nviews)
                goto done2;
            CGSize size = sizes[i];
            UIView *view = [views objectAtIndex:i];
            if (col != 0)
                x += paddingX;
            view.frame = CGRectMake(x, y, size.width, size.height);
            x += colWidths[col];
            if (size.height > maxHeight)
                maxHeight = size.height;
        }
        y += maxHeight + paddingY;
    }
done2:
    free(sizes);
    free(colWidths);
}
/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

@end
