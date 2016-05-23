//
//  ScanViewController.m
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#import "ScanViewController.h"
#import <SystemConfiguration/CaptiveNetwork.h>
#import "Reachability.h"
#import "NetworkDetails.h"
#import "Constant.h"
#import "JSON.h"
#import "ServerRequest.h"
#import "ProgressViewController.h"
#import "StartProvisionViewController.h"
#import "MessageList.h"


@interface ScanViewController ()<ServerRequestDelegate>
{
    ProgressViewController *progress;
}

@end

AppDelegate *appDelegate;

@implementation ScanViewController
@synthesize strCurrentSSID;

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


- (void)viewDidLoad
{
    [super viewDidLoad];
   // self.lblNetworkName.text = [self currentWifiSSID];
    appDelegate = [[UIApplication sharedApplication]delegate];
    self.arrResults = [[NSMutableArray alloc]init];
    NetworkDetails *networkDetails = [[NetworkDetails alloc]init];
    networkDetails.ssid = appDelegate.strNetworkName;
    NSLog(@"appDelegate.strNetworkName %@",appDelegate.strNetworkName);
    [self.arrResults addObject:networkDetails];
    [networkDetails release];
    
    //[self ScanForDevices];
    //[self setBusy:YES forMessage:@"Provisioning..."];
    //[self GetAck];
    //[self ScanForDevices];
	// Do any additional setup after loading the view.
}
-(void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:YES];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(checkWifi)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];

}
-(void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:YES];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:UIApplicationWillEnterForegroundNotification object:nil];
}
-(BOOL)checkWifi
{
    Reachability* wifiReach = [Reachability reachabilityForLocalWiFi];
    
    NetworkStatus netStatus = [wifiReach currentReachabilityStatus];
    
    if (netStatus != ReachableViaWiFi)
    {
        if ([[UIDevice currentDevice].model rangeOfString:@"iPad"].location != NSNotFound)
        {

            UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@""
                                                            message:MARVELL_NO_NETWORK_IPAD
                                                           delegate:self
                                                  cancelButtonTitle:@"OK"
                                                  otherButtonTitles:nil, nil];
            [alertView show];
            return NO;
        }
        else if ([[UIDevice currentDevice].model rangeOfString:@"iPhone"].location != NSNotFound)
        {
            UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@""
                                                                message:MARVELL_NO_NETWORK_IPHONE
                                                               delegate:self
                                                      cancelButtonTitle:@"OK"
                                                      otherButtonTitles:nil, nil];
            [alertView show];
            return NO;
        }
        else
        {
            UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@""
                                                                message:MARVELL_NO_NETWORK_IPOD
                                                               delegate:self
                                                      cancelButtonTitle:@"OK"
                                                      otherButtonTitles:nil, nil];
            [alertView show];
            return NO;
        }
    }
    else
    {
        
        self.strCurrentSSID = [self currentWifiSSID];
        if (([self.strCurrentSSID rangeOfString:MARVELL_NETWORK_NAME].location == NSNotFound) || !self.strCurrentSSID)
        {
          //  [self showMessage:@"Please choose Marvell network from Settings of your iPhone" withTag:50 withTarget:self];
            StartProvisionViewController *startProvisioningViewController = [[StartProvisionViewController alloc]initWithNibName:@"StartProvisionViewController" bundle:nil];
            [self.navigationController pushViewController:startProvisioningViewController animated:YES];
           // return NO;
        }
        else
        {
            progress = [[ProgressViewController alloc]init];
            [self isMarvellDevice];
            self.view.userInteractionEnabled = YES;
            return YES;
        }
    }
    return YES;
}
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
        [self showMessage:@"Please check Wifi in your device"];
    }
}

-(NSString *)currentWifiSSID
{
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

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark - Scan For Networks

/*
-(void)ScanForDevices
{
    if ([AppDelegate isWiFiReachable]==YES)
    {

        [self setBusy:YES];
        ServerRequest *serverRequest = [[ServerRequest alloc]initWithJSONMethod:@"" params:nil :@"GET"];
        serverRequest.delegate = self;
        [serverRequest startAsynchronous];
    }
    else
    {
        [self showMessage:@"Please check Wifi in your device"];
    }
}
 */

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
        NSDictionary *JSONValue =  [responseString JSONValue];
        NSLog(@"scan:Output:%@",JSONValue);
        NSArray *arrRecords = [JSONValue objectForKey:@"networks"];
        if (arrRecords)
        {
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
                
                //add only wmdemo - devices
                if ([networkDetails.ssid rangeOfString:@"wmdemo"].location != NSNotFound)
                {
                  [self.arrResults addObject:networkDetails]; 
                }
                
                [networkDetails release];
            }
            NSLog(@"arr records count %d",[arrRecords count]);
            
            if ([self.arrResults count]>0)
            {
                self.lblTitle.text = @"Devices";
                self.btnStartProvision.hidden = NO;
            }
            else
            {
                self.lblTitle.text = @"Device";
                self.btnStartProvision.hidden = YES;
            }
        }
        NSLog(@"scan:Output:%@",JSONValue);
    }
    else
    {
        NSString *responseString = [[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding];
        NSDictionary *JSONValue =  [responseString JSONValue];
        NSLog(@"scan:Output:%@",JSONValue);
    }
    [self setBusy:NO];
    
}
 
