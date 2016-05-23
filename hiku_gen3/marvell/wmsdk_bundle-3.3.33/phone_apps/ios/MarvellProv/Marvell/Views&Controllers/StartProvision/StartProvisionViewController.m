//
//  StartProvisionViewController.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "StartProvisionViewController.h"
#import <SystemConfiguration/CaptiveNetwork.h>
#import "Reachability.h"
#import "Constant.h"
#import "ServerRequest.h"
#import "JSON.h"
#import "NetworkDetails.h"
#import "ScanViewController.h"
#import "AppDelegate.h"
#import "StringUtil.h"
#import "ASIHTTPRequest.h"
#import "UIViewControllerAdditions.h"
#import "ProgressBar.h"
#import "ProgressView.h"
#import "ProgressViewController.h"
#import "RTLabel.h"
#import "GRAlertView.h"
#import "MessageList.h"

//0 - open, 1 - WEP Open, 3 - WPA , 4 - WPA2 , 5 - WPA Mixed
// Mode = 0 - first call to GET/sys Mode = 1 - Final Call to GET/sys after from Error
#define SECURITY_TYPE_OPEN 0
#define SECURITY_TYPE_WEP  1
#define SECURITY_TYPE_WPA  3
#define SECURITY_TYPE_WPA2 4
#define SECURITY_TYPE_WPA_WPA2 5
#define SECURITY_TYPE_DEFAULT 6
#define PADDING 10



@interface StartProvisionViewController ()
{
    ProgressViewController *progress;
}

@end

@implementation StartProvisionViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}
- (BOOL)prefersStatusBarHidden
{
    if (IOS_7_0_OR_LATER)
    {
        return YES;
    }
    return NO;
}

/* When app is loaded, this function is called */
- (void)viewDidLoad
{
    /* function to customize UI*/
    [self HidePopUp:YES animated:NO];
    
    [super viewDidLoad];
    
    isOpened = NO;
    self.btnProvision.enabled = NO;
    Mode = -1;
    self.imgViewDDown.layer.cornerRadius = 5.0;
    self.viewPassphrase.hidden = YES;

    if (self.btnNetwork.enabled == NO)
    {
        self.btnNetwork.enabled = YES;
    }
    
    TimerCount = 0;
    progress = [[ProgressViewController alloc]init];


    CGRect frame = self.imgViewPassword.frame;
    self.btnProvision.frame = frame;
    
    /* Do any additional setup after loading the view from its nib. */
    isChecked = YES;
    
    /* function to check if device is connected to marvell device's network */
    [self performSelector:@selector(checkWifiConnection) withObject:nil afterDelay:1.0];
}

/* Function to get network devices, using /sys/scan */
-(void)scanDevices
{
    networkList = [NSMutableArray arrayWithObjects:nil];
    networkSecurity = [NSMutableArray arrayWithObjects:nil];
    networkChannel = [NSMutableArray arrayWithObjects: nil];

    NSLog(@"scanning network devices");
    [self ShowProgressView:@"Scanning network devices.."];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
    [NSURL URLWithString:URL_SCAN]];
    [request setHTTPMethod:@"GET"];

    NSError *error = nil;

    NSData *responseData = [NSURLConnection sendSynchronousRequest:request
                                        returningResponse:nil
                                        error:&error];
    if (error)
    {
        NSLog(@"error %@",error);
        [self stopProgressView];
        if (error.code == NSURLErrorTimedOut)
        {
            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_BSSIDNOT_MATCH delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];
        }
        else
        {
            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_NETWORK_TIMEOUT delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];//@"Could not get list of networks, please check your connection"
            [alertVw show];
            [alertVw release];
        }
    }



    if(!error && [responseData length] > 0)
    {
        NSString *responseString = [[NSString alloc] initWithData:responseData  encoding:NSUTF8StringEncoding];
        [lblStatus setText:responseString];
        // NSLog(@"Response String: %@", responseString);

        NSDictionary *JSONValue =  [responseString JSONValue];
        // NSLog(@"Response Array: %@", JSONValue);

        NSArray *networkRecords = [JSONValue objectForKey:@"networks"];
        // NSLog(@"Network Records: %@", networkRecords);
        if (networkRecords)
        {
            for (int aindex =0 ; aindex<[networkRecords count];aindex++)
            {
                NSString *str = networkRecords[aindex][0];
                NSLog(@"SSID: %@", str);
                [networkList addObject:str];

                int temp = [networkRecords[aindex][2] intValue];
                NSLog(@"Security: %d", temp);
                [networkSecurity addObject:[NSNumber numberWithInt:temp]];

                temp = [networkRecords[aindex][3] intValue];
                NSLog(@"Channel :%d", temp);
                [networkChannel addObject: [NSNumber numberWithInt:temp]];
            }
        }
        [self stopProgressView];
        [networkList retain];
        [networkSecurity retain];
        [networkChannel retain];
        [networkTableView reloadData];
    }
}

/* to register function to be called on resuming the app */
-(void)viewWillAppear:(BOOL)animated
{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(checkWifi)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    
    [super viewWillAppear:YES];
}

-(void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:YES];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:UIApplicationWillEnterForegroundNotification object:nil];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    NSString *keyValue = SelectedSSID;
    NSString *valueToSave = txtPassword.text;
    [[NSUserDefaults standardUserDefaults] setObject:valueToSave forKey:keyValue];
    NSLog(@"Application is terminating");
}

- (void)showDeviceNotConnected
{
    if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
    {
        // The device is an iPad running iOS 3.2 or later.
        [self showMessage:MARVELL_NO_NETWORK_IPAD withTitle:@""];//\nNo WIFI available
        return;
    }
    else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
    {
        // The device is an iPhone.
        [self showMessage:MARVELL_NO_NETWORK_IPHONE withTitle:@""];//\nNo WIFI available
        return;
    }
    else
    {
        // The device is an iPod.
        [self showMessage:MARVELL_NO_NETWORK_IPOD withTitle:@""];//\nNo WIFI available
        return;
    }
}

