/* This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import "SPQueueItem.h"
#import "NSStringExtensions.h"

#include "util.h"

@implementation SPQueueItem

- (id)initWithTarget:(NSString *)aTarget
{
    if ((self = [super init])) {
        target = [aTarget retain];
        path = [[[aTarget stringByDeletingLastPathComponent] stringByAbbreviatingWithTildeInPath] retain];
        filename = [[aTarget lastPathComponent] retain];
        displayName = [filename retain];
        size = [[NSNumber numberWithUnsignedLongLong:0] retain];
        sources = [[NSMutableDictionary alloc] init];
        [self setPriority:[NSNumber numberWithInt:3]]; /* Normal priority */
        [self setStatusString:@"Queued"];
    }
    
    return self;
}

- (id)initWithTarget:(NSString *)aTarget displayName:(NSString *)aDisplayName
{
    if ((self = [self initWithTarget:aTarget])) {
        [displayName autorelease];
        displayName = [aDisplayName copy];
    }
    
    return self;
}

- (void)dealloc
{
    [target release];
    [filename release];
    [displayName release];
    [path release];
    [tth release];
    [attributedTTH release];
    [size release];
    [priority release];
    [priorityString release];
    [sources release];
    [children release];
    [status release];
    [super dealloc];
}

#pragma mark -

- (NSString *)filename
{
    return filename;
}

- (NSString *)displayName
{
    return displayName;
}

- (NSString *)path
{
    return path;
}

- (NSString *)target
{
    return target;
}

- (NSString *)tth
{
    return tth;
}

- (NSAttributedString *)attributedTTH
{
    return attributedTTH;
}

- (void)setTTH:(NSString *)aTTH
{
    if (aTTH != tth) {
        [tth release];
        tth = [aTTH retain];
        [attributedTTH release];
        attributedTTH = [tth truncatedString:NSLineBreakByTruncatingMiddle];
    }
}

- (NSNumber *)size
{
    return size;
}

- (NSNumber *)exactSize
{
    return size;
}

- (void)setSize:(NSNumber *)aSize
{
    if (aSize != size) {
        [size release];
        size = [aSize retain];
    }
}

- (void)addSource:(NSString *)sourcePath nick:(NSString *)nick
{
    [sources setObject:sourcePath forKey:nick];
}

- (void)removeSourceForNick:(NSString *)nick
{
    [sources removeObjectForKey:nick];
}

- (NSArray *)nicks
{
    return [sources allKeys];
}

- (NSMutableArray *)children
{
    return children;
}

- (void)setIsFilelist:(BOOL)aFlag
{
    isFilelist = aFlag;
}

- (BOOL)isFilelist
{
    return isFilelist;
}

- (void)setIsDirectory
{
    isDirectory = YES;
    [self setStatusString:@""];
    if (children == nil)
        children = [[NSMutableArray alloc] init];
}

- (BOOL)isDirectory
{
    return isDirectory;
}

- (NSString *)users
{
    NSArray *nicks = [self nicks];
    
    NSString *usersString = nil;
    switch([nicks count]) {
        case 0:
            usersString = @"none";
            break;
        case 1:
            usersString = [nicks objectAtIndex:0];
            break;
        case 2:
            usersString = [NSString stringWithFormat:@"%@, %@", [nicks objectAtIndex:0], [nicks objectAtIndex:1]];
            break;
        case 3:
            usersString = [NSString stringWithFormat:@"%@, %@, %@", [nicks objectAtIndex:0], [nicks objectAtIndex:1], [nicks objectAtIndex:2]];
            break;
        default:
            usersString = [NSString stringWithFormat:@"%i users", [nicks count]];
            break;
    }
    return [[usersString truncatedString:NSLineBreakByTruncatingTail] autorelease];
}

- (int)filetype
{
    return share_filetype([filename UTF8String]);
}

- (NSNumber *)priority
{
    return priority;
}

- (NSAttributedString *)priorityString
{
    return priorityString;
}

- (void)setPriority:(NSNumber *)aPriority
{
    if (aPriority != priority) {
        [priority release];
        priority = [aPriority retain];
        
        [priorityString release];
        
        switch([aPriority unsignedIntValue]) {
            case 0:
                priorityString = [@"Paused" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 1:
                priorityString = [@"Lowest" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 2:
                priorityString = [@"Low" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 4:
                priorityString = [@"High" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 5:
                priorityString = [@"Highest" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 3:
            default:
                priorityString = [@"Normal" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
        }
    }
}

- (void)setStatus:(NSNumber *)statusNumber
{
    if (statusNumber != status) {
        [status release];
        status = [statusNumber retain];
    }
}

- (NSNumber *)status
{
    return status;
}

- (void)setStatusString:(NSString *)aStatusString
{
    if (aStatusString != statusString) {
        [statusString release];
        statusString = [aStatusString retain];
    }
}

- (NSString *)statusString
{
    return statusString;
}

- (void)setFinished
{
    [self setStatus:[NSNumber numberWithFloat:100.0]];
    [self setStatusString:@"Finished"];
    isFinished = YES;
}

- (BOOL)isFinished
{
    return isFinished;
}

- (BOOL)isWaitingToBeRemoved
{
    return isWaitingToBeRemoved;
}

- (void)setIsWaitingToBeRemoved:(BOOL)waitingToBeRemoved
{
    isWaitingToBeRemoved = waitingToBeRemoved;
}

@end
