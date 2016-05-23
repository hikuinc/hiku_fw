//
//  DevicesCell.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "DevicesCell.h"

@implementation DevicesCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self) {
        // Initialization code
    }
    return self;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
    [super setSelected:selected animated:animated];

    // Configure the view for the selected state
}

- (void)dealloc {
    [_lblSSID release];
    [_lblProvisionStatus release];
    [super dealloc];
}
@end