/* Action defined on provision click */
-(IBAction)OnProvisionClick:(id)sender
{
    [self.view endEditing:YES];
    TimerCount = 0;
    /* currentWifiSSID returns the network to which device is conncted */
    self.strCurrentSSID=[self currentWifiSSID];
    
    NSLog(@"self.strCurrentSSID %@",self.strCurrentSSID);
    NSLog(@"self.security %d",self.security);
    
    if([[txtNetworkName.text Trim] isEqual:@""])
    {
        [self showMessage:MARVELL_CHOOSE_NETWORK];
        return;
    }
    else if([[txtPassword.text Trim] isEqual:@""])
    {
        if (self.security != 0)
        {
            [self showMessage:MARVELL_CHOOSE_PASSOWORD];
            return;
        }
    }
    else if ([AppDelegate isWiFiReachable] == NO)
    {
        [self showDeviceNotConnected];
    }
    else  if (([self.strCurrentSSID rangeOfString:MARVELL_NETWORK_NAME].location == NSNotFound) || !self.strCurrentSSID)
    {
        [self showDeviceNotConnected];
    }
    else if (self.security != SECURITY_TYPE_DEFAULT)
    {
        if (self.security == SECURITY_TYPE_WPA || self.security == SECURITY_TYPE_WPA2|| self.security == SECURITY_TYPE_WPA_WPA2)
        {
            if ([txtPassword.text Trim].length < 8)
            {
                [self showMessage:MARVELL_WPA_MINIMUM];
                return;
            }
        }
    }
    
    if (Mode != -1)
    {
        if ([AppDelegate isWiFiReachable])
        {
            Mode = 0;
            [self ShowProgressView:@"Checking connection..."];
            //[self setBusy:YES forMessage:@"Checking connection..."];
            [self isMarvellDevice];
        }
        else
        {
            [self showDeviceNotConnected];
        }
    }
    else
    {
        if ([AppDelegate isWiFiReachable])
        {
            [self ConfigureNetwork];
        }
        else
        {
            [self showDeviceNotConnected];
        }
    }
 }

/* Action performed on button click Choose Network */
- (IBAction)ChooseNetwork:(id)sender
{
    [self.view endEditing:YES];
    isOpened = !isOpened;
    /* scan device is called when choose network list expands */
    if(isOpened)
    {
        _imgarrow.transform = CGAffineTransformMakeRotation(degreesToRadians(180));
        [self scanDevices];
        [self HidePopUp:NO animated:YES];
        return;
    }
    else
    {
        _imgarrow.transform = CGAffineTransformMakeRotation(degreesToRadians(0));
        [self HidePopUp:YES animated:YES];
    }
}

/* Action invoked on clicking unmask passphrase */
- (IBAction)UnmaskPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = !btnSender.selected;
    // Get the selected text range
    if (btnSender.selected)
    {
        txtPassword.secureTextEntry = NO;
        /* to adjust the cursor position */
        txtPassword.text = txtPassword.text;
    }
    else
    {
        txtPassword.secureTextEntry = YES;
    }
}

/* Action invoked on clicking remember passphrase */
- (IBAction)RememberPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = !btnSender.selected;
    if (btnSender.selected) {
        NSString *keyValue = SelectedSSID;
        NSString *valueToSave = txtPassword.text;
        [[NSUserDefaults standardUserDefaults] setObject:valueToSave forKey:keyValue];
    } else {
        NSString *keyValue = SelectedSSID;
        NSString *valueToSave = @"NULL";
        [[NSUserDefaults standardUserDefaults] setObject:valueToSave forKey:keyValue];
    }
}

/* Virtual actions invoked by program to enable and disable remember passphrase*/
- (IBAction)EnableRememberPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = YES;
}

- (IBAction)DisableRememberPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = NO;
}

/* Function to configure UI */
-(void)HidePopUp:(BOOL)hide animated:(BOOL)animated
{
    if(animated)
    {
        [UIView beginAnimations:@"popup" context:nil];
        [UIView setAnimationBeginsFromCurrentState:YES];
        [UIView setAnimationDuration:0.3];
        
        /*popViewSecurity is a view */
        if (hide == YES)
        {
            /* Hide popViewSecurity */
            //self.popViewSecurity.hidden = YES;
        }
        else
        {
            /* Show popViewSecurity */
            self.popViewSecurity.hidden = NO;
        }

        /* If popViewSecurity:hide is YES then view height is set to 0
         * to hide popSecurityView, else set to its specified height
         */
        CGRect frame = self.popViewSecurity.frame;
        frame.size.height = hide ? 0 : 150;
        self.popViewSecurity.frame = frame;
        
        /* Configuring viewBottom view's position depending on
         * popViewSecurity's height
         */
        frame = self.viewBottom.frame;
        frame.origin.y = self.popViewSecurity.frame.origin.y + self.popViewSecurity.frame.size.height ;
        self.viewBottom.frame = frame;
        
        /* Configuring provision button's position*/
        if (self.viewPassphrase.hidden == YES)
        {
            frame = self.imgViewPassword.frame;
            self.btnProvision.frame = frame;
        }
        else
        {
            frame = self.imgViewPassword.frame;
            frame.origin.y = self.imgViewPassword.frame.origin.y+self.imgViewPassword.frame.size.height+50;//15
            self.btnProvision.frame = frame;
        }
        
        [UIView commitAnimations];
    }
    else{
        if (hide == YES)
        {
            //self.popViewSecurity.hidden = YES;
        }
        else
        {
            self.popViewSecurity.hidden = NO;
        }
        
        
        CGRect frame = self.popViewSecurity.frame;
        frame.size.height = hide ? 0 : 150;
        self.popViewSecurity.frame = frame;
        
        frame = self.viewBottom.frame;
        frame.origin.y = self.popViewSecurity.frame.origin.y + self.popViewSecurity.frame.size.height ;
        self.viewBottom.frame = frame;
        
        if (isOpen == YES && self.viewPassphrase.hidden == YES)
        {
            frame = self.imgViewPassword.frame;
            self.btnProvision.frame = frame;
        }
        else
        {
            frame = self.imgViewPassword.frame;
            frame.origin.y = self.imgViewPassword.frame.origin.y+self.imgViewPassword.frame.size.height+50;
            self.btnProvision.frame = frame;
            
        }
    }
}

#pragma mark - TapGesture Recognizer

/* Function to configure network, using POST /sys/network
 * Note: Called on provision click button
 */
-(void)ConfigureNetwork
{
    if ([AppDelegate isWiFiReachable]==YES)
    {
        [self ShowProgressView:@"Configuring network..."];
        NSString *requestString = [NSString stringWithFormat:@"{\"ssid\":\"%@\",\
                                   \"security\":%d,\
                                   \"ip\":%d,\
                                   \"channel\":%d,\
                                   \"key\":\"%@\"\
                                   }"
                                   ,
                                   SelectedSSID,
                                   self.security,
                                   1,
                                   self.channel,
                                   txtPassword.text?:@""];
        
        NSLog(@"REQUESTED STRING %@",requestString);
        ServerRequest *serverRequest = [[ServerRequest alloc] initWithJSONMethod:@"network" params:requestString :@"POST"];
        serverRequest.delegate = self;
        [serverRequest startAsynchronous];
    }
    else
    {
        [self showDeviceNotConnected];
    }
}

