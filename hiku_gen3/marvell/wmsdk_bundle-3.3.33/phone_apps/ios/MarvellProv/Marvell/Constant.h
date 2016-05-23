// App Information
#define APP_ID                  564561456445
#define AppName                 [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"]
#define AppVersion              [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"]
#define AppDelegate(type)       ((type *)[[UIApplication sharedApplication] delegate])
#define NSAppDelegate(type)     ((type *)[[NSApplication sharedApplication] delegate])
#define SharedApp               [UIApplication sharedApplication]
#define NSSharedApp             [NSApplication sharedApplication]
#define Bundle                  [NSBundle mainBundle]
#define MainScreen              [UIScreen minScreen]

// OS Information
#define IOS_6_0_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"6.0" options:NSNumericSearch] != NSOrderedAscending
#define IOS_7_0_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"7.0" options:NSNumericSearch] != NSOrderedAscending
#define IOS_5_1_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"5.1" options:NSNumericSearch] != NSOrderedAscending
#define IOS_5_0_1_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"5.0.1" options:NSNumericSearch] != NSOrderedAscending
#define IOS_5_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"5.0" options:NSNumericSearch] != NSOrderedAscending
#define IOS_4_OR_LATER [[[UIDevice currentDevice] systemVersion] compare:@"4.0" options:NSNumericSearch] != NSOrderedAscending
#define ONLY_IF_AT_LEAST_IOS_4(action) if ([[[UIDevice currentDevice] systemVersion] compare:@"4.0" options:NSNumericSearch] != NSOrderedAscending) { action; }

#define LANDSCAPE_ORENTATION    (interfaceOrientation == UIInterfaceOrientationLandscapeLeft) || (interfaceOrientation == UIInterfaceOrientationLandscapeRight)

#define ALPHANUMERIC @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.@-!#$%^*()"
#define ALPHANUMERICWITHSPACE @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.@-!#$%^*() "

#define NETWORK_NAME @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_ "
#define NUMERIC @"-+()0123456789"
#define ALPHA @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define NUMBERS @"0123456789"
#define ALPHANUM @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

#define ASCII @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define HEX @"ABCDEFabcdef0123456789"


#define WEBSERVICE_URL @"http://192.168.10.1/sys/"
#define LOCAL_URL @"http://192.168.10.1/sys/"
#define URL_SCAN @"http://192.168.10.1/sys/scan"
#define Marvell_URL @"http://192.168.10.1/"



