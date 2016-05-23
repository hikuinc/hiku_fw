//
//  ServerRequestDelegate.h
 
//
//  Created by iPhone Developer.
//  Copyright 2011. All rights reserved.
//

#import <Foundation/Foundation.h>

@class ServerRequest;

@protocol ServerRequestDelegate<NSObject>
@required
-(void)serverRequest:(ServerRequest *)server didFailWithError:(NSError *)error;
-(void)serverRequestDidFinishLoading:(ServerRequest *)server;
@end