/* Function called to handle validations */
-(void)ValidateFields
{
    self.strCurrentSSID=[self currentWifiSSID];
    
    NSLog(@"self.strCurrentSSID %@",self.strCurrentSSID);
    NSLog(@"self.security %d",self.security);
    
    if([[txtNetworkName.text Trim] isEqual:@""])
    {
        [self showMessage:MARVELL_CHOOSE_NETWORK];
        return;
    }
    else if([[txtPassword.text Trim] isEqual:@""])
    {
        if (self.security != 0)
        {
            [self showMessage:MARVELL_CHOOSE_PASSOWORD];
            return;
        }
        else
        {
            
        }

    }
    else if (self.security == 6)
    {
        [self showMessage:MARVELL_INVALID_SECURITY];
        return;
    }
    else if ([AppDelegate isWiFiReachable] == NO)
    {
        [self showDeviceNotConnected];
    }
    else  if (([self.strCurrentSSID rangeOfString:MARVELL_NETWORK_NAME].location == NSNotFound) || !self.strCurrentSSID)
    {
        [self showDeviceNotConnected];
    }
    else if (self.security != SECURITY_TYPE_DEFAULT)
    {
        if (self.security == SECURITY_TYPE_WPA || self.security == SECURITY_TYPE_WPA2|| self.security == SECURITY_TYPE_WPA_WPA2)
        {
            if ([txtPassword.text Trim].length<8)
            {
                [self showMessage:MARVELL_WPA_MINIMUM];
                return;
            }
        }
               
    }
[self ConfigureNetwork];

}

/* Function to check if the device is connected to Marvell Network */
-(void)isMarvellDevice
{
    if ([AppDelegate isWiFiReachable]==YES)
    {
        
        ServerRequest *serverRequest = [[ServerRequest alloc]initWithJSONMethod:@"" params:nil :@"GET"];
        serverRequest.delegate = self;
        [serverRequest startAsynchronous];
    }
    else
    {
        [self showDeviceNotConnected];
    }
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    NSLog(@"Connection failed! Error - %@", [error localizedDescription]);
}

/* Function called on didServerFinishLoading event
 * after POST /sys/network, sends GET /sys
 * to get system status
 */
-(void)startProvisioning
{
    TimerCount++;
    if (TimerCount > 3)
    {
        NSLog(@"Invalidating timer");
        [self stopProgressView];
        if ([timer isValid] && [timer isKindOfClass:[NSTimer class]])
        {
            [timer invalidate];
        }
        [self showAlert :4];
    }

    /* GET /sys request */
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                    [NSURL URLWithString:LOCAL_URL]];
    [request setHTTPMethod:@"GET"];
    [request setTimeoutInterval:15.0];

    NSLog(@"requested url %@",request);

    NSError *error = nil;

    NSData *responseData = [NSURLConnection sendSynchronousRequest:request  returningResponse:nil   error:&error];

    if (error)
    {
        NSLog(@"error %@",error);
    }

    if(!error && [responseData length] > 0)
    {
        NSString *responseString = [[NSString alloc] initWithData:responseData
                                                         encoding:NSUTF8StringEncoding];
        [lblStatus setText:responseString];
        
        NSDictionary *JSONValue =  [responseString JSONValue];

        NSLog(@"startProvisioning::Output:%@",JSONValue);
        
        int configured = [[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"configured"] intValue];
        int Status =[[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"status"] intValue];
        NSString *bssid = [[[JSONValue objectForKey:@"connection"] objectForKey:@"uap"] objectForKey:@"bssid"];
        NSString *strError =[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"failure"];
        
        /* configured
         * 0 -> device not configured
         * 1 -> device is configured
         * Status
         * 0 -> not connected
         * 1 -> connecting
         * 2 -> connected
         */
        if(configured == 1 && Status== 2)
        {
            if ([timer isValid] && [timer isKindOfClass:[NSTimer class]])
                [timer invalidate];
            AppDelegate *appDelegate = [[UIApplication sharedApplication]delegate];
            appDelegate.strNetworkName =self.strCurrentSSID ;
            SetisProvisioned(YES);
            [self ClientAck];
            NSLog(@"startProvisioning::Configured successfully");
        }
        else if (![GetBSSID isEqualToString:bssid])
        {
            [self stopProgressView];
            [self showMessage:MARVELL_BSSIDNOT_MATCH];//the chhosed @"The device has been successfully configured with provided settings. However this iPhone has lost the connectivity with the device. Please check the indicators on the device to get the current status"
            NSLog(@"Disable provision button");
            self.btnProvision.enabled = NO;
            txtPassword.text = @"";
            [self.btnNetwork setTitle:@"Choose Network" forState:UIControlStateNormal];
            self.btnNetwork.enabled = NO;
            CGRect frame = self.popViewSecurity.frame;
            frame.size.height = 0;
            self.popViewSecurity.frame = frame;
            self.viewPassphrase.hidden = YES;
            frame = self.imgViewPassword.frame;
            self.btnProvision.frame = frame;
            return;
        }
        else if(strError)
        {
            [self stopProgressView];
            if ([timer isKindOfClass:[NSTimer class]])
                [timer invalidate];
            
            NSString *strError1 =[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"failure"];
            if ([strError1 isEqualToString:MARVELL_AUTH_FAILED])
            {
                [self showAlert :1];
            }
            else if ([strError1 isEqualToString:MARVELL_NW_NOT_FOUND])
            {
                [self showAlert :2];
            }
            else if ([strError1 isEqualToString:MARVELL_DHCP_FAILED])
            {
                [self showAlert :3];
            }
            else if ([strError1 isEqualToString:@"other"])
            {
                [self showAlert :4];
            }
        }
        else if (TimerCount < 5)
        {
            if (configured == 1 && Status == 1)
            {
                [self UpdateMessage:@"Device is trying to connect..."];
            }
            return;
        }
        else if (((configured==1 && Status == 1) || (configured == 0 && Status == 0)) & !strError)
        {
            [self showAlert:5];
        }
    }
    else
    {
        [self stopProgressView];
        if ([timer isKindOfClass:[NSTimer class]])
            [timer invalidate];
        NSLog(@"startProvisioning::Error:%@", [error localizedDescription]);
        if (error.code == NSURLErrorTimedOut)
        {
            /*if (TimerCount <= 3)
            {
                [self performSelector:@selector(ContinueAttempts) withObject:nil
                           afterDelay:10.0];
            }*/

            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_TIMEOUT delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];//@"The device has been successfully configured with provided settings. However this iPhone has lost the connectivity with the device. Please check the indicators on the device to get the current status"
            NSLog(@"Disable provision button");
            self.btnProvision.enabled = NO;
            txtPassword.text = @""; 
            [self.btnNetwork setTitle:@"Choose Network" forState:UIControlStateNormal];
            self.btnNetwork.enabled = NO;
            CGRect frame = self.popViewSecurity.frame;
            frame.size.height = 0;
            self.popViewSecurity.frame = frame;
            self.viewPassphrase.hidden = YES;
            frame = self.imgViewPassword.frame;
            self.btnProvision.frame = frame;
            
            [alertVw show];
            [alertVw release];
        }
        else
        {
            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_TIMEOUT
                                               delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];//@"Could not confirm if the device successfully connected to the network"
            NSLog(@"Disable provision button");
            self.btnProvision.enabled = NO;
            txtPassword.text = @"";
            [self.btnNetwork setTitle:@"Choose Network" forState:UIControlStateNormal];
            self.btnNetwork.enabled = NO;
            CGRect frame = self.popViewSecurity.frame;
            frame.size.height = 0;
            self.popViewSecurity.frame = frame;
            self.viewPassphrase.hidden = YES;
            frame = self.imgViewPassword.frame;
            self.btnProvision.frame = frame;
            [alertVw show];
            [alertVw release];
        }
    }
    //[self stopProgressView];
}

