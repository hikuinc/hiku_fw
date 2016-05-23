//
//  StringUtil.h
//  TwitterFon
//
//  Created by kaz on 7/20/08.
//  Copyright 2008 naan studio. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface NSString (NSStringUtils)
- (NSString*)encodeAsURIComponent;
- (NSString*)escapeHTML;
- (NSString*)unescapeHTML;
+ (NSString*)localizedString:(NSString*)key;
+ (NSString*)base64encode:(NSString*)str;
+ (NSString *)decodeBase64:(NSString *)input;
- (NSString*)Trim;
- (BOOL)isEmpty;
+ (NSString *)generateUUID;
-(BOOL)filterType:(NSString*)type inputValue:(NSString*)value length:(int)length maxLength:(int)maxLength;
-(NSString*)stringBetweenString:(NSString*)start andString:(NSString*)end;
@end


