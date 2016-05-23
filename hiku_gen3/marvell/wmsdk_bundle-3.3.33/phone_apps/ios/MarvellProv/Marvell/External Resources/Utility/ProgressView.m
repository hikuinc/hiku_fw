//
//  ProgressView.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "ProgressView.h"

@implementation ProgressView

- (void)dealloc {
    [_imgProgressBar release];
    [super dealloc];
}
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}



-(void) viewDidLoad
{
    NSArray *gif = [NSArray arrayWithObjects:[UIImage imageNamed: @"animation0001.png"],[UIImage imageNamed:@"animation0011.png"],[UIImage imageNamed:@"animation0026.png"],[UIImage imageNamed:@"animation0041.png"],[UIImage imageNamed:@"animation0050.png"], nil];
    //self.imgProgressBar.animationImages = gif;
    self.imgProgressBar.animationRepeatCount = MAXFLOAT;
    self.imgProgressBar.animationDuration = .3;
    [self.imgProgressBar startAnimating];
    
    [super viewDidLoad];
}
-(void) startAnimation
{
    NSArray *gif = [NSArray arrayWithObjects:@"animation0001.png",@"animation0011.png",@"animation0026.png",@"animation0041.png",@"animation0050.png", nil];
    self.imgProgressBar.animationImages = gif;
    self.imgProgressBar.animationRepeatCount = MAXFLOAT;
    self.imgProgressBar.animationDuration = .3;
    [self.imgProgressBar startAnimating];
}
-(void) stopAnimation
{
     [self.imgProgressBar stopAnimating];
}
- (void)viewDidUnload {
    [self setImgProgressBar:nil];
    [super viewDidUnload];
}
@end