-(void)ContinueAttempts
{

    NSLog(@"Continue Attempts..");
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                    [NSURL URLWithString:LOCAL_URL]];
    [request setHTTPMethod:@"GET"];
    [request setTimeoutInterval:15.0];

    NSLog(@"requested url %@",request);

    NSError *error = nil;

    NSData *responseData = [NSURLConnection sendSynchronousRequest:request returningResponse:nil
                                                             error:&error];

    if(!error && [responseData length] > 0)
    {
        NSString *responseString = [[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding];
        [lblStatus setText:responseString];

        NSDictionary *JSONValue =  [responseString JSONValue];

        NSLog(@"startProvisioning::Output:%@",JSONValue);

        int configured = [[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"configured"] intValue];
        int Status =[[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"status"] intValue];
        NSString *bssid = [[[JSONValue objectForKey:@"connection"] objectForKey:@"uap"] objectForKey:@"bssid"];


        if(configured == 1 && Status== 2)
        {
            if ([timer isKindOfClass:[NSTimer class]])
                [timer invalidate];
            AppDelegate *appDelegate = [[UIApplication sharedApplication]delegate];
            appDelegate.strNetworkName =self.strCurrentSSID ;
            SetisProvisioned(YES);
            [self ClientAck];
            NSLog(@"startProvisioning::Configured successfully");
        }
        else if (![GetBSSID isEqualToString:bssid])
        {
            [self stopProgressView];
            [self showMessage:MARVELL_BSSIDNOT_MATCH];//@"The device has been configured with provided settings. However this iPhone has lost the connectivity with the device. Please reconnect to the device or home network and try to reload this application. Please check status indicators on the device to check connectivity"
            return;
        }
        else
        {
            [self stopProgressView];
            NSString *strError =[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"failure"];
            if ([strError isEqualToString:MARVELL_AUTH_FAILED])
            {
                [self showAlert :1];
            }
            else if ([strError isEqualToString:MARVELL_NW_NOT_FOUND])
            {
                [self showAlert :2];
            }
            else if ([strError isEqualToString:MARVELL_DHCP_FAILED])
            {
                [self showAlert :3];
            }
            else if ([strError isEqualToString:@"other"])
            {
                [self showAlert :4];
            }
            else if (((configured==1 && Status == 1) || (configured == 0 && Status == 0)) && !strError)
            {
                [self showAlert:5];
            }
            else
            {
                [self showAlert :4];
            }
        }
    }
    else
    {
        [self stopProgressView];
        NSLog(@"startProvisioning::Error:%@", [error localizedDescription]);
        if (error.code == NSURLErrorTimedOut)
        {

            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_BSSIDNOT_MATCH delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];
            [alertVw show];
            [alertVw release];
        }
        else
        {
            alertVw = [[UIAlertView alloc]initWithTitle:@"" message:MARVELL_TIMEOUT delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];//@"Could not confirm if the device is successfully connected to the network"
            [alertVw show];
            [alertVw release];
        }
        
    }
    //[self stopProgressView];
}

-(void)ScanForDevices
{
    NSLog(@"scan");
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                    [NSURL URLWithString:URL_SCAN]];
    [request setHTTPMethod:@"GET"];
    
    NSError *error = nil;
    
    NSData *responseData = [NSURLConnection sendSynchronousRequest:request returningResponse:nil
                                                             error:&error];
    if (error)
    {
        NSLog(@"error %@",error);
    }
    
    if(!error && [responseData length] > 0)
    {
        NSString *responseString = [[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding];
        [lblStatus setText:responseString];
        
        NSDictionary *JSONValue =  [responseString JSONValue];
        NSArray *arrRecords = [JSONValue objectForKey:@"networks"];
        if (arrRecords)
        {
            
            NSMutableArray *arrResults = [[NSMutableArray alloc]init];
            for (int aindex =0 ; aindex<[arrRecords count];aindex++)
            {
                NSString *str = [arrRecords objectAtIndex:aindex];
                NetworkDetails *networkDetails = [[NetworkDetails alloc]init];
                NSArray *myString = [str componentsSeparatedByString:@","];
                networkDetails.ssid = [myString objectAtIndex:0];
                networkDetails.bssid = [myString objectAtIndex:1];
                networkDetails.security= [[myString objectAtIndex:2] intValue];
                networkDetails.channel = [[myString objectAtIndex:3] intValue];
                networkDetails.rssi = [[myString objectAtIndex:4] intValue];
                networkDetails.nf = [[myString objectAtIndex:5] intValue];
                [arrResults addObject:networkDetails];
                [networkDetails release];
            }
            NSLog(@"arr records count %d",[arrRecords count]);
        }
        
        NSLog(@"scan:Output:%@",JSONValue);
    }
}

