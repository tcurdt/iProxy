#import "NSStringAdditions.h"
#import <CommonCrypto/CommonDigest.h>

@implementation NSString (NSStringAdditions)

- (NSString*) URLEncodedString
{
    NSString *result = (NSString *)CFURLCreateStringByAddingPercentEscapes(
        kCFAllocatorDefault,
        (CFStringRef)self,
        NULL,
        CFSTR("!*'();:@&=+$,/?%#[]"),
        kCFStringEncodingUTF8);

	return [result autorelease];
}

- (NSString*) URLDecodedString
{
	NSString *result = (NSString *)CFURLCreateStringByReplacingPercentEscapesUsingEncoding(
        kCFAllocatorDefault,
        (CFStringRef)self,
        CFSTR(""),
        kCFStringEncodingUTF8);

	return [result autorelease];
}

- (NSString *) md5
{
	const char *cStr = [self UTF8String];
	unsigned char result[16];
	CC_MD5( cStr, strlen(cStr), result );
	return [NSString stringWithFormat:
		@"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		result[0], result[1], result[2], result[3], 
		result[4], result[5], result[6], result[7],
		result[8], result[9], result[10], result[11],
		result[12], result[13], result[14], result[15]
		];	
}

@end
