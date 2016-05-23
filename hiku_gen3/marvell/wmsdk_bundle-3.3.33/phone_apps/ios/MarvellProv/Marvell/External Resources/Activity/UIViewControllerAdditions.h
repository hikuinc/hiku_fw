//
//  UIViewControllerAdditions.h
//  MapCut
//
//  Created by merit info solutions on 4/29/10.
//  Copyright merit info solutions 2010. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface UIViewController (UIViewControllerAdditions)


-(void) showMessage:(NSString *)text withTitle:(NSString *)title;
-(void) showMessage:(NSString *)text withTag:(int)tag withTarget:(id)target;
-(void) showMessage:(NSString *)text ;

-(void) MemoryReport ;
-(void) updateProgressMessage:(NSString*)message;
-(void) setBusy:(BOOL)busy;
-(void) setBusy:(BOOL)busy forMessage:(NSString*)message;

@end
