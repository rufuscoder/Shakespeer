/* vim: ft=objc
 */
#import <Foundation/Foundation.h>
#include <sys/types.h>
#include <unistd.h>

void SPLogMessage(NSString *fmt, ...);

#define SPLog(fmt, ...)  SPLogMessage(@"[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

