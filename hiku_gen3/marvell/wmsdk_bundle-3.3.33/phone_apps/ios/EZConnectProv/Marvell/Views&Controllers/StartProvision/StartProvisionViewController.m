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

#import "ServerRequestDelegate.h"

#import "AppDelegate.h"

#import "MessageList.h"

#import "errno.h"

#import "arpa/inet.h"

#import "zlib.h"

#import "CommonCrypto/CommonCryptor.h"

#import "CommonCrypto/CommonKeyDerivation.h"

@interface StartProvisionViewController ()
{
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
    if (IOS_7_0_OR_LATER) {
        return YES;
    }
    return NO;
}

int alertFlag = 0;

/* This function is called when app is loaded on the device */
- (void)viewDidLoad
{
    self.security = 7;
    self.state = 0;
    self.substate = 0;
    Mode = -1;

    if (alertFlag == 0) {
        UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"EZConnect" message:@"For internal use only. Do not distribute this app.\r\n Copyright (c) 2016 Marvell." delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:nil, nil];
        [alert show];
        [alert release];
        alertFlag = 1;
    }

    
//    [self Secure];
    if (self.security == 0) {
        [self HidePopUp:YES animated:NO];
    } else {
        [self HidePopUp:NO animated:NO];
    }

    [super viewDidLoad];
    
    txtNetworkName.enabled = NO;
    self.imgViewDDown.layer.cornerRadius = 5.0;
    status.text = MARVELL_APP_STARTED;
    
    TimerCount = 0;

    // Do any additional setup after loading the view from its nib.
    isChecked = YES;
    [self performSelector:@selector(checkWifiConnection) withObject:nil afterDelay:0.1];
}


Byte rand_number[64];
int len;
NSString *rand_string;

-(void) Secure
{
    int i;
//    rand_string = [[NSString alloc]];
    for (i = 0; i < 64; i++) {
        NSLog(@"Value of number is %d", rand());
        rand_number[i] = floor(((float)rand() / RAND_MAX * 256));
        NSString *rand_string = [[NSString alloc] initWithFormat:@"%x", rand_number[i]];
        NSString *masking = [[NSString alloc] initWithFormat:@"%x", 0];
        len = (int)rand_string.length;
        NSLog(@"Length is %d %@\r\n", len, rand_string);
        if (len % 2 != 0) {
            strcat(masking, rand_string);
        }
        [rand_string release];
    }
}


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

/* Action invoked when provision button is clicked */
-(IBAction)OnProvisionClick:(id)sender
{
    [self.view endEditing:YES];
    
    if (!invalidKey && !invalidPassphrase) {
        status.text = @"";
    } else if (invalidKey) {
        status.text = INVALID_KEY_LENGTH;
        return;
    } else {
        status.text = INVALID_PASSPHRASE_LENGTH;
        return;
    }
    
    if ([AppDelegate isWiFiReachable]) {
        [self xmitterTask];
    } else {
        [self showMessage:MARVELL_NO_NETWORK withTitle:@""];
        return;
    }
}

/* To mask and unmask the passphrase */
- (IBAction)UnmaskPassword:(id)sender
{
    NSString *tmpString;
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = !btnSender.selected;
    if (btnSender.selected) {
        txtPassword.secureTextEntry = NO;
        /* to adjust the cursor position */
        tmpString = txtPassword.text;
        txtPassword.text = @" ";
        txtPassword.text = tmpString;
    } else {
        txtPassword.secureTextEntry = YES;
        /* to adjust the cursor position */
        tmpString = txtPassword.text;
        txtPassword.text = @" ";
        txtPassword.text = tmpString;
    }
}

/* Action invoked on clicking remember passphrase */
- (IBAction)RememberPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = !btnSender.selected;
    if (btnSender.selected) {
        NSString *keyValue = txtNetworkName.text;
        NSString *valueToSave = txtPassword.text;
        [[NSUserDefaults standardUserDefaults] setObject:valueToSave forKey:keyValue];
    } else {
        NSString *keyValue = txtNetworkName.text;
        NSString *valueToSave = @"NULL";
        [[NSUserDefaults standardUserDefaults] setObject:valueToSave forKey:keyValue];
    }
}

