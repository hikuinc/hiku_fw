//
//  ProgressBar.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "ProgressBar.h"

@implementation ProgressBar

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self addSubview:self.progressView];
        // Initialization code
    }
    return self;
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    // Drawing code
}
*/

- (void)dealloc {
    [_progressView release];
    [super dealloc];
}
@end
