//
//  ParseXMLDelegate.h
 
//
//  Created by iPhone Developer.
//  Copyright 2011. All rights reserved.
//

#import <Foundation/Foundation.h>


@class ParseJSON;

@protocol ParseJSONDelegate<NSObject>

@optional

-(void)finishParserJSON:(ParseJSON *)parseJSON;

@end
