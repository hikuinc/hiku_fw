//
//  ScanViewController.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "DevicesCell.h"

@interface ScanViewController : UIViewController<UIAlertViewDelegate>
@property (nonatomic,retain)IBOutlet DevicesCell *devicesCell;
@property (nonatomic,retain) NSMutableArray *arrResults;
@property (retain, nonatomic) IBOutlet UILabel *lblTitle;
@property (retain, nonatomic) IBOutlet UIButton *btnStartProvision;
@property (retain, nonatomic) IBOutlet UITableView *tblView;
- (IBAction)StartProvisioning:(id)sender;
@property (retain, nonatomic) IBOutlet UILabel *lblNetworkName;
@property (retain, nonatomic) NSString *strCurrentSSID;

@end
