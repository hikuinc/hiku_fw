//
//  AppDelegate.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "MBProgressHUD.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate>
{
    MBProgressHUD *hud;
}

@property (strong, nonatomic)  UIWindow *window;
@property (nonatomic, retain)  UINavigationController  *navigationController;
@property (nonatomic, retain)  NSString *strNetworkName;

-(void)hideHUDForView:(UIView *)view;
-(void)showHUDAddedTo:(UIView *)view;
-(void)showHUDAddedTo:(UIView *)view message:(NSString *)message;
-(void)UpdateMessage:(NSString *)message;
+(BOOL)isWiFiReachable;

@end