-(void)GetAck
{
    [self setBusy:YES forMessage:@"Acknowledging..."];
//    NSMutableDictionary *act = [[NSMutableDictionary alloc] init];
//    [act setObject:@"1" forKey:@"client_ack"];
//
//    
//    NSMutableDictionary *config = [[NSMutableDictionary alloc] init];
//    [config setObject:@"0" forKey:@"configured"];
//    NSMutableDictionary *station = [[NSMutableDictionary alloc] init];
//    [station setObject:config forKey:@"station"];
////    NSMutableDictionary *connection = [[NSMutableDictionary alloc] init];
////    [connection setObject:station forKey:@"connection"];
//    NSMutableDictionary * request =[[NSMutableDictionary alloc] init];
//  
//    [request setObject:station forKey:@"connection"];
//    [request setObject:act forKey:@"prov"];
//    
//    NSLog(@"request %@",request);
    

    
    
    
    NSString *str = [NSString stringWithFormat:@"{\"prov\":{\"client_ack\":%@,},\"connection\":{\"station\": {\"configured\":%@,}}}",@"1",@"0"];
    
    
    NSLog(@"Requested string %@",str);

    ServerRequest *serverRequest = [[ServerRequest alloc]init];
    [serverRequest initWithJSONMethod:@"" params:str];
    serverRequest.delegate = self;
    [serverRequest startAsynchronous];
     
}
-(void)Provision
{
    
}

#pragma mark - UITableView DataSource and delegate Methods

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return 52.0;
}
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.arrResults count];
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString  *CellIdentifier = @"DevicesCell";
	
    DevicesCell * cell  = (DevicesCell *) [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    
    if(cell == nil)
    {
        cell =  [[[NSBundle mainBundle] loadNibNamed:@"DevicesCell" owner:self options:nil] objectAtIndex:0];
        cell = self.devicesCell;
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        
    }
    NetworkDetails *networkDetails = [self.arrResults objectAtIndex:indexPath.row];
    cell.lblSSID.text = networkDetails.ssid?:appDelegate.strNetworkName;
    cell.lblProvisionStatus.text = @"Provisioned successfully";

    return cell;
}

-(void)tableView:(UITableView*)tableView didDeselectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [self Provision];
}

- (void)dealloc
{
    [_tblView release];
    [_lblTitle release];
    [_btnStartProvision release];
    [_lblNetworkName release];
    [super dealloc];
}
- (IBAction)StartProvisioning:(id)sender
{
    [self GetAck];
}

#pragma mark - Server Request Delegate Methods
-(void)serverRequestDidFinishLoading:(ServerRequest *)server
{
    [self setBusy:NO];
    NSDictionary *JSONValue =  [server.responseString JSONValue];
    NSLog(@"startProvisioning::Output:%@",JSONValue);
    
    int configured = [[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"configured"] intValue];
    int Status =[[[[JSONValue objectForKey:@"connection"] objectForKey:@"station"] objectForKey:@"status"] intValue];
    if (configured == 1 && Status == 2)
    {
        [self showMessage:@"Device is already provisioned Please connect to another device" withTitle:@""];
      
    }
    else
    {
        StartProvisionViewController *startProvisioningViewController = [[StartProvisionViewController alloc]initWithNibName:@"StartProvisionViewController" bundle:nil];
        [self.navigationController pushViewController:startProvisioningViewController animated:YES];
    }
//    else if (configured == 1 && Status == 1)
//    {
//        UIAlertView *alertView = [[UIAlertView alloc]initWithTitle:@"" message:[NSString stringWithFormat:@"%@\n Do you want to Reset to provisioning or continue attempts?",@"Device is trying to connect"] delegate:self cancelButtonTitle:nil otherButtonTitles:nil, nil];
//        [alertView addButtonWithTitle:@"Reset"];
//        [alertView addButtonWithTitle:@"Continue"];
//        alertView.tag = 10;
//        [alertView show];
//        [alertView release];
//    }
    

}
-(void)serverRequest:(ServerRequest *)server didFailWithError:(NSError *)error{
    
    [server release];
    UIAlertView *alertView = [[UIAlertView alloc]initWithTitle:@"" message:@"Connection Failure" delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil, nil];
    [alertView show];
    [alertView release];
    NSLog(@"error: %@", [error localizedDescription]);
    
    [self setBusy:NO];
}

#pragma mark - UIAlertView Delegate Methods

-(void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    if (buttonIndex == 0 && alertView.tag == 10)
    {
        if (alertView.tag ==10 && buttonIndex==0)
        {
            [self GetAck];
        }
        else if (alertView.tag == 10 && buttonIndex ==1)
        {
            [self ShowProgressView:@"Confirming network configuration..."];
            [self performSelector:@selector(startProvisioning) withObject:nil afterDelay:15.0];
        }
    }
}

#pragma mark - Progress View Methods

-(void)ShowProgressView :(NSString*)msg
{
    progress.view.frame = self.view.bounds;
    [self.view addSubview:progress.view];
    [progress startAnimation:msg];
    //[progress release];
}
-(void)stopProgressView
{
    // ProgressViewController *progress = [[ProgressViewController alloc]init];
    [progress stopAnimation];
    [progress.view removeFromSuperview];
    
    //[progress release];
}
-(void)UpdateMessage :(NSString*)msg
{
    [progress startAnimation:msg];
}
@end
