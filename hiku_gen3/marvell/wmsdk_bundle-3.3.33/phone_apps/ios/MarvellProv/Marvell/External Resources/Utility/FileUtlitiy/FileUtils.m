//
//  FileUtils.m
//  Kaboom
//


#import "FileUtils.h"
#import <sys/xattr.h>
#import "Constant.h"

@implementation FileUtils

+(NSString *) documentsDirectoryPath {
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentsDirectory = [paths objectAtIndex:0];
	return documentsDirectory;
}

+(NSString *) documentsDirectoryPathForResource:(NSString *) fileName {
	NSString *documentsDirectoryPath = [FileUtils documentsDirectoryPath];
	NSString *finalPath = [documentsDirectoryPath stringByAppendingPathComponent:fileName];
	return finalPath;
}

+(BOOL) fileExistsAtPath:(NSString *) path fileName: (NSString *) fileName {
	NSString *combinedPath = [path stringByAppendingPathComponent:fileName];
	
	NSFileManager * fileManager = [NSFileManager defaultManager];
	BOOL exists = [fileManager fileExistsAtPath:combinedPath]; 
	return exists;
}

+(NSString *) nextSequentialFileName:(NSString *) fileName fileExtenstion:(NSString *) extenstion  directoryPath: (NSString *) path {
	NSFileManager * fileManager = [NSFileManager defaultManager];
	
	NSArray *contents;
	
#if MAC_OS_X_VERSION_10_5 <= MAC_OS_X_VERSION_MAX_ALLOWED || __IPHONE_2_0 <= __IPHONE_OS_VERSION_MAX_ALLOWED	
	contents =[fileManager contentsOfDirectoryAtPath:path error:nil];
#else
	contents =[fileManager directoryContentsAtPath:path];
#endif	
		
	if(contents) {
		NSString *formattedFileName = [[NSString alloc] initWithFormat:@"%@.%@", fileName, extenstion];
		int counter = 1;
		while([contents indexOfObject:formattedFileName] != NSNotFound) {
			NSString *newFileName = [[NSString alloc] initWithFormat:@"%@%d.%@", fileName, counter, extenstion];
			[formattedFileName release];
			formattedFileName = [newFileName retain];
			[newFileName release];			
			counter++;
		}
		//NSLog(@"Formated file name %@",formattedFileName );
		return [formattedFileName autorelease];
		
	} else {
		return nil;
	}		
}

+(BOOL) writeFileToDocuemntsDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion {
	NSString *documentsDirectoryPath = [FileUtils documentsDirectoryPath];
	NSString *nextSequentialFileName = [FileUtils nextSequentialFileName:prefix fileExtenstion:extenstion directoryPath:documentsDirectoryPath];
	//NSLog(@"nextSequentialFileName %@",nextSequentialFileName);
	NSString *fileNameWithExtension = [[NSString alloc] initWithString: nextSequentialFileName];
	NSString *finalFilePath = [documentsDirectoryPath stringByAppendingPathComponent:fileNameWithExtension];
	//NSLog(@"final path name %@",finalFilePath);
	BOOL success = [fileData writeToFile:finalFilePath atomically:YES];	
	[fileNameWithExtension release];
	return success;
}

+(void) copyResourceFileToDocumentsDirectory: (NSString *) fileName {
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *writablePath = [documentsDirectory stringByAppendingPathComponent:fileName];
	
	NSFileManager * fileManager = [NSFileManager defaultManager];
	BOOL succeeded = [fileManager fileExistsAtPath:writablePath]; 
	NSError *error;
	
	if(!succeeded) { //If file is not in the documents directory then only write
		NSString *newPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:fileName];
		//NSLog(@"newPath : %@ ", newPath);
		
		succeeded = [fileManager copyItemAtPath:newPath toPath:writablePath error:&error]; 
		
		if(succeeded == FALSE)
		{
			//NSLog(@"%@ : copy failed", fileName);
			
		} else {
			//NSLog(@"%@ : copy success", fileName);
		}
	} else {
		//NSLog(@"%@ : already exists", fileName);
	}
}

