#include "log.h"
#include "SPlog.h"

void SPLogMessage(NSString *fmt, ...);
void SPLogMessage(NSString *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    NSString *msg = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);

    sp_log(LOG_LEVEL_INFO, "%s", [msg UTF8String]);

    [msg release];
}