-(void)showAlert :(int)ErrorCode
{
    NSString *messsage = nil;// Reset to provisioning or continue attempts?
    NSString *title = nil;
    //  NSString *reason= nil;
    
    switch (ErrorCode)
    {
        case 1:
            messsage = MARVELL_RST_To_PROV;
            title = MARVELL_AUTH_FAILED_MSG;

            break;
        case 2:
            title = MARVELL_NETWORK_NOT_FOUND_MSG;
            messsage = MARVELL_RST_To_PROV;
            break;
        case 3:
            messsage = MARVELL_RST_To_PROV;
            title = MARVELL_DHCP_FAILED_MSG;
            break;
        case 4:
            title = MARVELL_NETWORK_NOT_FOUND_MSG;
            messsage = MARVELL_RST_To_PROV;

            break;
        case 5:
            messsage =@"The device is trying to connect";
            break;
        default:
            messsage = @"Unknown Error";
            break;
    }
    alertVw = [[UIAlertView alloc]initWithTitle:title?:@"" message:[NSString stringWithFormat:@"%@ %@\nReset to provisioning or continue attempts?",MARVELL_RST_To_PROV,title]  delegate:self cancelButtonTitle:nil otherButtonTitles:nil, nil];
    UILabel *theTitle = [alertVw valueForKey:@"_bodyTextLabel"];
    theTitle.textAlignment = UITextAlignmentLeft;

    [alertVw addButtonWithTitle:@"Reset"];
    [alertVw addButtonWithTitle:@"Continue"];
    alertVw.tag = 10;
    [alertVw show];
    [alertVw release];
}

-(void)GetAck
{
    NSLog(@"GETACK");
    [self ShowProgressView:@"Reset-to-provisioning..."];
    //[self setBusy:YES forMessage:@"Reset-to-provisioning..."];
    
    NSString *str = [NSString stringWithFormat:@"{\"connection\":{\"station\": {\"configured\":%@,}}}",@"0"];//\"prov\":{\"client_ack\":%@,},
    
    
    NSLog(@"Requested string %@",str);
    
    ServerRequest *serverRequest = [[ServerRequest alloc]init];
    [serverRequest initWithJSONMethod:@"" params:str :@"POST"];
    serverRequest.delegate = self;
    [serverRequest startAsynchronous];
    
}

-(void)ClientAck
{
    [self ShowProgressView:@"Acknowleding the device..."];
    Mode = 3;
    NSString *str = [NSString stringWithFormat:@"{\"prov\":{\"client_ack\":%@ }}",@"1"];//\"prov\":{\"client_ack\":%@,},
    
    
    NSLog(@"Requested string %@",str);
    
    ServerRequest *serverRequest = [[ServerRequest alloc]init];
    [serverRequest initWithJSONMethod:@"" params:str :@"POST"];
    serverRequest.delegate = self;
    [serverRequest startAsynchronous];
}
-(void)removeFields
{
    self.kScrollView.hidden = YES;
}

#pragma mark ServerRequest Delegate

/* Function called on server request completion */
-(void)serverRequestDidFinishLoading:(ServerRequest *)server
{
    [self stopProgressView];
    //[self setBusy:NO];
    if([server.method isEqual:@"network"])
    {
        NSDictionary *JSONValue =  [server.responseString JSONValue];
        NSString *strValue = [JSONValue objectForKey:@"success"];
        
        if(strValue)
        {
            [self ShowProgressView:@"Confirming network configuration..."];
            timer=  [NSTimer scheduledTimerWithTimeInterval:5.0 target:self selector:           @selector(startProvisioning) userInfo:nil repeats:YES];
        }
        else
        {
            strValue = [JSONValue objectForKey:@"error"];
            if (strValue)
            {
                [self showMessage:MARVELL_INCORRECT_DATA];
            }
            else
            {
                [self showMessage:MARVELL_INCORRECT_DATA];
            }
            
        }
        [lblStatus setText:server.responseString];
    }
    else if ([server.method isEqualToString:@""])
    {
        if ([server.RMethod isEqualToString:@"GET"])
        {
            NSDictionary *JSONValue =  [server.responseString JSONValue];
            lblStatus.text = server.responseString;
            NSLog(@"startProvisioning::Output:%@",JSONValue);
            
            int configured = [[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"configured"] intValue];
            int Status =[[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"status"] intValue];
            
            
            if (Mode==-1)
            {
                SetBSSID([[[JSONValue objectForKey:@"connection"] objectForKey:@"uap"] objectForKey:@"bssid"]);
                NSLog(@"bssid %@",GetBSSID);
            }
            if (configured == 1 && Status == 2)
            {
                [self showMessage:MARVELL_PROVISIONED withTitle:@""];//@"Device is already provisioned Please connect to another device"
                ScanViewController *scanViewController = [[ScanViewController alloc]initWithNibName:@"ScanView" bundle:Nil];
                [self.navigationController pushViewController:scanViewController animated:YES];
            }
            else if (configured == 0)
            {
                if (Mode ==0)
                {
                    [self ConfigureNetwork];
                }
                else if(Mode != -1)
                {
                    txtPassword.text = @"";
                    sleep(3);
                    CGRect frame = self.popViewSecurity.frame;
                    frame.size.height = 0;
                    self.popViewSecurity.frame = frame;
                    self.viewPassphrase.hidden = YES;
                    frame = self.imgViewPassword.frame;
                    self.btnProvision.frame = frame;
                    
                    [self.btnNetwork setTitle:@"Choose Network" forState:UIControlStateNormal];
                    [self showMessage:MARVELL_RESET];//@"Reset to provisioning successful"
                    self.btnProvision.enabled = NO;
                    TimerCount=0;
                }
                
            }
            else if ((configured == 1 && Status == 1) || (configured == 1 && Status == 0))
            {
                //Device is already configured\n Do you want to proceed?
                alertVw = [[UIAlertView alloc]initWithTitle:@"" message:@"Device is trying to connect \n Do you want to Reset to provisioning or continue attempts?" delegate:self cancelButtonTitle:nil otherButtonTitles:nil, nil];
                [alertVw addButtonWithTitle:@"Reset"];
                [alertVw addButtonWithTitle:@"Continue"];
                alertVw.tag = 10;
                [alertVw show];
                [alertVw release];
                //[self showMessage:@"Device is already configured"];
            }
            else
            {
                NSString *status = [[JSONValue objectForKey:@"prov"] objectForKey:@"types"] ;
                NSLog(@"prov->types  status:%@",status);
                
                if([status isEqualToString:@"no \"marvell\""])
                {
                    [self showMessage:@"Not a valid device, try WPS"];
                }
                else
                {
                    [self showMessage:@"Not a valid device"];
                }
            }
        }
        else
        {
            NSDictionary *JSONValue =  [server.responseString JSONValue];
            NSString *strValue = [JSONValue objectForKey:@"success"];
            if (Mode != 3)
            {
                if (strValue)
                {
                    [self ShowProgressView:@"Confirming network configuration.."];
                    //[self setBusy:YES forMessage:@"Confirming network configuration..."];
                    Mode = 1;
                    [self performSelector:@selector(isMarvellDevice) withObject:nil afterDelay:5.0];
                }
            }
            else
            {
                [self stopProgressView];
                // [self setBusy:NO forMessage:@""];
                TimerCount=0;
                [self showMessage:[NSString stringWithFormat:@"%@%@",MARVELL_SUCESS_PROV,txtNetworkName.text?:@""] withTag:30 withTarget:self];
            }
        }
    }
    [server release];
}

