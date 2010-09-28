//
//  HTTPServer.m
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

#import "HTTPServer.h"
#import "SynthesizeSingleton.h"
#import <sys/socket.h>
#import <netinet/in.h>
#if TARGET_OS_IPHONE
#import <CFNetwork/CFNetwork.h>
#endif
#import "HTTPResponseHandler.h"

NSString * const HTTPServerNotificationStateChanged = @"ServerNotificationStateChanged";

//
// Internal methods and properties:
//	The "lastError" and "state" are only writable by the server itself.
//
@interface HTTPServer ()
@property (nonatomic, readwrite, retain) NSError *lastError;
@property (readwrite, assign) HTTPServerState state;
@end

@implementation HTTPServer

@synthesize lastError;
@synthesize state;

SYNTHESIZE_SINGLETON_FOR_CLASS(HTTPServer);

//
// init
//
// Set the initial state and allocate the responseHandlers and incomingRequests
// collections.
//
// returns the initialized server object.
//
- (id)init
{
	self = [super init];
	if (self != nil)
	{
		self.state = SERVER_STATE_IDLE;
		responseHandlers = [[NSMutableSet alloc] init];
		incomingRequests =
			CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks);
	}
	return self;
}

-(void)startBonjourServices
{
    netService = [[NSNetService alloc] initWithDomain:@"" type:@"_iproxyhttpserver._tcp." name:@"" port:self.httpServerPort];
    netService.delegate = self;
    [netService publish];
}

-(void)stopBonjourServices
{
    [netService stop];
    [netService release];
    netService = nil;
}

//
// setLastError:
//
// Custom setter method. Stops the server and 
//
// Parameters:
//    anError - the new error value (nil to clear)
//
- (void)setLastError:(NSError *)anError
{
	[anError retain];
	[lastError release];
	lastError = anError;
	
	if (lastError == nil)
	{
		return;
	}
	
	[self stop];
	
	self.state = SERVER_STATE_IDLE;
	NSLog(@"HTTPServer error: %@", self.lastError);
}

//
// errorWithName:
//
// Stops the server and sets the last error to "errorName", localized using the
// HTTPServerErrors.strings file (if present).
//
// Parameters:
//    errorName - the description used for the error
//
- (void)errorWithName:(NSString *)errorName
{
	self.lastError = [NSError
		errorWithDomain:@"HTTPServerError"
		code:0
		userInfo:
			[NSDictionary dictionaryWithObject:
				NSLocalizedStringFromTable(
					errorName,
					@"",
					@"HTTPServerErrors")
				forKey:NSLocalizedDescriptionKey]];	
}

//
// setState:
//
// Changes the server state and posts a notification (if the state changes).
//
// Parameters:
//    newState - the new state for the server
//
- (void)setState:(HTTPServerState)newState
{
	if (state == newState)
	{
		return;
	}

	state = newState;
	
	[[NSNotificationCenter defaultCenter]
		postNotificationName:HTTPServerNotificationStateChanged
		object:self];
}

//
// start
//
// Creates the socket and starts listening for connections on it.
//
- (void)start
{
	self.lastError = nil;
	self.state = SERVER_STATE_STARTING;

	socket = CFSocketCreate(kCFAllocatorDefault, PF_INET, SOCK_STREAM,
		IPPROTO_TCP, 0, NULL, NULL);
	if (!socket)
	{
		[self errorWithName:@"Unable to create socket."];
		return;
	}

	int reuse = true;
	int fileDescriptor = CFSocketGetNative(socket);
	if (setsockopt(fileDescriptor, SOL_SOCKET, SO_REUSEADDR,
		(void *)&reuse, sizeof(int)) != 0)
	{
		[self errorWithName:@"Unable to set socket options."];
		return;
	}
	
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_len = sizeof(address);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(self.httpServerPort);
	CFDataRef addressData =
		CFDataCreate(NULL, (const UInt8 *)&address, sizeof(address));
	[(id)addressData autorelease];
	
	if (CFSocketSetAddress(socket, addressData) != kCFSocketSuccess)
	{
		[self errorWithName:@"Unable to bind socket to address."];
		return;
	}

	listeningHandle = [[NSFileHandle alloc]
		initWithFileDescriptor:fileDescriptor
		closeOnDealloc:YES];

	[[NSNotificationCenter defaultCenter]
		addObserver:self
		selector:@selector(receiveIncomingConnectionNotification:)
		name:NSFileHandleConnectionAcceptedNotification
		object:nil];
	[listeningHandle acceptConnectionInBackgroundAndNotify];
	
	self.state = SERVER_STATE_RUNNING;
    [self startBonjourServices];
}