/* Virtual action invoked by program to disable remember passphrase*/
- (IBAction)DisableRememberPassword:(id)sender
{
    UIButton *btnSender = (UIButton*)sender;
    btnSender.selected = NO;
}


/* To set the attribute values of the UI widgets */
-(void)HidePopUp:(BOOL)hide animated:(BOOL)animated
{
    if(animated) {
        [UIView beginAnimations:@"popup" context:nil];
        [UIView setAnimationBeginsFromCurrentState:YES];
        [UIView setAnimationDuration:0.3];

        if (hide)
            self.viewPassphrase.hidden = YES;
        else
            self.viewPassphrase.hidden = NO;

        CGRect frame = self.viewPassphrase.frame;
        frame.size.height = hide ? 0 : 110;
        self.viewPassphrase.frame = frame;
        
        [UIView commitAnimations];
    } else {
        
        if (hide)
            self.viewPassphrase.hidden = YES;
        else
            self.viewPassphrase.hidden = NO;

        CGRect frame = self.viewPassphrase.frame;
        frame.size.height = hide ? 0 : 110;
        self.viewPassphrase.frame = frame;

        frame = self.viewBottom.frame;
        frame.origin.y = self.viewPassphrase.frame.origin.y + self.viewPassphrase.frame.size.height ;
        self.viewBottom.frame = frame;
    }
}

#pragma mark - TapGesture Recognizer
-(void) xmitRaw:(int) u data:(int) m substate:(int) l
{
    int sock;
    struct sockaddr_in addr;
    char buf = 'a';

    NSMutableString* getnamebyaddr = [NSMutableString stringWithFormat:@"239.%d.%d.%d", u, m, l];
    const char * d_addr = [getnamebyaddr UTF8String];
    NSLog(@"String %@", getnamebyaddr);
    
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    	NSLog(@"ERROR: broadcastMessage - socket() failed");
    	return;
    }
    
    bzero((char *)&addr, sizeof(struct sockaddr_in));
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(d_addr);
    addr.sin_port        = htons(10000);
    
    if ((sendto(sock, &buf, sizeof(buf), 0, (struct sockaddr *) &addr, sizeof(addr))) != 1) {
        NSLog(@"Errno %d", errno);
        NSLog(@"ERROR: broadcastMessage - sendto() sent incorrect number of bytes");
        close(sock);
        return;
    }
    
    close(sock);
}

-(void) myEncrypt : (char [])key passPhrase:(char [])secret
{
    char dataOut[80];// set it acc ur data
    unsigned char derivedSecret[32];
    bzero(dataOut, sizeof(dataOut));
    bzero(derivedSecret, sizeof(derivedSecret));
    size_t numBytesEncrypted = 0;

    int res = CCKeyDerivationPBKDF(kCCPBKDF2, key, strlen(key), (uint8_t *)ssid, ssidLength,
                                      kCCPRFHmacAlgSHA1, 4096, derivedSecret, sizeof(derivedSecret));
    NSLog(@"Derived key result is %d\r\n", res);

    CCCryptorStatus result = CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding, derivedSecret, kCCKeySizeAES256, nil, secret, passLen, dataOut, sizeof(dataOut), &numBytesEncrypted);

    memcpy(passphrase, dataOut, passLen);
}


-(void)xmitState0:(int)substate
{
    int i, j, k;

    k = preamble[2  * substate];
    j = preamble[2 * substate + 1];
    i = substate | 0x78;
    [self xmitRaw:i data: j substate: k];
}

-(void)xmitState1:(int)substate LengthSSID:(int)len
{
    if (substate == 0) {
        int u = 0x40;
        [self xmitRaw:u data:ssidLength substate: ssidLength];
    } else if (substate == 1 || substate == 2) {
        int k = (int) (ssidCRC >> ((2 * (substate - 1) + 0) * 8)) & 0xff;
        int j = (int) (ssidCRC >> ((2 * (substate - 1) + 1) * 8)) & 0xff;
        int i = substate | 0x40;
        [self xmitRaw:i data: j substate: k];
    } else {
        int u = 0x40 | substate;
        int l = (0xff & ssid[(2 * (substate - 3))]);
        int m;
        if (len == 2)
            m = (0xff & ssid[(2 * (substate - 3) + 1)]);
        else
            m = 0;
        [self xmitRaw:u data:m substate:l];
    }
}