-(void)serverRequest:(ServerRequest *)server didFailWithError:(NSError *)error{
    
    [server release];
    [self stopProgressView];
    //[self setBusy:NO];
    NSLog(@"localizedDescription%@", [error localizedDescription]);
    NSLog(@"error code %d",error.code);
    if (error.code == ASIRequestTimedOutErrorType)
    {
        if ([server.method isEqualToString:@"network"])
        {
            [self showMessage:MARVELL_NET_NOTFOUND];
        }
        else
        {
            [self showMessage:MARVELL_WPS];
        }
    }
    else
    {
        [self showMessage:MARVELL_NET_NOTFOUND];
        
    }
    NSLog(@"error: %@", [error localizedDescription]);
    [self stopProgressView];
    //[self setBusy:NO];
}


- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark get Current WiFi Name

-(NSString *)currentWifiSSID
{
    
    NSLog(@"Hi");
    // Does not work on the simulator.
    NSString *ssid = nil;
    NSString *securityType;
    NSArray *ifs = (id)CNCopySupportedInterfaces();
    for (NSString *ifnam in ifs) {
        NSDictionary *info = (id)CNCopyCurrentNetworkInfo((CFStringRef)ifnam);
        if (info[@"SSID"])
        {
            ssid = info[@"SSID"];
            
            // To identify the security Type
            
            id wep = [info objectForKey:@"WEP"];
            id wpa = [info objectForKey:@"WPA_IE"];
            id rsn = [info objectForKey:@"RSN_IE"];
            
            if(wep)
            {
                securityType =@"wep";
                //ret =@"Secured network (WEP)";
            }
            else if (wpa)
            {
                securityType =@"wpa";
                // ret =@"Secured network (WPA)";
            }
            else if (rsn) {
                securityType =@"wpa2";
                // ret =@"Secured network (WPA2)";
            }
            else
            {
                securityType =@"open";
                //ret =@"Open Network";
            }
            
            NSLog(@"security Type %@",securityType);
            NSLog(@"ssid %@",ssid);
        }
    }
    return ssid;
}

#pragma mark checkForWIFIConnection

- (BOOL)checkWifiConnection
{
    Reachability* wifiReach = [Reachability reachabilityForLocalWiFi];
    
    NetworkStatus netStatus = [wifiReach currentReachabilityStatus];
    
    if (netStatus != ReachableViaWiFi)
    {
        isChecked = NO;
        if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
        {
            // The device is an iPad running iOS 3.2 or later.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPAD
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }
        else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
        {
            // The device is an iPhone or iPod touch.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPHONE
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }
        else
        {
            // The device is an iPhone or iPod touch.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPOD
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }
        
        [alertVw setTag:60];
        [alertVw show];
        
        return NO;
    }
    else
    {
        self.strCurrentSSID = [self currentWifiSSID];
        if (([self.strCurrentSSID rangeOfString:MARVELL_NETWORK_NAME].location == NSNotFound) || !self.strCurrentSSID)
        {
            if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
            {
                // The device is an iPad running iOS 3.2 or later.
                [self showMessage:MARVELL_NO_NETWORK_IPAD withTag:70 withTarget:self];
                
                return NO;
            }
            else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
            {
                // The device is an iPhone or iPod touch.
                [self showMessage:MARVELL_NO_NETWORK_IPHONE withTag:70 withTarget:self];
                
                return NO;
            }
            else
            {
                // The device is an iPhone or iPod touch.
                [self showMessage:MARVELL_NO_NETWORK_IPOD withTag:70 withTarget:self];
                
                return NO;
            }
        }
        else
        {

            [self ShowProgressView:@"Scanning Network Devices.."];
            [self isMarvellDevice];
        }
    }
    [self performSelector:@selector(scanDevices) withObject:Nil afterDelay:1.0];
    return YES;
}

-(BOOL)checkWifi
{
    NSLog(@"Hey App is active again");
    Mode = -1;

    if (self.btnNetwork.enabled == NO)
    {
        self.btnNetwork.enabled = YES;
    }

    Reachability* wifiReach = [Reachability reachabilityForLocalWiFi];
    
    NetworkStatus netStatus = [wifiReach currentReachabilityStatus];
    
    if (netStatus != ReachableViaWiFi)
    {
        if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
        {
            // The device is an iPad running iOS 3.2 or later.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPAD
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }
        else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
        {
            // The device is an iPhone or iPod touch.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPHONE
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }
        else
        {
            // The device is an iPhone or iPod touch.
            alertVw = [[UIAlertView alloc] initWithTitle:@""
                                                 message:MARVELL_NO_NETWORK_IPOD
                                                delegate:self
                                       cancelButtonTitle:@"OK"
                                       otherButtonTitles:nil, nil];//No WIFI available!
        }

        [alertVw show];
        return NO;
    }
    else
    {
        /* This else part is executed on resuming the app */
        //[self startProvisioning];
        self.strCurrentSSID = [self currentWifiSSID];
        if (([self.strCurrentSSID rangeOfString:MARVELL_NETWORK_NAME].location == NSNotFound) || !self.strCurrentSSID)
        {
            if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
            {
                // The device is an iPad running iOS 3.2 or later.
                [self showMessage:MARVELL_NO_NETWORK_IPAD withTag:50 withTarget:self];//\nNo WIFI available
                return NO;
            }
            else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
            {
                // The device is an iPhone or iPod touch.
                [self showMessage:MARVELL_NO_NETWORK_IPHONE withTag:50 withTarget:self];//\nNo WIFI available
                return NO;
            }
            else
            {
                // The device is an iPhone or iPod touch.
                [self showMessage:MARVELL_NO_NETWORK_IPOD withTag:50 withTarget:self];//\nNo WIFI available
                return NO;
            }
        }
        else
        {
            if (!alertVw)
            {
                [self isMarvellDevice];
            }
            self.view.userInteractionEnabled = YES;
            return YES;
        }
    }
}