//
// stopReceivingForFileHandle:close:
//
// If a file handle is accumulating the header for a new connection, this
// method will close the handle, stop listening to it and release the
// accumulated memory.
//
// Parameters:
//    incomingFileHandle - the file handle for the incoming request
//    closeFileHandle - if YES, the file handle will be closed, if no it is
//		assumed that an HTTPResponseHandler will close it when done.
//
- (void)stopReceivingForFileHandle:(NSFileHandle *)incomingFileHandle
	close:(BOOL)closeFileHandle
{
	if (closeFileHandle)
	{
		[incomingFileHandle closeFile];
	}
	
	[[NSNotificationCenter defaultCenter]
		removeObserver:self
		name:NSFileHandleDataAvailableNotification
		object:incomingFileHandle];
	CFDictionaryRemoveValue(incomingRequests, incomingFileHandle);
}

//
// stop
//
// Stops the server.
//
- (void)stop
{
	self.state = SERVER_STATE_STOPPING;

	[[NSNotificationCenter defaultCenter]
		removeObserver:self
		name:NSFileHandleConnectionAcceptedNotification
		object:nil];

	[responseHandlers removeAllObjects];

	[listeningHandle closeFile];
	[listeningHandle release];
	listeningHandle = nil;
	
	for (NSFileHandle *incomingFileHandle in
		[[(NSDictionary *)incomingRequests copy] autorelease])
	{
		[self stopReceivingForFileHandle:incomingFileHandle close:YES];
	}
	
	if (socket)
	{
		CFSocketInvalidate(socket);
		CFRelease(socket);
		socket = nil;
	}

	self.state = SERVER_STATE_IDLE;
    [self stopBonjourServices];
}

//
// receiveIncomingConnectionNotification:
//
// Receive the notification for a new incoming request. This method starts
// receiving data from the incoming request's file handle and creates a
// new CFHTTPMessageRef to store the incoming data..
//
// Parameters:
//    notification - the new connection notification
//
- (void)receiveIncomingConnectionNotification:(NSNotification *)notification
{
	NSDictionary *userInfo = [notification userInfo];
	NSFileHandle *incomingFileHandle =
		[userInfo objectForKey:NSFileHandleNotificationFileHandleItem];

    if(incomingFileHandle)
	{
		CFDictionaryAddValue(
			incomingRequests,
			incomingFileHandle,
			[(id)CFHTTPMessageCreateEmpty(kCFAllocatorDefault, TRUE) autorelease]);
		
		[[NSNotificationCenter defaultCenter]
			addObserver:self
			selector:@selector(receiveIncomingDataNotification:)
			name:NSFileHandleDataAvailableNotification
			object:incomingFileHandle];
		
        [incomingFileHandle waitForDataInBackgroundAndNotify];
    }

	[listeningHandle acceptConnectionInBackgroundAndNotify];
}

//
// receiveIncomingDataNotification:
//
// Receive new data for an incoming connection.
//
// Once enough data is received to fully parse the HTTP headers,
// a HTTPResponseHandler will be spawned to generate a response.
//
// Parameters:
//    notification - data received notification
//
- (void)receiveIncomingDataNotification:(NSNotification *)notification
{
	NSFileHandle *incomingFileHandle = [notification object];
	NSData *data = [incomingFileHandle availableData];
	
	if ([data length] == 0)
	{
		[self stopReceivingForFileHandle:incomingFileHandle close:NO];
		return;
	}

	CFHTTPMessageRef incomingRequest =
		(CFHTTPMessageRef)CFDictionaryGetValue(incomingRequests, incomingFileHandle);
	if (!incomingRequest)
	{
		[self stopReceivingForFileHandle:incomingFileHandle close:YES];
		return;
	}
	
	if (!CFHTTPMessageAppendBytes(
		incomingRequest,
		[data bytes],
		[data length]))
	{
		[self stopReceivingForFileHandle:incomingFileHandle close:YES];
		return;
	}

	if(CFHTTPMessageIsHeaderComplete(incomingRequest))
	{
		HTTPResponseHandler *handler =
			[HTTPResponseHandler
				handlerForRequest:incomingRequest
				fileHandle:incomingFileHandle
				server:self];
		
		[responseHandlers addObject:handler];
		[self stopReceivingForFileHandle:incomingFileHandle close:NO];

		[handler startResponse];	
		return;
	}

	[incomingFileHandle waitForDataInBackgroundAndNotify];
}

//
// closeHandler:
//
// Shuts down a response handler and removes it from the set of handlers.
//
// Parameters:
//    aHandler - the handler to shut down.
//
- (void)closeHandler:(HTTPResponseHandler *)aHandler
{
	[aHandler endResponse];
	[responseHandlers removeObject:aHandler];
}

- (UInt32)httpServerPort
{
	return HTTP_SERVER_PORT;
}

@end
