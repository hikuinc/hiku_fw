//
//  AppDelegate.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "AppDelegate.h"
#import "StartProvisionViewController.h"
#import "Constant.h"
#import "Reachability.h"
#import "GRAlertView.h"
//#import "ScanViewController.h"

@implementation AppDelegate
@synthesize window =_window;
@synthesize navigationController;

- (void)dealloc
{
    [_window release];
    [super dealloc];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
     NSString *text = @"The device is configured with provided settings. However device can’t connect to configured network. Reason: <b>Authentication failure</b>.";
    self.window = [[[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]] autorelease];
    // Override point for customization after application launch.
    SetisProvisioned(NO);
//    ScanViewController *splash = [[ScanViewController alloc]initWithNibName:@"ScanView" bundle:nil];
//    navigationController = [[UINavigationController alloc] initWithRootViewController:splash];
//    navigationController.navigationBarHidden = YES;
//    GRAlertView *gralertView = [[GRAlertView alloc]initWithTitle:@"" message:@"OK" delegate:self cancelButtonTitle:@"Cancel" otherButtonTitles:nil, nil];
//    [gralertView setStyle:GRAlertStyleAlert];
//    [gralertView layoutIfNeeded];
//    [gralertView show];
    
    StartProvisionViewController *splash = [[StartProvisionViewController alloc]initWithNibName:@"StartProvisionViewController" bundle:nil];
    navigationController = [[UINavigationController alloc] initWithRootViewController:splash];
    navigationController.navigationBarHidden = YES;
    
    self.window.rootViewController = navigationController;
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    [splash release];
    
    NSLog(@"IOS Version: %@",[[UIDevice currentDevice] systemVersion]);

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}
#pragma mark - ActivityIndicator methods

/*
-(void)hideHUDForView:(UIView *)view {
    
    HideNetworkActivityIndicator();
	[MBProgressHUD hideHUDForView:view animated:YES];
}


-(void)showHUDAddedTo:(UIView *)view {
    
	ShowNetworkActivityIndicator();
    hud =[MBProgressHUD showHUDAddedTo:view animated:YES];
	hud.labelText= @"Loading...";
}


-(void)showHUDAddedTo:(UIView *)view message:(NSString *)message {
    
    [self hideHUDForView:view];
    ShowNetworkActivityIndicator();
	hud =[MBProgressHUD showHUDAddedTo:view animated:YES];
	hud.labelText= message;
}


-(void)UpdateMessage:(NSString *)message {
    
    if(hud!=nil && hud.labelText!=nil)
        hud.labelText= message;
}
 */

#pragma mark - Network Indicator
-(void)hideHUDForView:(UIView *)view 
{
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:NO];
	[MBProgressHUD hideHUDForView:view animated:YES];
}

-(void)showHUDAddedTo:(UIView *)view
{
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:YES];
	hud =[MBProgressHUD showHUDAddedTo:view animated:YES];
	hud.labelText=@"Downloading";
}
-(void)showHUDAddedTo:(UIView *)view message:(NSString *)message 
{
    //[self hideHUDForView:view];
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:YES];
	hud =[MBProgressHUD showHUDAddedTo:view animated:YES];
	hud.labelText=message;
    hud.backgroundColor = [UIColor blackColor];
    hud.alpha = 0.6;
}

-(void)UpdateMessage:(NSString *)message 
{
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:YES];
	if(hud!=nil && hud.labelText!=nil)
        hud.labelText=message;
}


//Checks whether wifi is reachable or not
+(BOOL)isWiFiReachable
{
    Reachability *wifiReach = [[Reachability reachabilityForLocalWiFi] retain];
    NetworkStatus netStatus2 = [wifiReach currentReachabilityStatus];
    if(netStatus2 == NotReachable)
    {
        return NO;
    }
    else
    {
        return YES;
    }
    return YES;
}

@end