/* alertView */
#pragma mark alertView Delegate

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    if (alertView.tag ==10 && buttonIndex==0)
    {
        [self GetAck];
    }
    else if (alertView.tag == 10 && buttonIndex ==1)
    {
        [self ShowProgressView:@"Confirming network configuration..."];
        //[self setBusy:YES forMessage:@"Confirming network configuration..."];
        [self performSelector:@selector(ContinueAttempts) withObject:nil afterDelay:1.0];
    }
    else if (alertView.tag == 20 && buttonIndex == 0)
    {
        [self ValidateFields];
        //  [self ConfigureNetwork];
    }
    else if(alertView.tag == 30 && buttonIndex ==0)
    {
        ScanViewController *scanViewController = [[ScanViewController alloc]initWithNibName:@"ScanView" bundle:nil];
        [self.navigationController pushViewController:scanViewController animated:YES];
    }
    else if (alertView.tag == 50 || alertView.tag == 60 || alertView.tag == 70)
    {
        self.view.userInteractionEnabled = NO;
    }
}

/* TextField */
#pragma mark TextField delegate

- (void)textFieldDidBeginEditing:(UITextField *)textField
{
    [self HidePopUp:YES animated:YES];
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    NSUInteger newLength = [textField.text length] + [string length] - range.length;
    
    if (textField == txtNetworkName)
    {
        NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:ALPHANUMERICWITHSPACE] invertedSet];
        NSString *filtered = [[string componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
        if(self.security != SECURITY_TYPE_DEFAULT)
        {
            [self ValidatePassword];
        }
        
        return [string isEqualToString:filtered] && (newLength < 75) ;
    }
    else
    {
        if (self.security == SECURITY_TYPE_DEFAULT)
        {
            [self showMessage:@"Please choose security type before entering the passphrase"];
            return NO;
            
        }
        else
        {
            NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:ALPHANUMERIC] invertedSet];
            NSString *filtered = [[string componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
            
            if (self.security ==SECURITY_TYPE_OPEN)
            {
                //return NO;
            }
            else if (self.security == SECURITY_TYPE_WEP)
            {
                if (textField == txtPassword)
                {
                    if(newLength == 5 || newLength == 10 || newLength == 13 || newLength == 26)
                    {
                        if(newLength == 5 || newLength == 13) //ASCII
                        {
                            NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:ASCII] invertedSet];
                            NSString *filtered = [[string componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
                            BOOL status = [string isEqualToString:filtered];
                            
                            if(status)
                                [self.btnProvision setAlpha:1];
                            else
                                [self.btnProvision setAlpha:.5];
                            
                            [self.btnProvision setEnabled:status];
                            
                        }
                        else if(newLength == 10 || newLength == 26) // HEX
                        {
                            NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:HEX] invertedSet];
                            NSString *filtered = [[string componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
                            BOOL status = [string isEqualToString:filtered];
                            
                            if(status)
                                [self.btnProvision setAlpha:1];
                            else
                                [self.btnProvision setAlpha:.5];
                            
                            [self.btnProvision setEnabled:status];
                            
                        }
                        else
                        {
                            [self.btnProvision setEnabled:NO];
                            [self.btnProvision setAlpha:.5];
                        }
                    }
                    else
                    {
                        [self.btnProvision setEnabled:NO];
                        [self.btnProvision setAlpha:.5];
                    }
                }

            }
            else if (self.security == SECURITY_TYPE_WPA || self.security == SECURITY_TYPE_WPA2 || self.security == SECURITY_TYPE_WPA_WPA2)
            {
                if (newLength >= 8)
                {
                    [self.btnProvision setEnabled:YES];
                    [self.btnProvision setAlpha:1];
                }
                else
                {
                    [self.btnProvision setEnabled:NO];
                    [self.btnProvision setAlpha:5];
                }
            }
            else
            {
                return [string isEqualToString:filtered] && (newLength < 64) ;
            }
            
            
        }
    }
    
    /* Remember password is unchecked if original password is modified */
    if (textField == txtPassword) {
        [_btnRememberPassword sendActionsForControlEvents:UIControlEventTouchDragOutside];
    }
    return TRUE;
}



-(BOOL) textFieldShouldReturn:(UITextField *)textField
{
    if (textField == txtNetworkName)
    {
        [txtPassword becomeFirstResponder];
    }
    else
    {
        [textField resignFirstResponder];
        [self.view endEditing:YES];
        [self HidePopUp:YES animated:YES];
        
        self.strCurrentSSID=[self currentWifiSSID];
        
        NSLog(@"self.strCurrentSSID %@",self.strCurrentSSID);
        NSLog(@"self.security %d",self.security);
        
        if([[txtNetworkName.text Trim] isEqual:@""])
        {
            [self showMessage:MARVELL_CHOOSE_NETWORK];//@"Please enter the network name !!!"
            return NO;
        }
        else if([[txtPassword.text Trim] isEqual:@""])
        {
            if (self.security != 0)
            {
                [self showMessage:MARVELL_CHOOSE_PASSOWORD];
                return NO;
            }
        }
        else if (self.security >= SECURITY_TYPE_DEFAULT)
        {
            [self showMessage:MARVELL_INVALID_SECURITY];
            return NO;
        }
        else if ([AppDelegate isWiFiReachable] == NO)
        {
            [self showDeviceNotConnected];
        }
        else if (self.security != SECURITY_TYPE_DEFAULT)
        {
            if (self.security == SECURITY_TYPE_WPA || self.security == SECURITY_TYPE_WPA2 || self.security == SECURITY_TYPE_WPA_WPA2)
            {
                if ([txtPassword.text Trim].length<8)
                {
                    [self showMessage:MARVELL_WPA_MINIMUM];
                    return NO;
                }
            }
            /*
             else if (self.security == 1)
             {
             if (([txtPassword.text Trim].length != 5) || ([txtPassword.text Trim].length != 5) || ([txtPassword.text Trim].length != 5) || ([txtPassword.text Trim].length != 5))
             {
             
             [self showMessage:@"Passphrase should be 5/13 ASCII characters or 10/26 hexa digits"];
             return;
             }
             }*/
        }
        
        if (Mode != -1)
        {
            if ([AppDelegate isWiFiReachable])
            {
                Mode = 0;
                [self ShowProgressView:@"Checking connection..."];
                //[self setBusy:YES forMessage:@"Checking connection..."];
                [self isMarvellDevice];
            }
            else
            {
                [self showDeviceNotConnected];
            }
        }
        else
        {
            if ([AppDelegate isWiFiReachable])
            {
                if (self.btnProvision.enabled)
                {
                    [self ConfigureNetwork];
                }
                
            }
            else
            {
                [self showDeviceNotConnected];
            }

        }
    }
    
    [self HidePopUp:YES animated:YES];
    
    return YES;
}

