//
//  AppTextFileResponse.m
//  TextTransfer
//
//  Created by Matt Gallagher on 2009/07/13.
//  Copyright 2009 Matt Gallagher. All rights reserved.
//
//  Permission is given to use this source code file, free of charge, in any
//  project, commercial or otherwise, entirely at your risk, with the condition
//  that any redistribution (in part or whole) of source code must retain
//  this copyright and permission notice. Attribution in compiled projects is
//  appreciated but not required.
//

#import "AppTextFileResponse.h"
#import "HTTPServer.h"
#import <ifaddrs.h>
#import <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

@implementation AppTextFileResponse

//
// load
//
// Implementing the load method and invoking
// [HTTPResponseHandler registerHandler:self] causes HTTPResponseHandler
// to register this class in the list of registered HTTP response handlers.
//
+ (void)load
{
	[HTTPResponseHandler registerHandler:self];
}

//
// canHandleRequest:method:url:headerFields:
//
// Class method to determine if the response handler class can handle
// a given request.
//
// Parameters:
//    aRequest - the request
//    requestMethod - the request method
//    requestURL - the request URL
//    requestHeaderFields - the request headers
//
// returns YES (if the handler can handle the request), NO (otherwise)
//
+ (BOOL)canHandleRequest:(CFHTTPMessageRef)aRequest
                  method:(NSString *)requestMethod
                     url:(NSURL *)requestURL
            headerFields:(NSDictionary *)requestHeaderFields
{
    return YES;
}

//
// startResponse
//
// Since this is a simple response, we handle it synchronously by sending
// everything at once.
//
- (void)startResponse
{
	int socket = [fileHandle fileDescriptor];
    struct sockaddr_in address;
    socklen_t address_len;
    NSData *fileData = nil;
    
    address_len = sizeof(address);
    if (getsockname(socket, (struct sockaddr *)&address, &address_len) == 0) {
        fileData = [[NSString stringWithFormat:@"function FindProxyForURL(url, host) { return \"SOCKS %s:%d\"; }", inet_ntoa(address.sin_addr), [HTTPServer sharedHTTPServer].proxyPort] dataUsingEncoding:NSUTF8StringEncoding];
    }
     

    //	NSData *fileData =
    //		[NSData dataWithContentsOfFile:[AppTextFileResponse pathForFile]];
	CFHTTPMessageRef response =
    CFHTTPMessageCreateResponse(kCFAllocatorDefault, 
                                fileData?200:505, 
                                NULL, 
                                kCFHTTPVersion1_1);
	CFHTTPMessageSetHeaderFieldValue(response, 
                                     (CFStringRef)@"Content-Type", 
                                     (CFStringRef)@"application/x-ns-proxy-autoconfig");
    //	CFHTTPMessageSetHeaderFieldValue(
    //        response, (CFStringRef)@"Content-Type", (CFStringRef)@"text/plain");
	CFHTTPMessageSetHeaderFieldValue(response, 
                                     (CFStringRef)@"Connection", 
                                     (CFStringRef)@"close");
	CFHTTPMessageSetHeaderFieldValue(response,
                                     (CFStringRef)@"Content-Length",
                                     (CFStringRef)[NSString stringWithFormat:@"%ld", 
                                                   [fileData length]]);
	CFDataRef headerData = CFHTTPMessageCopySerializedMessage(response);
    
	@try
	{
		[fileHandle writeData:(NSData *)headerData];
        if (fileData) {
            [fileHandle writeData:fileData];
        }
	}
	@catch (NSException *exception)
	{
		// Ignore the exception, it normally just means the client
		// closed the connection from the other end.
	}
	@finally
	{
		CFRelease(headerData);
		[server closeHandler:self];
	}
}

#if 0
//
// pathForFile
//
// In this sample application, the only file returned by the server lives
// at a fixed location, whose path is returned by this method.
//
// returns the path of the text file.
//
+ (NSString *)pathForFile
{
	NSString *path =
    [NSSearchPathForDirectoriesInDomains(
                                         NSApplicationSupportDirectory,
                                         NSUserDomainMask,
                                         YES)
     objectAtIndex:0];
	BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:path];
	if (!exists)
	{
		[[NSFileManager defaultManager]
         createDirectoryAtPath:path
         withIntermediateDirectories:YES
         attributes:nil
         error:nil];
	}
	return [path stringByAppendingPathComponent:@"file.txt"];
}
#endif

@end
