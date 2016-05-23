//
//  ProgressViewController.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "ProgressViewController.h"

@interface ProgressViewController ()

@end

@implementation ProgressViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}
- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
//    NSArray *gif = [NSArray arrayWithObjects:[UIImage imageNamed: @"animation0001.png"],[UIImage imageNamed:@"animation0011.png"],[UIImage imageNamed:@"animation0026.png"],[UIImage imageNamed:@"animation0041.png"],[UIImage imageNamed:@"animation0050.png"], nil];
//    self.imgProgressBar.animationImages = gif;
//    self.imgProgressBar.animationRepeatCount = MAXFLOAT;
//    self.imgProgressBar.animationDuration = .3;
//    [self.imgProgressBar startAnimating];
    // Do any additional setup after loading the view from its nib.
}

-(void)UpdateMessage :(NSString*)strMsg
{
    self.lblMessage.text =   strMsg;
}
-(void) startAnimation :(NSString*)message
{
    if (message.length>0)
    {
        self.lblMessage.hidden = NO;
        self.lblMessage.text =   message;
    }
    else
    {
        self.lblMessage.hidden = YES;
    }
    NSMutableArray *gif = [[NSMutableArray alloc]init];
    for ( int index=1; index<=50; index++)
    {
        NSString *fileName = [NSString stringWithFormat:@"animation00%02d.png",index];
       // NSLog(@"filename %@",fileName);
        [gif addObject:[UIImage imageNamed:fileName]];
    }
//   NSArray *gif1 = [NSArray arrayWithObjects:[UIImage imageNamed: @"animation0001.png"],[UIImage imageNamed: @"animation0002.png"],[UIImage imageNamed: @"animation0003.png"],[UIImage imageNamed: @"animation0004.png"],[UIImage imageNamed: @"animation0005.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0007.png"],[UIImage imageNamed: @"animation0008.png"],[UIImage imageNamed: @"animation0009.png"],[UIImage imageNamed: @"animation0010.png"],[UIImage imageNamed: @"animation0011.png"],[UIImage imageNamed: @"animation0012.png"],[UIImage imageNamed: @"animation0013.png"],[UIImage imageNamed: @"animation0014.png"],[UIImage imageNamed: @"animation0015.png"],[UIImage imageNamed: @"animation0016.png"],[UIImage imageNamed: @"animation0017.png"],[UIImage imageNamed: @"animation0018.png"],[UIImage imageNamed: @"animation0019.png"],[UIImage imageNamed: @"animation0020.png"],[UIImage imageNamed: @"animation0021.png"],[UIImage imageNamed: @"animation0022.png"],[UIImage imageNamed: @"animation0023.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"],[UIImage imageNamed: @"animation0006.png"], nil];
    self.imgProgressBar.animationImages = gif;
    self.imgProgressBar.animationRepeatCount = MAXFLOAT;
    self.imgProgressBar.animationDuration = 3;
    [self.imgProgressBar startAnimating];
}
-(void) stopAnimation
{
    [self.imgProgressBar stopAnimating];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    [_imgProgressBar release];
    [_lblMessage release];
    [super dealloc];
}
- (void)viewDidUnload {
    [self setImgProgressBar:nil];
    [self setLblMessage:nil];
    [super viewDidUnload];
}
@end
