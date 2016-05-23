//
//  ProgressViewController.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ProgressViewController : UIViewController

@property (retain, nonatomic) IBOutlet UIImageView *imgProgressBar;
@property (retain, nonatomic) IBOutlet UILabel *lblMessage;
-(void) startAnimation :(NSString*)message;
-(void) stopAnimation;
-(void)UpdateMessage :(NSString*)strMsg;
@end