-(void)xmitState2: (int)substate LengthPassphrase:(int)len
{
    if (substate == 0) {
        int u = 0x00;
        [self xmitRaw:u data:passLen substate: passLen];
    } else if (substate == 1 || substate == 2) {
        int k = (int) (passCRC >> ((2 * (substate - 1) + 0) * 8)) & 0xff;
        int j = (int) (passCRC >> ((2 * (substate - 1) + 1) * 8)) & 0xff;
        int i = substate;
        [self xmitRaw:i data: j substate: k];
    } else {
        int u = substate;
        int l = (0xff & passphrase[(2 * (substate - 3))]);
        int m;
        if (len == 2)
            m = (0xff & passphrase[(2 * (substate - 3)) + 1]);
        else
            m = 0;
        [self xmitRaw:u data:m substate:l];
    }
}

-(void)xmitState3: (int)substate
{
    int i, j, k;
    
    k = (int) (passCRC >> ((2 * _substate + 0) * 8)) & 0xff;
    j = (int) (passCRC >> ((2 * _substate + 1) * 8)) & 0xff;
    i = substate | 0x7c;
    
    [self xmitRaw:i data: j substate: k];
}

- (id)init
{
    NSLog(@"Initializing browser");
    self = [super init];
    if (self) {
        self.services = [NSMutableArray arrayWithCapacity: 0];
    }
    return self;
}