+(void) copyResourcesToDocumentsDirectory: (NSArray *) resources {
	int fileCount = [resources count];
	for(int i = 0; i < fileCount; i++) {
		NSString *filename = [resources objectAtIndex:i];
		[self copyResourceFileToDocumentsDirectory:filename];
	}
}

+(BOOL) deleteFileFromDocumentsDirectory: (NSString *) fileName{
	NSString *path = [self documentsDirectoryPath];
	NSString *finalpath = [self documentsDirectoryPathForResource:fileName];
	BOOL fileExits = [self fileExistsAtPath:path fileName:fileName];
	if(fileExits){
		NSFileManager *fileManager = [NSFileManager defaultManager];
		[fileManager removeItemAtPath:finalpath error:NULL];
		return YES;
	} else{
		return NO;
	}
	
}

/// temp directory:
+(NSString *) temporaryDirectoryPath {
	//NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *temporaryDirectory = NSTemporaryDirectory();
	return temporaryDirectory;
}

+(BOOL) writeFileToTemporaryDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion {
	NSString *temporaryDirectoryPath = [FileUtils temporaryDirectoryPath];
	NSString *nextSequentialFileName = [FileUtils nextSequentialFileName:prefix fileExtenstion:extenstion directoryPath:temporaryDirectoryPath];
	//NSLog(@"nextSequentialFileName %@",nextSequentialFileName);
	NSString *fileNameWithExtension = [[NSString alloc] initWithString: nextSequentialFileName];
	NSString *finalFilePath = [temporaryDirectoryPath stringByAppendingPathComponent:fileNameWithExtension];
	//NSLog(@"final path name %@",finalFilePath);
	BOOL success = [fileData writeToFile:finalFilePath atomically:YES];	
	[fileNameWithExtension release];
	return success;
}

+(NSString *) temporaryDirectoryPathForResource:(NSString *) fileName {
	NSString *temporaryDirectoryPath = [FileUtils temporaryDirectoryPath];
	NSString *finalPath = [temporaryDirectoryPath stringByAppendingPathComponent:fileName];
	return finalPath;
}

+(BOOL) createNewDirectoryAtPath:(NSString *)path andName:(NSString *)name
{
	NSFileManager * fileManager = [NSFileManager defaultManager];
	NSString *newDirectoryWithPath=[[NSString alloc]initWithFormat:@"%@/%@",path,name];
	//NSLog(@"\n%@",newDirectoryWithPath);
	BOOL created;
	
	#if MAC_OS_X_VERSION_10_5 <= MAC_OS_X_VERSION_MAX_ALLOWED || __IPHONE_2_0 <= __IPHONE_OS_VERSION_MAX_ALLOWED	
		created = [fileManager createDirectoryAtPath:newDirectoryWithPath withIntermediateDirectories:YES attributes:nil error:nil];
	#else
		created = [fileManager createDirectoryAtPath:newDirectoryWithPath attributes:nil];
	#endif
	
		
	[newDirectoryWithPath release];
	return created;
}

+(BOOL) writeFileToAnyDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion andPath:(NSString *)path
{
	//NSLog(@"\n  path : %@",prefix);
	//NSLog(@"\n  path : %@",extenstion);
		//NSLog(@"\n  path : %@",path);

	NSString *nextSequentialFileName = [FileUtils nextSequentialFileName:prefix fileExtenstion:extenstion directoryPath:path];
	
	//NSLog(@"nextSequentialFileName %@",nextSequentialFileName);
	NSString *fileNameWithExtension = [[NSString alloc] initWithString: nextSequentialFileName];
	NSString *finalFilePath = [path stringByAppendingPathComponent:fileNameWithExtension];
	//NSLog(@"final path name %@",finalFilePath);
	BOOL success = [fileData writeToFile:finalFilePath atomically:YES];	
	[fileNameWithExtension release];
	return success;
}

+(NSData *) getDataFromPath:(NSString *) path
{
	NSFileManager *fileManager = [NSFileManager defaultManager];
	
	NSData *data = nil;
	//NSString *ext=@"png";
	//NSString *finalPath=[[NSString alloc]initWithFormat:@"%@.%@",path,ext];
	NSString *finalPath=[[NSString alloc]initWithFormat:@"%@",path];
	//NSLog(@"\ncontest at path : %@",finalPath);
	data=[fileManager contentsAtPath:finalPath];
	[finalPath release];
	return data; 

}

