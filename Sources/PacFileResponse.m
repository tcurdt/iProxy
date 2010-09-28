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

#import "PacFileResponse.h"
#import "HTTPServer.h"

@implementation PacFileResponse

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
    NSData *fileData = nil;
    NSString *currentIP = [self serverIPForRequest];
    
    NSString *requestPath = [url path];
    
    if ([@"/http.pac" isEqualToString:requestPath] && currentIP) {
        fileData = [[NSString stringWithFormat:
            @"function FindProxyForURL(url, host) { return \"PROXY %@:%d\"; }",
            currentIP, HTTP_PROXY_PORT] dataUsingEncoding:NSUTF8StringEncoding];
    }
    
    if ([@"/socks.pac" isEqualToString:requestPath] && currentIP) {
        fileData = [[NSString stringWithFormat:
            @"function FindProxyForURL(url, host) { return \"SOCKS %@:%d\"; }",
            currentIP, SOCKS_PROXY_PORT] dataUsingEncoding:NSUTF8StringEncoding];
    }
    
    if (fileData) {
        CFHTTPMessageRef response = CFHTTPMessageCreateResponse(kCFAllocatorDefault, 
                                    200, 
                                    NULL, 
                                    kCFHTTPVersion1_1);

        CFHTTPMessageSetHeaderFieldValue(response, 
                                         (CFStringRef)@"Content-Type", 
                                         (CFStringRef)@"application/x-ns-proxy-autoconfig");
        CFHTTPMessageSetHeaderFieldValue(response, 
                                         (CFStringRef)@"Connection", 
                                         (CFStringRef)@"close");
        CFHTTPMessageSetHeaderFieldValue(response,
                                         (CFStringRef)@"Content-Length",
                                         (CFStringRef)[NSString stringWithFormat:@"%ld", [fileData length]]);

        CFDataRef headerData = CFHTTPMessageCopySerializedMessage(response);
        
        @try
        {
            [fileHandle writeData:(NSData *)headerData];
            [fileHandle writeData:fileData];
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
    } else {
        CFHTTPMessageRef response = CFHTTPMessageCreateResponse(kCFAllocatorDefault, 
                                    currentIP ? 404 : 500, 
                                    NULL, 
                                    kCFHTTPVersion1_1);

        CFDataRef headerData = CFHTTPMessageCopySerializedMessage(response);
        
        @try
        {
            [fileHandle writeData:(NSData *)headerData];
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
}

@end
