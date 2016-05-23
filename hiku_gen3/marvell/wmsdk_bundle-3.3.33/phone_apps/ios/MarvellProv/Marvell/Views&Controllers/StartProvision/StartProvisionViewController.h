//
//  StartProvisionViewController.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ServerRequest.h"
#import "TPKeyboardAvoidingScrollView.h"
#import <QuartzCore/QuartzCore.h>
#import <CoreText/CoreText.h>

#define degreesToRadians(degrees) (M_PI * degrees / 180.0)

@interface StartProvisionViewController : UIViewController <ServerRequestDelegate,UIAlertViewDelegate>
{
    IBOutlet UITextField *txtNetworkName;
    IBOutlet UITextField *txtPassword;
    IBOutlet UITextView *lblStatus;
    IBOutlet UITableView *networkTableView;
    int Mode;
    NSString *SelectedSSID;
    NSMutableArray *networkList;
    NSMutableArray *networkSecurity;
    NSMutableArray *networkChannel;
    BOOL isOpened;
    BOOL isOpen;
    BOOL isChecked;
    NSTimer *timer;
    int TimerCount;
    UIAlertView *alertVw;
    
}
@property (retain, nonatomic) IBOutlet UIView *popViewSecurity;
@property (retain, nonatomic) IBOutlet UIButton *btnNetwork;
@property (retain, nonatomic) IBOutlet UIButton *btnProvision;
@property (retain, nonatomic) IBOutlet UIButton *btnRememberPassword;
@property (assign, nonatomic) NSInteger security;
@property (assign, nonatomic) NSInteger channel;
@property (retain, nonatomic) NSString *strCurrentSSID;
@property (retain, nonatomic) IBOutlet TPKeyboardAvoidingScrollView *kScrollView;
@property (retain, nonatomic) IBOutlet UILabel *lblPassphrase;
@property (retain, nonatomic) IBOutlet UIView *viewBottom;
@property (retain, nonatomic) IBOutlet UIImageView *imgViewNetwork;
@property (retain, nonatomic) IBOutlet UIImageView *imgViewPassword;
@property (retain, nonatomic) IBOutlet UIImageView *imgarrow;
@property (retain, nonatomic) IBOutlet UILabel *lblNetwork;
@property (retain, nonatomic) IBOutlet UIView *viewPassphrase;
@property (retain, nonatomic) IBOutlet UIImageView *imgViewDDown;
@property (retain, nonatomic) IBOutlet UIImageView *imgProgressBar;
- (IBAction)OnProvisionClick:(id)sender;
- (IBAction)ChooseNetwork:(id)sender;
- (IBAction)UnmaskPassword:(id)sender;
- (IBAction)RememberPassword:(id)sender;
@end