+(NSArray*)getContentsofDirectory:(NSString *)path
{	
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSArray *result=nil;
	
	#if MAC_OS_X_VERSION_10_5 <= MAC_OS_X_VERSION_MAX_ALLOWED || __IPHONE_2_0 <= __IPHONE_OS_VERSION_MAX_ALLOWED	
		result =[fileManager contentsOfDirectoryAtPath:path error:nil];
	#else
		result =[fileManager directoryContentsAtPath:path];
	#endif	
	
		
	////////////NSLog(@"result count :%d",[result count]);
	return result;
	
}

+(BOOL) removeFileFromPath:(NSString *)path
{
	NSFileManager *fileManager = [NSFileManager defaultManager];
	BOOL isRemoved=[fileManager removeItemAtPath:path error:NULL];
	return isRemoved;
}

+(BOOL) fileExistsAtPath:(NSString *)fileName
{
    if(nil == fileName)
        return NO;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    return [fileManager fileExistsAtPath:fileName];
}

+(BOOL) fileExistsAtCatchDirectoryPath:(NSString *)fileName
{
    if(nil == fileName)
        return NO;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *filePath = [[self cachesDirectoryPath] stringByAppendingPathComponent:fileName];
    
    return [fileManager fileExistsAtPath:filePath];
}



+(BOOL) isAudioUrl:(NSURL*) audioURL
{
    if (nil == audioURL) return NO;
    if (nil == audioURL.scheme) return NO ;
    if ([[audioURL.pathExtension lowercaseString] compare:@"mp3"] != NSOrderedSame &&
        [[audioURL.pathExtension lowercaseString] compare:@"aif"] != NSOrderedSame &&
        [[audioURL.pathExtension lowercaseString] compare:@"m4a"] != NSOrderedSame &&
        [[audioURL.pathExtension lowercaseString] compare:@"wav"] != NSOrderedSame) {
        return NO;
        
    }
    return YES;
}

+(BOOL) isPhotoUrl:(NSURL*) photoUrl
{
    if (nil == photoUrl) return NO;
    if (nil == photoUrl.scheme) return NO ;
    if ([[photoUrl.pathExtension lowercaseString] compare:@"jpg"] != NSOrderedSame &&
        [[photoUrl.pathExtension lowercaseString] compare:@"png"] != NSOrderedSame &&
        [[photoUrl.pathExtension lowercaseString] compare:@"bmp"] != NSOrderedSame &&
        [[photoUrl.pathExtension lowercaseString] compare:@"tiff"] != NSOrderedSame) {
        return NO;
        
    }
    return YES;
}

+(BOOL) isVideoUrl:(NSURL*) videoURL
{
    if (nil == videoURL) return NO;
    if (nil == videoURL.scheme) return NO ;
    if ([[videoURL.pathExtension lowercaseString] compare:@"mp4"] != NSOrderedSame &&
        [[videoURL.pathExtension lowercaseString] compare:@"mov"] != NSOrderedSame) {
        return NO;
        
    }
    return YES;
}



+(NSString *) sharedDirectoryPath
{
    if(IOS_5_1_OR_LATER || IOS_5_0_1_OR_LATER)
    {
        NSLog(@"IOS Version: %@",[[UIDevice currentDevice] systemVersion]);
        NSLog(@"File Save to Document Directory");
        
        
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        
        BOOL isExistPath = [self addSkipBackupAttributeToItemAtURL:[NSURL fileURLWithPath:documentsDirectory]];
        if(isExistPath)
        {
            NSLog(@"Do not back up");  
        }
        
        return documentsDirectory;
    }
    else
    {
        NSLog(@"IOS Version: %@",[[UIDevice currentDevice] systemVersion]);
        NSLog(@"File Save to Caches Directory");
        
        
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
        NSString *cachesDirectory = [paths objectAtIndex:0];
        return cachesDirectory;
    }
}

+(NSString*) sharedDirectoryFile:(NSString*)fileName
{
    if(nil == fileName || [fileName isEqualToString:@""])
        return @"";
    
    return [[self documentsDirectoryPath] stringByAppendingPathComponent:fileName];
}

 