// Directories
static inline NSString *CachesDirectory() {
	return [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}
static inline NSString *LibraryDirectory() {
	return [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}
static inline NSString *DocumentsDirectory() {
	return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}

// Actions
#define OpenURL(urlString) [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]]

// Preferences
#define UDValue(x)              [[NSUserDefaults standardUserDefaults] valueForKey:(x)]
#define UDBool(x)               [[NSUserDefaults standardUserDefaults] boolForKey:(x)]
#define UDInteger(x)            [[NSUserDefaults standardUserDefaults] integerForKey:(x)]
#define UDSetValue(x, y)        [[NSUserDefaults standardUserDefaults] setValue:(y) forKey:(x)]
#define UDSetBool(x, y)         [[NSUserDefaults standardUserDefaults] setBool:(y) forKey:(x)]
#define UDSetInteger(x, y)      [[NSUserDefaults standardUserDefaults] setInteger:(y) forKey:(x)]
#define UDObserveValue(x, y)    [[NSUserDefaults standardUserDefaults] addObserver:y forKeyPath:x options:NSKeyValueObservingOptionOld context:nil];
#define UDSync(ignored)         [[NSUserDefaults standardUserDefaults] synchronize]


// Debugging
#define StartTimer(ignored)     NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
#define EndTimer(msg)           NSTimeInterval stop = [NSDate timeIntervalSinceReferenceDate]; NSLog(@"%@", [NSString stringWithFormat:@"%@ Time = %f", msg, stop-start]);


/* key, observer, object */
#define ObserveValue(x, y, z) [(z) addObserver:y forKeyPath:x options:NSKeyValueObservingOptionOld context:nil];



// User Interface Related
#define HexColor(c)                         [UIColor colorWithRed:((c>>24)&0xFF)/255.0 green:((c>>16)&0xFF)/255.0 blue:((c>>8)&0xFF)/255.0 alpha:((c)&0xFF)/255.0]
#define RGB(r, g, b)                        [UIColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0]
#define RGBA(r, g, b, a)                    [UIColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:a]
#define NSHexColor(c)                       [NSColor colorWithRed:((c>>24)&0xFF)/255.0 green:((c>>16)&0xFF)/255.0 blue:((c>>8)&0xFF)/255.0 alpha:((c)&0xFF)/255.0]
#define NSRGB(r, g, b)                      [NSColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0]
#define NSRGBA(r, g, b, a)                  [NSColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:a]

#define TextFont    [UIFont fontWithName:@"HelveticaNeue-Bold" size:16]
#define ShowNetworkActivityIndicator()      [UIApplication sharedApplication].networkActivityIndicatorVisible = YES
#define HideNetworkActivityIndicator()      [UIApplication sharedApplication].networkActivityIndicatorVisible = NO
#define SetNetworkActivityIndicator(x)      [UIApplication sharedApplication].networkActivityIndicatorVisible = x
#define NavBar                              self.navigationController.navigationBar
#define TabBar                              self.tabBarController.tabBar
#define NavBarHeight                        self.navigationController.navigationBar.bounds.size.height
#define TabBarHeight                        self.tabBarController.tabBar.bounds.size.height
#define ScreenWidth                         [[UIScreen mainScreen] bounds].size.width
#define ScreenHeight                        [[UIScreen mainScreen] bounds].size.height
#define TouchHeightDefault                  44
#define TouchHeightSmall                    32
#define ViewWidth(v)                        v.frame.size.width
#define ViewHeight(v)                       v.frame.size.height
#define ViewX(v)                            v.frame.origin.x
#define ViewY(v)                            v.frame.origin.y



// Rect stuff
#define CGWidth(rect)                   rect.size.width
#define CGHeight(rect)                  rect.size.height
#define CGOriginX(rect)                 rect.origin.x
#define CGOriginY(rect)                 rect.origin.y
#define CGRectCenter(rect)              CGPointMake(NSOriginX(rect) + NSWidth(rect) / 2, NSOriginY(rect) + NSHeight(rect) / 2)
#define CGRectModify(rect,dx,dy,dw,dh)  CGRectMake(rect.origin.x + dx, rect.origin.y + dy, rect.size.width + dw, rect.size.height + dh)
#define NSLogRect(rect)                 NSLog(@"%@", NSStringFromCGRect(rect))
#define NSLogSize(size)                 NSLog(@"%@", NSStringFromCGSize(size))
#define NSLogPoint(point)               NSLog(@"%@", NSStringFromCGPoint(point))

#define TitleFont                           [UIFont boldSystemFontOfSize:18.0]
#define SearchBarFont                       [UIFont boldSystemFontOfSize:14.0]

#define NSImage(x)                          [UIImage imageNamed:x]
#define StretchableImage(x)                 [NSImage(x) stretchableImageWithLeftCapWidth:12 topCapHeight:12]

#define WIDTH_IPHONE_5 568
#define IS_IPHONE   ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
#define IS_IPHONE_5 ([[UIScreen mainScreen] bounds ].size.height == WIDTH_IPHONE_5 )

#define    SetisProvisioned(x)            [[NSUserDefaults standardUserDefaults] setBool:(x) forKey:@"isProvisioned"]
#define    GetisProvisioned               [[NSUserDefaults standardUserDefaults] boolForKey:@"isProvisioned"]

#define    SetBSSID(x)      [[NSUserDefaults standardUserDefaults] setObject:(x) forKey:@"BSSID"]
#define    GetBSSID         [[NSUserDefaults standardUserDefaults] objectForKey:@"BSSID"]



#import "AppDelegate.h"
