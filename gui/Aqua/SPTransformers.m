/* vim: ft=objc fdm=indent foldnestmax=1
 *
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
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

#import "SPTransformers.h"
#import "SPUser.h"
#import "SPTransferController.h"
#import "NSStringExtensions.h"

#include "util.h"

void registerSPTransformers(void)
{
    [NSValueTransformer setValueTransformer:[[[HumanSizeTransformer alloc] init] autorelease] forName:@"HumanSizeTransformer"];
    [NSValueTransformer setValueTransformer:[[[HumanSpeedTransformer alloc] init] autorelease] forName:@"HumanSpeedTransformer"];
    [NSValueTransformer setValueTransformer:[[[HumanTimeTransformer alloc] init] autorelease] forName:@"HumanTimeTransformer"];
    [NSValueTransformer setValueTransformer:[[[NickImageTransformer alloc] init] autorelease] forName:@"NickImageTransformer"];
    [NSValueTransformer setValueTransformer:[FiletypeImageTransformer defaultFiletypeImageTransformer] forName:@"FiletypeImageTransformer"];
    [NSValueTransformer setValueTransformer:[[[FilenameImageTransformer alloc] init] autorelease] forName:@"FilenameImageTransformer"];
    [NSValueTransformer setValueTransformer:[[[TransferImageTransformer alloc] init] autorelease] forName:@"TransferImageTransformer"];
    [NSValueTransformer setValueTransformer:[[[TruncateTailTransformer alloc] init] autorelease] forName:@"TruncateTailTransformer"];
    [NSValueTransformer setValueTransformer:[[[FriendStatusTransformer alloc] init] autorelease] forName:@"FriendStatusTransformer"];
    [NSValueTransformer setValueTransformer:[[[FriendLastSeenTransformer alloc] init] autorelease] forName:@"FriendLastSeenTransformer"];
}

@implementation HumanSizeTransformer

+ (id)defaultHumanSizeTransformer
{
    static id def = nil;
    if (def == nil)
        def = [[HumanSizeTransformer alloc] init];
    
    return def;
}

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    NSAttributedString *str = [[NSString stringWithUTF8String:str_size_human([value longLongValue])] truncatedString:NSLineBreakByTruncatingTail];
    
    return [str autorelease];
}
@end

@implementation HumanSpeedTransformer

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    NSString *str;
    int num = [value intValue];

    if (num < 1024)
        str = [NSString stringWithFormat:@"%i B/s", num];
    else if (num < 999.0*1024) /* kilobinary */
        str = [NSString stringWithFormat:@"%.1f KiB/s", (float)num/1024];
    else if (num < 999.0*1024*1024) /* megabinary */
        str = [NSString stringWithFormat:@"%.1f MiB/s", (float)num/(1024*1024)];
    else
        str = [NSString stringWithFormat:@"%.1f GiB/s", (float)num/(1024*1024*1024)];

    return [[str truncatedString:NSLineBreakByTruncatingTail] autorelease];
}
@end

@implementation HumanTimeTransformer

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    int seconds = [value intValue];
    int hours = seconds / 3600;
    seconds -= hours * 3600;
    int minutes = seconds / 60;
    seconds -= minutes * 60;
    
    return [[[NSString stringWithFormat:@"%02u:%02u:%02u", hours, minutes, seconds] truncatedString:NSLineBreakByTruncatingTail] autorelease];
}
@end


@implementation NickImageTransformer

- (id)init
{
    if ((self = [super init])) {
        defaultImage = [[NSImage imageNamed:@"user"] retain];
        opImage = [[NSImage imageNamed:@"op"] retain];
    }
    
    return self;
}

- (void)dealloc
{
    [defaultImage release];
    [opImage release];
    [super dealloc];
}

+ (id)defaultNickImageTransformer
{
    static id def = nil;
    
    if (def == nil)
        def = [[NickImageTransformer alloc] init];
    
    return def;
}

+ (Class)transformedValueClass
{
    return [NSImage class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    if ([value isOperator])
        return opImage;
    else
        return defaultImage;
}
@end

@implementation FiletypeImageTransformer

- (id)init
{
    if ((self = [super init])) {
        audioImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".mp3"] retain];
        [audioImage setSize:NSMakeSize(16.0, 16.0)];
        
        compressedImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".gz"] retain];
        [compressedImage setSize:NSMakeSize(16.0, 16.0)];
        
        documentImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".pdf"] retain];
        [documentImage setSize:NSMakeSize(16.0, 16.0)];
        
        executableImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".sh"] retain];
        [executableImage setSize:NSMakeSize(16.0, 16.0)];
        
        pictureImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".jpg"] retain];
        [pictureImage setSize:NSMakeSize(16.0, 16.0)];
        
        videoImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".avi"] retain];
        [videoImage setSize:NSMakeSize(16.0, 16.0)];
        
        folderImage = [[[NSWorkspace sharedWorkspace] iconForFile:@"/usr"] retain];
        [folderImage setSize:NSMakeSize(16.0, 16.0)];
        
        anyImage = [[[NSWorkspace sharedWorkspace] iconForFileType:@".asdfqs"] retain];
        [anyImage setSize:NSMakeSize(16.0, 16.0)];
    }
    
    return self;
}

