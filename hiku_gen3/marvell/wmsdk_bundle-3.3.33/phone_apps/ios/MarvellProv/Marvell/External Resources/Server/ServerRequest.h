//
//  ServerRequest.h

//
//  Created by iPhone Developer.
//  Copyright 2011. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ServerRequestDelegate.h"
#import "ParseJSON.h"

@class ASIHTTPRequest;
@class ParseJSON;

@interface ServerRequest : NSObject<ParseJSONDelegate>
{
    NSString * _object;
    NSString * _method;
    NSDictionary * _params;
    NSString * _responseString;
    NSString *_RMethod;
    
    id<ServerRequestDelegate>delegate;
    ASIHTTPRequest *request;
}
@property (assign) id<ServerRequestDelegate>delegate;
@property (readonly) NSString * object;
@property (readonly) NSString * method;
@property (readonly) NSDictionary * params;
@property (assign) NSString * responseString;
@property (readonly)NSString * RMethod;

- (void)abort;
- (void)load ;
- (void)startAsynchronous;
 


- (id)initWithJSONMethod:(NSString *)method params:(NSString*)requestString :(NSString*)RMethod;
- (NSString *)paramsAsKeyValuePairs:(NSDictionary *)params  keys:(NSMutableArray *)keys;

@end