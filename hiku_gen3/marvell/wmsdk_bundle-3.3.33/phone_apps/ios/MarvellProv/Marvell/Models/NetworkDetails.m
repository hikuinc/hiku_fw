//
//  NetworkDetails.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "NetworkDetails.h"

@implementation NetworkDetails
@synthesize ssid;
@synthesize bssid;
@synthesize security;
@synthesize channel;
@synthesize rssi;
@synthesize nf;


-(void)dealloc
{
    [super dealloc];
    [ssid release];
    [bssid release];
   
    
}

@end