/* Function used to enable and disable provision button depending on
 * length of password entered
 */
-(void)ValidatePassword
{
    if (self.security == SECURITY_TYPE_WEP)
    {
        int newLength = txtPassword.text.length;
        if(newLength == 5 || newLength == 10 || newLength == 13 || newLength == 26)
        {
            if(newLength == 5 || newLength == 13) //ASCII
            {
                NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:ASCII] invertedSet];
                NSString *filtered = [[txtPassword.text componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
                BOOL status = [txtPassword.text isEqualToString:filtered];
                
                if(status)
                    [self.btnProvision setAlpha:1];
                else
                    [self.btnProvision setAlpha:.5];
                
                [self.btnProvision setEnabled:status];
                
            }
            else if(newLength == 10 || newLength == 26) // HEX
            {
                NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:HEX] invertedSet];
                NSString *filtered = [[txtPassword.text componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
                BOOL status = [txtPassword.text isEqualToString:filtered];
                
                if(status)
                    [self.btnProvision setAlpha:1];
                else
                    [self.btnProvision setAlpha:.5];
                
                [self.btnProvision setEnabled:status];
                
            }
            else
            {
                [self.btnProvision setEnabled:NO];
                [self.btnProvision setAlpha:.5];
            }
        }
        else
        {
            [self.btnProvision setEnabled:NO];
            [self.btnProvision setAlpha:.5];
        }
    }
    else if (self.security == SECURITY_TYPE_WPA || self.security == SECURITY_TYPE_WPA2 || self.security == SECURITY_TYPE_WPA_WPA2)
    {

        if (txtPassword.text.length >= 8)
        {
            [self.btnProvision setEnabled:YES];
            [self.btnProvision setAlpha:1];
        }
        else
        {
            [self.btnProvision setEnabled:NO];
            [self.btnProvision setAlpha:5];
        }
    }
}

/* TableView */
#pragma mark - TableView delegate & datasource

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [networkList count];
}


- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString* cellIdentifier = @"SecurityType";
    UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
    if(cell == nil)
    {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];
        //cell.textLabel.font = [UIFont fontWithName:@"Helvetica" size:14.0f];
        cell.selectionStyle = UITableViewCellSelectionStyleGray;
        cell.textLabel.font = [UIFont fontWithName:@"HelveticaNeue" size:15.0];
        cell.textLabel.textColor = [UIColor grayColor];
    }

    cell.textLabel.text = [networkList objectAtIndex:indexPath.row];
    return cell;
}

/* Action performed when an entry in selected in tableview */
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    self.security = [networkSecurity[indexPath.row] intValue];
    /* As per wifi certification requirements, WPA only security is no more
       supported */
    if (self.security == SECURITY_TYPE_WPA) {
	    self.security = SECURITY_TYPE_WPA_WPA2;
    }
    self.channel = [networkChannel[indexPath.row] intValue];
    _imgarrow.transform = CGAffineTransformMakeRotation(degreesToRadians(0));
    
    NSLog(@"Selected Network %@", networkList[indexPath.row]);
    NSLog(@"Selected Network's Security %d", self.security);
    NSLog(@"Selected Network's Channel %d", self.channel);
    SelectedSSID = networkList[indexPath.row];
    
    if (self.security == 0)
    {
        self.btnProvision.enabled = YES;
        self.viewPassphrase.hidden = YES;
        isOpen = YES;
    }
    else
    {
        self.btnProvision.enabled = NO;
        self.viewPassphrase.hidden = NO;
        isOpen = NO;
    }

    /* To hide and show passphrase */
    if (self.security != 0)
    {
        txtPassword.hidden = NO;
        self.viewPassphrase.hidden = NO;
        self.lblPassphrase.hidden = NO;
        self.imgViewPassword.hidden = NO;
        
        if (![self.btnNetwork.titleLabel.text isEqualToString:[networkList objectAtIndex:indexPath.row]])
        {
            txtPassword.text = @"";
        }

        NSString *savedValue = [[NSUserDefaults standardUserDefaults] stringForKey:networkList[indexPath.row]];
        if ([savedValue isEqualToString:@"NULL"] || savedValue == NULL) {
            [_btnRememberPassword sendActionsForControlEvents:UIControlEventTouchDragOutside];
        } else {
            [_btnRememberPassword sendActionsForControlEvents:UIControlEventTouchDragInside];
            txtPassword.text = savedValue;
            self.btnProvision.enabled = YES;
        }

        CGRect frame = self.imgViewPassword.frame;
        frame.origin.y = self.imgViewPassword.frame.origin.y+self.imgViewPassword.frame.size.height+50;
        self.btnProvision.frame = frame;
    }
    
    [self.btnNetwork setTitle:[networkList objectAtIndex:indexPath.row] forState:UIControlStateNormal];
    isOpened = NO;
    [self HidePopUp:YES animated:YES];
}

/* Progress View */
#pragma mark - Progress View Methods

/* Function to show loading image with a message */
-(void)ShowProgressView :(NSString*)msg
{
    progress.view.frame = self.view.bounds;
    [self.view addSubview:progress.view];
    [progress startAnimation:msg];
    //[progress release];
}

/* Function to stop loading image */
-(void)stopProgressView
{
    // ProgressViewController *progress = [[ProgressViewController alloc]init];
    [progress stopAnimation];
    [progress.view removeFromSuperview];

    //[progress release];
}
-(void)UpdateMessage :(NSString*)msg
{
    [progress UpdateMessage:msg];
}

- (void)dealloc {
    [txtNetworkName release];
    [txtPassword release];
    [lblStatus release];
    [_popViewSecurity release];
    [_btnNetwork release];
    [_btnProvision release];
    
    [_kScrollView release];
    [_lblPassphrase release];
    
    [_imgViewNetwork release];
    [_imgViewPassword release];
    [_imgarrow release];
    [_lblNetwork release];
    [_viewBottom release];

    [_viewPassphrase release];
    [_imgViewDDown release];
    [super dealloc];
}
- (void)viewDidUnload {
    [self setKScrollView:nil];
    [self setLblPassphrase:nil];
    [self setImgViewNetwork:nil];
    [self setImgViewPassword:nil];
    [self setImgarrow:nil];
    [self setLblNetwork:nil];
    [self setViewBottom:nil];
    [self setViewPassphrase:nil];
    [self setViewPassphrase:nil];
    [self setImgViewDDown:nil];
    [super viewDidUnload];
}
@end
