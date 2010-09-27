#import "UIDeviceAdditions.h"

#import <ifaddrs.h>
#import <arpa/inet.h>

@implementation UIDevice (UIDeviceAdditions)

- (NSString *)IPAddressForInterface:(NSString*)ifname
{
    NSString *address = nil;

    struct ifaddrs *interfaces = NULL;

    int success = getifaddrs(&interfaces);

    if (success == 0) {
        struct ifaddrs *temp_addr = NULL;

        temp_addr = interfaces;
        while(temp_addr != NULL) {

            if(temp_addr->ifa_addr->sa_family == AF_INET) {

                if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:ifname]) {

                    address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
                }
            }

            temp_addr = temp_addr->ifa_next;
        }
    }

    freeifaddrs(interfaces);

    return address;
}

@end
