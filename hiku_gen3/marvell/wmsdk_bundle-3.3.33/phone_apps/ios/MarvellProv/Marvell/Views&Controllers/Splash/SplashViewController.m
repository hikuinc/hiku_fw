//
//  SplashViewController.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "SplashViewController.h"
#import "StartProvisionViewController.h"
#import <QuartzCore/QuartzCore.h>
#import "Constant.h"
#import "ScanViewController.h"

@interface SplashViewController ()

- (CATransition*) getAnimation;
- (void) GotoStartProvision;

@end

@implementation SplashViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (CATransition*) getAnimation
{
    CATransition *animation = [CATransition animation];
    [animation setDelegate:self];
    [animation setDuration:2.0f];
    [animation setTimingFunction:UIViewAnimationCurveEaseInOut];
    [animation setType:@"unGenieEffect" ];
    [imgSplash.layer addAnimation:animation forKey:NULL];
    
	return animation;
}

- (void) GotoStartProvision
{
    if (GetisProvisioned == NO)
    {
    StartProvisionViewController *provision = [[StartProvisionViewController alloc]initWithNibName:@"StartProvisionViewController" bundle:nil];
	
	CATransition * animation = [self getAnimation];
    
    AppDelegate *appDelegate = AppDelegate(AppDelegate);
    
	[[appDelegate.window layer] addAnimation:animation forKey:@"Animation"];
	
	[self.navigationController pushViewController:provision	animated:NO];
	[provision release];
    }
    else
    {
        ScanViewController *provision = [[ScanViewController alloc]initWithNibName:@"ScanView" bundle:nil];
        
        CATransition * animation = [self getAnimation];
        
        AppDelegate *appDelegate = AppDelegate(AppDelegate);
        
        [[appDelegate.window layer] addAnimation:animation forKey:@"Animation"];
        
        [self.navigationController pushViewController:provision	animated:NO];
        [provision release];
    }
}


- (void)viewDidLoad
{
    [super viewDidLoad];
    // Do any additional setup after loading the view from its nib.
    
    [self performSelector:@selector(GotoStartProvision) withObject:nil afterDelay:0.5];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    [imgSplash release];
    [super dealloc];
}
@end
