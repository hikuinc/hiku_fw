//
//  ParseJSON.m
//  Jobatar
//
//  Created by vikie on 06/02/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#import "ParseJSON.h"
#import "JSON.h"

@implementation ParseJSON

@synthesize listData;
@synthesize method;
@synthesize delegate;

- (id)initSOAP:(NSString *)data method:(NSString*)_method
{
    self = [super init];
    if(self){
        
        self.listData = [[[NSMutableArray alloc] init] autorelease];
        JSONValue = [data JSONValue];
        self.method = _method;
    }
    return self;
}

-(void)dealloc {
    
    JSONValue = nil;//[JSONValue release];
    [listData release];
    [method release];
    [super dealloc];
}

- (void)parser
{
    if([self.method isEqualToString:@"network"]) {
        
        ALog(@"CL : %@",JSONValue);
        
        if ([[JSONValue objectForKey:@"ReturnVal"] intValue] != 0) {
            
        }
    }
    [self.delegate finishParserJSON:self];
}


@end


