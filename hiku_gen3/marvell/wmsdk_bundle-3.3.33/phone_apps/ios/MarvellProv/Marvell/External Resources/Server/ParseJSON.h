//
//  ParseJSON.h
//  Jobatar
//
//  Created by vikie on 06/02/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ParseJSONDelegate.h"

@class Candidate;

@interface ParseJSON : NSObject
{
    NSDictionary *JSONValue;
}

@property (retain) NSMutableArray *listData;
@property (assign) NSString * method;
@property (assign) id<ParseJSONDelegate>delegate; 

- (id)initSOAP:(NSString *)data method:(NSString*)method;
- (void)parser ;

@end
