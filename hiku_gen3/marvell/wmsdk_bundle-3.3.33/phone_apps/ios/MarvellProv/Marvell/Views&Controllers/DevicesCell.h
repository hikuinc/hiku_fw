//
//  DevicesCell.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface DevicesCell : UITableViewCell
@property (retain, nonatomic) IBOutlet UILabel *lblSSID;
@property (retain, nonatomic) IBOutlet UILabel *lblProvisionStatus;

@end
