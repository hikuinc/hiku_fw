//
//  UIViewControllerAdditions.m
//  MapCut
//
//  Created by merit info solutions on 4/29/10.
//  Copyright merit info solutions 2010. All rights reserved.
//

#import "UIViewControllerAdditions.h"
#import "Constant.h"
#import <mach/mach.h>


@implementation UIViewController (UIViewControllerAdditions)


#pragma mark UIAlertView methods

- (void)showMessage:(NSString *)text withTitle:(NSString *)title {
	UIAlertView * alert =[[UIAlertView alloc] initWithTitle:title 	
													message:text 
												   delegate:nil 
										  cancelButtonTitle:@"OK"
										  otherButtonTitles:nil];
	[alert show];
	[alert release];
}
-(void) showMessage:(NSString *)text withTag:(int)tag withTarget:(id)target
{
    UIAlertView * alert =[[UIAlertView alloc] initWithTitle:@"" 	
													message:text 
												   delegate:nil 
										  cancelButtonTitle:@"OK"
										  otherButtonTitles:nil,nil];
    [alert setDelegate:target];
    [alert setTag:tag];
    [alert show];
	[alert release];
}

- (void)showMessage:(NSString *)text  {
	UIAlertView * alert =[[UIAlertView alloc] initWithTitle:@""
													message:text
												   delegate:nil
										  cancelButtonTitle:@"OK"
										  otherButtonTitles:nil];
	[alert show];
	[alert release];
}




#pragma mark ProgressView methods

-(void) setBusy:(BOOL)busy
{
    if(!busy)
        [AppDelegate(AppDelegate) hideHUDForView:self.view];
    else
        [AppDelegate(AppDelegate) showHUDAddedTo:self.view];
}


-(void) setBusy:(BOOL)busy forMessage:(NSString*)message
{
    if(!busy)
        [AppDelegate(AppDelegate) hideHUDForView:self.view];
    else
        [AppDelegate(AppDelegate) showHUDAddedTo:self.view message:message];
    
}

-(void) updateProgressMessage:(NSString*)message
{
    [AppDelegate(AppDelegate) UpdateMessage:message];
}



#pragma mark - Utilities methods 
 
-(void) MemoryReport 
{
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t kerr = task_info(mach_task_self(),
                                   TASK_BASIC_INFO,
                                   (task_info_t)&info,
                                   &size);
    if( kerr == KERN_SUCCESS ) {
        
        //NSLog(@"%@: Memory in use %d mb",NSStringFromClass([self class]),info.resident_size/(1024*1024));
    } 
    else {
        //NSLog(@"Error with task_info(): %s", mach_error_string(kerr));
    }
    
}

@end
