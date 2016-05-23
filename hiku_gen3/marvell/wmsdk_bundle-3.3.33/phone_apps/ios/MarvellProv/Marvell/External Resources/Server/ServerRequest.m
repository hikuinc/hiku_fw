//
//  ServerRequest.m
//  ClubManagement
//
//  Created by Gnanavadivelu on 10/08/12.
//  Copyright 2011. All rights reserved.
//

#import "ServerRequest.h"
#import "Constant.h"
#import "ASIHTTPRequest.h"
#import "JSON.h"

#define URLSOAP @"http://tempuri.org"
#define PageSize 20
#define TIMEOUT 15


@implementation ServerRequest

@synthesize delegate, object = _object, method = _method, params = _params , RMethod = _RMethod;
@synthesize responseString = _responseString;

#pragma mark Static
static NSMutableArray * __requests;
+ (NSMutableArray *)requests {
	@synchronized([ServerRequest class]) {
		if (__requests == nil)
			__requests = [NSMutableArray new];
		return __requests;
	}
}

+ (void)cancelPendingRequestsForObject:(NSString *)object method:(NSString *)method {
	@synchronized([ServerRequest class]) {
		for (ServerRequest * server in [self requests]) {
			if ((object == nil || [server.object isEqualToString:object]) &&
				(method == nil || [server.method isEqualToString:method])) {
				[server abort];
			}
		}
	}
}

#pragma mark Helpers

- (void)abort {
	//[_connection cancel];
}

+ (BOOL)pendingRequestsForObject:(NSString *)object method:(NSString *)method {
	@synchronized([ServerRequest class]) {
		for (ServerRequest *server in [self requests]) {
			if ((object == nil || [server.object isEqualToString:object]) &&
				(method == nil || [server.method isEqualToString:method])) {
				if (server->delegate != nil)
					return YES;
			}
		}
	}
	
	return NO;
}

- (NSString *)paramsAsKeyValuePairs:(NSDictionary *)params  keys:(NSMutableArray *)keys{
    
	NSMutableString * pairs = [[[NSMutableString alloc] initWithCapacity:[params count]] autorelease];
	for (NSString * key in keys) {
        [pairs appendFormat:@"<%@>%@</%@>\n",
         [key stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
         [[params objectForKey:key] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]],
         [key stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
	}
	return pairs;
}

#pragma mark Public Methods


- (id)initWithJSONMethod:(NSString *)method params:(NSString*)requestString :(NSString*)RMethod
{
   // assert (method != nil && [method length] > 0);
    
	self = [super init];
	if (self) {
        @synchronized([ServerRequest class]) {
			[[ServerRequest requests] addObject:self];
		}
        
		_method = [method retain];
        _RMethod = [RMethod retain];
       
        NSData* requestData = [requestString dataUsingEncoding:NSUTF8StringEncoding];
        
        NSURL *requestURL = [[NSURL alloc] initWithString:[NSString stringWithFormat:@"%@%@",LOCAL_URL,method]];
        
        NSLog(@"requestURL:%@",requestURL);
        request = [ASIHTTPRequest requestWithURL:requestURL];
        [request setRequestMethod:RMethod];//@"POST"
        [request addRequestHeader:@"Content-Type" value:@"application/json"];
        [request setTimeOutSeconds:20];
        [request setShouldAttemptPersistentConnection:NO];
        [request setPostBody:[NSMutableData dataWithData:requestData]];
        
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_4_0
        [request setShouldContinueWhenAppEntersBackground:YES];
#endif
        
        [request setDelegate:self];
        [request setDidFailSelector:@selector(requestFail:)];
        [request setDidFinishSelector:@selector(requestFinished:)];
        
        [requestURL release];
	}
	return self;
}




 
- (void)stopLoading {
	
	[delegate release];
	delegate = nil;
}
- (void)startAsynchronous
{
    [request startAsynchronous];   
}

- (void)load {
	//assert (_connection != nil);
	//assert (delegate != nil);
	//assert (_downloadedData == nil);
	
	//[_connection start];
}




#pragma mark ASIHTTPRequest delegate

-(void) requestFail:(ASIHTTPRequest*) __request {
    int statusCode = [request responseStatusCode];
    if (statusCode == 400)
    {
        if ([self.method isEqualToString:@"network"])
        {
            UIAlertView *alertView = [[UIAlertView alloc]initWithTitle:@"" message:@"Could not configure network, please check your connection" delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];
            [alertView show];
            [alertView release];
        }
        else if ([self.method isEqualToString:@""])
        {
            UIAlertView *alertView = [[UIAlertView alloc]initWithTitle:@"" message:@"Not a valid device, please check your connection " delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];
            [alertView show];
            [alertView release];
           
        }
        //Could not configure network, please check your connection
    }
    else
    {
    [self.delegate serverRequest:self didFailWithError:__request.error];
    }
}


-(void) requestFinished:(ASIHTTPRequest*) __request
{
    NSLog(@"Network::response %@",[request responseString]);
    self.responseString = __request.responseString;
    int success = [[[request.responseString JSONValue] objectForKey:@"success"] intValue];
    if(success == 0)
    {
       [self.delegate serverRequestDidFinishLoading:self]; 
    }
    else
    {
        [self.delegate serverRequest:self didFailWithError:__request.error];
    }
    
}

 
#pragma mark NSObject Members

- (void)dealloc {
    @synchronized([ServerRequest class]) {
		[[ServerRequest requests] removeObject:self];
	}
    [_object release];
	[_method release];
    [_RMethod release];
	[_params release];
	[delegate release];
    [_responseString release];
	
	[super dealloc];
}

@end
