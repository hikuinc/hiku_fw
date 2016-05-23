//
//  FileUtils.h
//  Kaboom
//


#import <Foundation/Foundation.h>


@interface FileUtils : NSObject 

+(NSString *) documentsDirectoryPath;

+(NSString *) documentsDirectoryPathForResource:(NSString *) fileName;

+(BOOL) fileExistsAtPath:(NSString *) path fileName: (NSString *) fileName; 

+(NSString *) nextSequentialFileName:(NSString *) fileName fileExtenstion:(NSString *) extenstion  directoryPath: (NSString *) path;

+(BOOL) writeFileToDocuemntsDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion;

+(void) copyResourceFileToDocumentsDirectory: (NSString *) fileName;

+(void) copyResourcesToDocumentsDirectory: (NSArray *) resources;

+(BOOL) deleteFileFromDocumentsDirectory: (NSString *) fileName;

+(NSString *) temporaryDirectoryPath;

+(BOOL) writeFileToTemporaryDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion;

+(NSString *) temporaryDirectoryPathForResource:(NSString *) fileName;

+(BOOL) createNewDirectoryAtPath:(NSString *)path andName:(NSString *)name;

+(BOOL) writeFileToAnyDirectory:(NSData *)fileData withPrefix:(NSString *)prefix withExtension:(NSString *)extenstion andPath:(NSString *)path;

+(NSData *)getDataFromPath:(NSString*)path;

+(NSArray *)getContentsofDirectory:(NSString *)path;

+(BOOL) removeFileFromPath:(NSString *)path; 

+(BOOL) isPhotoUrl:(NSURL*) photoUrl;

+(BOOL) isAudioUrl:(NSURL*) audioURL;

+(BOOL) isVideoUrl:(NSURL*) videoURL;

+(BOOL) fileExistsAtPath:(NSString *)fileName;

+(BOOL) fileExistsAtCatchDirectoryPath:(NSString *)fileName;

+(NSString*) sharedDirectoryFile:(NSString*)fileName;

+ (BOOL)addSkipBackupAttributeToItemAtURL:(NSURL *)url;

+(NSString *) cachesDirectoryPath;

+(NSString*) cachesDirectoryFile:(NSString*)fileName;

+(NSString *) sharedDirectoryPath;

+(BOOL) createImageDirectory;

+(NSString*) ImagesDirectoryFile:(NSString*)fileName;

+(BOOL) deleteDirectory:(NSString*)dirctory;

+(BOOL) createLayerImageDirectory;

+(BOOL) deleteLayerDirectory;

+(NSString*) sharedLayerDirectoryFile:(NSString*)fileName;

@end