int flag = 1;
- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
           didFindService:(NSNetService *)aNetService
               moreComing:(BOOL)moreComing
{
    [self.services addObject:aNetService];
    const char *serviceType = [[aNetService type] cStringUsingEncoding:NSUTF8StringEncoding];
    NSLog(@"Got service %p with serviceType %s %@ %@\n", aNetService,
          serviceType, [aNetService name], [aNetService type]);
    if (flag && !strncasecmp(serviceType, "_ezconnect-prov._tcp.", sizeof("_ezconnect-prov._tcp."))) {
        NSString *text = [NSString stringWithFormat:@"Device %@ is successfully provisioned to Home Network", [aNetService name]];
        UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Device Status" message:text delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:@"Cancel", nil];
        [alert show];
        status.text = text;
        if (timerMdns.isValid && [timerMdns isKindOfClass:[NSTimer class]]) {
            [timerMdns invalidate];
            timerMdns = nil;
        }
        flag = 0;
        [text release];
    }
    
    if(!moreComing)
    {;
        NSLog(@"More coming");
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
             didNotSearch:(NSDictionary *)errorDict
{
    NSLog(@"Failed searching");
}

-(void)queryMdnsService
{
    NSNetServiceBrowser *serviceBrowser;
    
    serviceBrowser = [[NSNetServiceBrowser alloc] init];
    [serviceBrowser setDelegate:self];
    [serviceBrowser searchForServicesOfType:@"_ezconnect-prov._tcp" inDomain:@"local"];
}

int timerCount;

-(void) mdnsFound
{
    timerCount--;
    if (!timerCount) {
        status.text = MARVELL_SWITCH_PROV_MODE;
        [timerMdns invalidate];
        timerMdns = nil;
    }
}

-(void)statemachine
{
//    NSLog(@"statemachine");
    NSString *temp;
    if (_state == 0 && _substate == 0) {
        TimerCount++;
        if (TimerCount % 10 == 0) {
            temp = [NSString stringWithFormat:@"Information sent %d times.", TimerCount];
            status.text = temp;
        }
    }
    if (TimerCount >= 300) {
        [self queryMdnsService];
        NSLog(@"Browsing services");

        status.text = MARVELL_INFO_SENT;
        timerCount = 15;
        timerMdns =  [NSTimer scheduledTimerWithTimeInterval:1 target:self selector: @selector(mdnsFound) userInfo:nil repeats:YES];
        
        if ([timer isValid] && [timer isKindOfClass:[NSTimer class]]) {
            [timer invalidate];
            timer = nil;
        }
        _state = 0;
        _substate = 0;
        TimerCount = 0;
        [_btnProvision setTitle:@"START" forState:UIControlStateNormal];
        flag = 1;
    }
    
    switch(_state) {
        case 0:
            if (_substate == 3) {
                _state = 1;
                _substate = 0;
            } else {
                [self xmitState0:_substate];
                _substate++;
            }
            break;
        case 1:

            [self xmitState1:_substate LengthSSID:2];
            _substate++;
            if (ssidLength % 2 == 1) {
                if (_substate * 2 == ssidLength + 5) {
                    [self xmitState1:_substate LengthSSID: 1];
                    _state = 2;
                    _substate = 0;
                }
            } else {
                if ((_substate - 1) * 2 == (ssidLength + 4)) {
                    _state = 2;
                    _substate = 0;
                }
            }
            break;
        case 2:
            [self xmitState2:_substate LengthPassphrase:2];

            _substate++;
            if (passLen % 2 == 1) {
                if (_substate * 2 == passLen + 5) {
                    [self xmitState2:_substate LengthPassphrase: 1];
                    _state = 0;
                    _substate = 0;
                }
            } else {
                if ((_substate - 1) * 2 == (passLen + 4)) {
                    _state = 0;
                    _substate = 0;
                }
            }
            break;

            default:
            NSLog(@"MRVL: I should not be here!");
        }
}

-(void)xmitterTask
{
    const char *temp = [txtPassword.text UTF8String];
    strcpy(passphrase, temp);
    passLength = (int)txtPassword.text.length;
    passLen = passLength;
    unsigned char *str = (unsigned char *)temp;
    unsigned char *str1 = (unsigned char *)ssid;

    passCRC = crc32(0, str, passLen);
    ssidCRC = crc32(0, str1, ssidLength);
    
    NSLog(@"CRC is %lu", passCRC);
    
    passCRC = passCRC & 0xffffffff;
    ssidCRC = ssidCRC & 0xffffffff;
    
    NSLog(@"Passphrase %d %d %s", passLen, passLength, passphrase);
    NSLog(@"CRC32 %lu", passCRC);
    
    if ([txtDeviceKey.text length] != 0) {
        char plainpass[100];
        bzero(plainpass, 100);
        char key[100];
        bzero(key, 100);
        if (passLen % 16 != 0) {
            int i;
            passLen = (16 - passLen%16) + passLen;
            for (i = 0; i < passLength; i++)
                plainpass[i] = passphrase[i];
            for (; i < passLen; i++)
                plainpass[i] = 0x00;
            strcpy(key,[txtDeviceKey.text UTF8String]);
            for (i = strlen(key); i < 100; i++)
                key[i] = 0x00;
        }
        [self myEncrypt: key passPhrase: plainpass];
    }

    if ([[[_btnProvision titleLabel] text] isEqualToString:@"START"]) {
        [_btnProvision setTitle:@"STOP" forState:UIControlStateNormal];
        NSLog(@"TEXT is %@", _btnProvision.titleLabel.text);
        timer=  [NSTimer scheduledTimerWithTimeInterval:0.001 target:self selector:           @selector(statemachine) userInfo:nil repeats:YES];
    } else {
        if ([timer isValid] && [timer isKindOfClass:[NSTimer class]]) {
            [timer invalidate];
            timer = nil;
        }
        _state = 0;
        _substate = 0;
        [_btnProvision setTitle:@"START" forState:UIControlStateNormal];
        flag = 1;
    }
}

#pragma mark get Current WiFi Name

-(NSString *)currentWifiSSID
{
    // Does not work on the simulator.
    NSString *ssid_str = nil;
    NSString *security_str = nil;

    NSString *bssid_str = nil;
    NSArray *testArray = [[NSArray alloc] init];
    NSArray *ifs = (id)CNCopySupportedInterfaces();
    NSString *temp;
    for (NSString *ifnam in ifs) {
        NSDictionary *info = (id)CNCopyCurrentNetworkInfo((CFStringRef)ifnam);
        NSLog(@"String %@", info);
        if (info[@"SSID"]) {
            ssid_str = info[@"SSID"];
            NSLog(@"ssid %@", ssid_str);
            for(int i = 0 ;i < [ssid_str length]; i++) {
                ssid[i] = [ssid_str characterAtIndex:i];
            }
            ssidLength = ssid_str.length;
            bssid_str = info[@"BSSID"];
            NSLog(@"bssid %@", bssid_str);
            testArray = [bssid_str componentsSeparatedByString:@":"];
            NSScanner *aScanner;
            unsigned int te;
            for (int i = 0; i < 6; i++) {
                temp = [testArray objectAtIndex:i];
                aScanner = [NSScanner scannerWithString:temp];
                [aScanner scanHexInt: &te];
                bssid[i] = te;
            }

            preamble[0] = 0x45;
            preamble[1] = 0x5a;
            preamble[2] = 0x50;
            preamble[3] = 0x52;
            preamble[4] = 0x32;
            preamble[5] = 0x32;
            
            
            txtNetworkName.text = ssid_str;
            NSString *savedValue = [[NSUserDefaults standardUserDefaults] stringForKey:ssid_str];
            if ([savedValue isEqualToString:@"NULL"]) {
                //do nothing
            } else {
                txtPassword.text = savedValue;
            }
            self.security = 7;
        }
    }
    return ssid_str;
}

#pragma mark checkForWIFIConnection
-(BOOL)checkWifi
{
    Reachability* wifiReach = [Reachability reachabilityForLocalWiFi];
    
    NetworkStatus netStatus = [wifiReach currentReachabilityStatus];
    
    if (netStatus != ReachableViaWiFi) {
        [self showMessage:MARVELL_NO_WIFI withTag:70 withTarget:self];
        return NO;
    } else {
        self.strCurrentSSID = [self currentWifiSSID];
        if ([[txtNetworkName.text Trim] isEqual:@""]) {
            isChecked = NO;
            [self showMessage:MARVELL_NO_WIFI withTag:70 withTarget:self];
            return NO;
        }
        txtPassword.hidden = NO;
        return YES;
    }
}


- (BOOL)checkWifiConnection
{
    Reachability* wifiReach = [Reachability reachabilityForLocalWiFi];
    
    NetworkStatus netStatus = [wifiReach currentReachabilityStatus];
    
    if (netStatus != ReachableViaWiFi) {
        isChecked = NO;
        [self showMessage:MARVELL_NO_WIFI withTag:70 withTarget:self];
        return NO;
    } else {
        self.strCurrentSSID = [self currentWifiSSID];
        if ([[txtNetworkName.text Trim] isEqual:@""]) {
            isChecked = NO;
            [self showMessage:MARVELL_NO_NETWORK withTag:70 withTarget:self];
            return NO;
        }
        txtPassword.hidden = NO;
    }
    return YES;
}

#pragma mark TextField delegate

- (void)textFieldDidBeginEditing:(UITextField *)textField
{
    if (textField == txtPassword) {
        txtPassword.placeholder = @"";
    }
    if (textField == txtDeviceKey) {
        txtDeviceKey.placeholder = @"";
    }
    if (self.security == 0)
        [self HidePopUp:YES animated:YES];
    else
        [self HidePopUp:NO animated:YES];
}

- (void)textFieldDidEndEditing:(UITextField *)textField {
    if (textField == txtPassword && [txtPassword.text isEqualToString:@""]) {
        txtPassword.placeholder = @"If required";
    }
    if (textField == txtDeviceKey && [txtDeviceKey.text isEqualToString:@""]) {
        txtDeviceKey.placeholder = @"If required";
    }

    if (textField == txtDeviceKey) {
        unsigned long deviceKeyLen = [textField.text length];
        if (deviceKeyLen >= 8 && deviceKeyLen <= 32) {
            invalidKey = 0;
            self.btnProvision.enabled = YES;
        } else {
            self.btnProvision.enabled = NO;
            invalidKey = 1;
        }
    }

    if ((!invalidKey && !invalidPassphrase) ||
        ([txtPassword.text isEqualToString:@""] && !invalidKey)) {
        status.text = @"";
        self.btnProvision.enabled = YES;
    } else if (invalidKey) {
        self.btnProvision.enabled = NO;
        status.text = INVALID_KEY_LENGTH;
    } else {
        self.btnProvision.enabled = NO;
        status.text = INVALID_PASSPHRASE_LENGTH;
    }
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    NSUInteger newLength = [textField.text length] + [string length] - range.length;
    
    if (textField == txtNetworkName) {
        NSCharacterSet *cs = [[NSCharacterSet characterSetWithCharactersInString:ALPHANUMERICWITHSPACE] invertedSet];
        NSString *filtered = [[string componentsSeparatedByCharactersInSet:cs] componentsJoinedByString:@""];
        
        return [string isEqualToString:filtered] && (newLength < 75) ;
    }
    if (textField == txtPassword) {
        if ((newLength >= 8 && newLength < 64) || (newLength == 0)) {
            self.btnProvision.enabled = YES;
            invalidPassphrase = 0;
        } else {
            self.btnProvision.enabled = NO;
            invalidPassphrase = 1;
        }
    }

    if (textField == txtDeviceKey) {
        if (newLength >= 8 && newLength <= 32) {
            invalidKey = 0;
            self.btnProvision.enabled = YES;
        } else {
            self.btnProvision.enabled = NO;
            invalidKey = 1;
        }
    }
    
    if ((!invalidKey) &&
        ([txtPassword.text isEqualToString:@""] || (!invalidPassphrase))) {
        status.text = @"";
        self.btnProvision.enabled = YES;
    } else if (invalidKey) {
        self.btnProvision.enabled = NO;
        status.text = INVALID_KEY_LENGTH;
    } else {
        self.btnProvision.enabled = NO;
        status.text = INVALID_PASSPHRASE_LENGTH;
    }
    
    /* Remember password is unchecked if original password is modified */
    if (textField == txtPassword) {
        [_btnRememberPassword sendActionsForControlEvents:UIControlEventTouchDragOutside];
    }

    return TRUE;
}



-(BOOL) textFieldShouldReturn:(UITextField *)textField
{
    if (textField == txtPassword && [txtPassword.text isEqualToString:@""]) {
        txtPassword.placeholder = @"If required";
    }
    if (textField == txtDeviceKey && [txtDeviceKey.text isEqualToString:@""]) {
        txtDeviceKey.placeholder = @"If required";
    }
    
    if (!invalidKey && !invalidPassphrase) {
        status.text = @"";
    } else if (invalidKey) {
            status.text = INVALID_KEY_LENGTH;
    } else if (invalidPassphrase){
        status.text = INVALID_PASSPHRASE_LENGTH;
    }

    if (textField == txtNetworkName) {
        [txtPassword becomeFirstResponder];
    } else {
        [textField resignFirstResponder];
        [self.view endEditing:YES];
        if (self.security == 0)
            [self HidePopUp:YES animated:YES];
        else
            [self HidePopUp:NO animated:YES];

        if ([AppDelegate isWiFiReachable] == NO)
        {
            [self showMessage:MARVELL_NO_NETWORK withTitle:@""];//\nNo WIFI available
            return NO;
        }
    }
    
    if (self.security == 0) {
        [self HidePopUp:YES animated:NO];
    } else {
        [self HidePopUp:NO animated:NO];
    }

    return YES;
}

#pragma mark ServerRequest Delegate

/* Function called on server request completion */
-(void)serverRequestDidFinishLoading:(ServerRequest *)server
{
    [server release];
}


-(void)serverRequest:(ServerRequest *)server didFailWithError:(NSError *)error{
    
    [server release];
}


- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    [txtNetworkName release];
    [txtPassword release];
    [lblStatus release];
    [_btnProvision release];
    
    [_kScrollView release];
    [_lblPassphrase release];
    
    [_imgViewNetwork release];
    [_imgViewPassword release];
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
    [self setLblNetwork:nil];
    [self setViewBottom:nil];
    [self setViewPassphrase:nil];
    [self setViewPassphrase:nil];
    [self setImgViewDDown:nil];
    [super viewDidUnload];
}
@end