+(BOOL)addSkipBackupAttributeToItemAtURL:(NSURL *)url

{
    if(IOS_5_1_OR_LATER)
    {
        assert([[NSFileManager defaultManager] fileExistsAtPath: [url path]]);
        
        NSLog(@"IOS_5_1_OR_LATER");
        
        NSError *error = nil;
        
        BOOL success = [url setResourceValue: [NSNumber numberWithBool: YES]
                        
                                      forKey: @"NSURLIsExcludedFromBackupKey" error: &error];
        
        if(!success){
            
            NSLog(@"Error excluding %@ from backup %@", [url lastPathComponent], error);
            
        }
        
        return success;  
    }
    else if(IOS_5_0_1_OR_LATER)
    {
        assert([[NSFileManager defaultManager] fileExistsAtPath: [url path]]);
        
        
        NSLog(@"IOS_5_0_1_OR_LATER");
        
        const char* filePath = [[url path] fileSystemRepresentation];
        
        
        
        const char* attrName = "com.apple.MobileBackup";
        
        u_int8_t attrValue = 1;
        
        
        
        int result = setxattr(filePath, attrName, &attrValue, sizeof(attrValue), 0, 0);
        
        return result == 0;
    }
    
    return NO;
    
}

+(NSString *) cachesDirectoryPath {
    
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
	NSString *cachesDirectory = [paths objectAtIndex:0];
	return cachesDirectory;
}

+(NSString*) cachesDirectoryFile:(NSString*)fileName
{
    if(nil == fileName || [fileName isEqualToString:@""])
        return @"";
    
    return [[self cachesDirectoryPath] stringByAppendingPathComponent:fileName];
}

#pragma mark Setup Directory
+(BOOL) createImageDirectory
{
    NSString *directroy =[NSString stringWithFormat:@"%@/Images",[self cachesDirectoryPath]];
    BOOL isDir;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if(![fileManager fileExistsAtPath:directroy isDirectory:&isDir])
        [self createNewDirectoryAtPath:[self cachesDirectoryPath] andName:@"Images"];
    
    return TRUE;
}

+(BOOL) createLayerImageDirectory
{
    NSString *directroy =[NSString stringWithFormat:@"%@/LayerImages",[self cachesDirectoryPath]];
    BOOL isDir;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if(![fileManager fileExistsAtPath:directroy isDirectory:&isDir])
        [self createNewDirectoryAtPath:[self cachesDirectoryPath] andName:@"LayerImages"];
    
    return TRUE;
}

+(BOOL) deleteLayerDirectory
{
    NSString *directroy =[NSString stringWithFormat:@"%@/LayerImages",[self cachesDirectoryPath]];
    BOOL isDir;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if([fileManager fileExistsAtPath:directroy isDirectory:&isDir])
    {
        return [fileManager removeItemAtPath:directroy error:nil];
        
    }
    return NO;
}


+(NSString*) sharedLayerDirectoryFile:(NSString*)fileName
{
    if(nil == fileName || [fileName isEqualToString:@""])
        return @"";
    
    NSLog(@"sdfsdfds:%@",[[self cachesDirectoryPath] stringByAppendingPathComponent:[NSString stringWithFormat:@"LayerImages/%@",fileName]]);
     
     
    return [[self cachesDirectoryPath] stringByAppendingPathComponent:[NSString stringWithFormat:@"LayerImages/%@",fileName]];
}


+(NSString*) ImagesDirectoryFile:(NSString*)fileName
{
    if(nil == fileName || [fileName isEqualToString:@""])
        return @"";
    
    return [NSString stringWithFormat:@"%@/Images/%@",[self cachesDirectoryPath],fileName];
}

+(BOOL) deleteDirectory:(NSString*)dirctory
{
    NSString *directroy =[NSString stringWithFormat:@"%@/Images",[self cachesDirectoryPath]];
    BOOL isDir;
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if([fileManager fileExistsAtPath:directroy isDirectory:&isDir])
    {
       return [fileManager removeItemAtPath:directroy error:nil];
        
    }
    return NO;
}
 
@end
