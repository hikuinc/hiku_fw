//
//  NetworkDetails.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NetworkDetails : NSObject
@property (nonatomic, retain)NSString *ssid;
@property (nonatomic, retain)NSString *bssid;
@property (nonatomic, assign)int security;
@property (nonatomic, assign)int channel;
@property (nonatomic, assign)int rssi;
@property (nonatomic, assign)int nf;


@end