+ (id)defaultFiletypeImageTransformer
{
    static id def = nil;
    
    if (def == nil)
        def = [[FiletypeImageTransformer alloc] init];
    
    return def;
}

- (void)dealloc
{
    [audioImage release];
    [compressedImage release];
    [documentImage release];
    [executableImage release];
    [pictureImage release];
    [videoImage release];
    [folderImage release];
    [anyImage release];
    [super dealloc];
}

+ (Class)transformedValueClass
{
    return [NSImage class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    NSImage *img = nil;
    
    switch([value intValue]) {
        case SHARE_TYPE_AUDIO:
            img = audioImage;
            break;
        case SHARE_TYPE_COMPRESSED:
            img = compressedImage;
            break;
        case SHARE_TYPE_DOCUMENT:
            img = documentImage;
            break;
        case SHARE_TYPE_EXECUTABLE:
            img = executableImage;
            break;
        case SHARE_TYPE_IMAGE:
            img = pictureImage;
            break;
        case SHARE_TYPE_MOVIE:
            img = videoImage;
            break;
        case SHARE_TYPE_DIRECTORY:
            img = folderImage;
            break;
        case SHARE_TYPE_ANY:
        default:
            img = anyImage;
            break;
    }
    
    return img;
}
@end

@implementation FilenameImageTransformer

+ (Class)transformedValueClass
{
    return [NSImage class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    share_type_t ftype = share_filetype([value UTF8String]);
    
    return [[FiletypeImageTransformer defaultFiletypeImageTransformer]
        transformedValue:[NSNumber numberWithInt:ftype]];
}
@end

@implementation TransferImageTransformer

- (id)init
{
    if ((self = [super init])) {
        downloadImage = [[NSImage imageNamed:@"download"] retain];
        uploadImage = [[NSImage imageNamed:@"upload"] retain];
        idleImage = [[NSImage imageNamed:@"idle"] retain];
        errorImage = [[NSImage imageNamed:@"error"] retain];
    }
    
    return self;
}

- (void)dealloc
{
    [downloadImage release];
    [uploadImage release];
    [idleImage release];
    [errorImage release];
    [super dealloc];
}

+ (Class)transformedValueClass
{
    return [NSImage class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    int state = [value intValue];
    switch(state) {
        case SPTransferState_Downloading:
            return downloadImage;
            break;
        case SPTransferState_Uploading:
            return uploadImage;
            break;
        case SPTransferState_Idle:
            return idleImage;
            break;
        case SPTransferState_Error:
        default:
            return errorImage;
            break;
    }
}

@end

@implementation TruncateTailTransformer

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    return [[(NSString *)value truncatedString:NSLineBreakByTruncatingTail] autorelease];
}

@end

@implementation FriendStatusTransformer

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    BOOL isOnline = [value boolValue];
    NSString *statusString;
    
    if (isOnline)
        statusString = @"Online";
    else
        statusString = @"Offline";
    
    return statusString;
}

@end

@implementation FriendLastSeenTransformer

+ (Class)transformedValueClass
{
    return [NSAttributedString class];
}

+ (BOOL)supportsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    NSDate *now = [NSDate date];
    int seconds = [now timeIntervalSinceDate:value];
    
    // If the date is in the future, the user has never been seen. We'll add
    // ten seconds to that threshold to account for the running time of this
    // method. Actually, we could as well add several years to that threshold
    // since new friends are initialized at [NSDate distantFuture] which currently
    // defaults to the year 4000 or similar.
    if (seconds < -10)
        return @"Never";
    
    // we'll display this as a fuzzy date string
    int years = seconds / (60 * 60 * 24 * 365);
    if (years > 0)
        return @"More than a year ago";
    
    seconds -= years * (60 * 60 * 24 * 365);
    int months = seconds / (60 * 60 * 24 * 30);
    if (months > 6)
        return @"More than half a year ago";
    if (months >= 1)
        return @"More than a month ago";
    
    seconds -= months * (60 * 60 * 24 * 30);
    int weeks = seconds / (60 * 60 * 24 * 7);
    if (weeks > 0)
        return @"A few weeks ago";
    
    seconds -= weeks * (60 * 60 * 24 * 30);
    int days = seconds / (60 * 60 * 24);
    if (days > 0)
        return @"A few days ago";
    
    return @"Today";
}

@end