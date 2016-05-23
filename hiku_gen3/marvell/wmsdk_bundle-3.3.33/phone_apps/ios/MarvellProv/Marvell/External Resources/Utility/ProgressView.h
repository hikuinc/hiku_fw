//
//  ProgressView.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ProgressView : UIViewController
@property (retain, nonatomic) IBOutlet UIImageView *imgProgressBar;

-(void) startAnimation;
-(void) stopAnimation;

@end
