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

#import <Cocoa/Cocoa.h>

@interface HumanSizeTransformer : NSValueTransformer
{
}
+ (id)defaultHumanSizeTransformer;
@end

@interface HumanSpeedTransformer : NSValueTransformer
{
}
@end

@interface HumanTimeTransformer : NSValueTransformer
{
}
@end

@interface NickImageTransformer: NSValueTransformer
{
    NSImage *defaultImage;
    NSImage *opImage;
}
+ (id)defaultNickImageTransformer;
@end

@interface FiletypeImageTransformer: NSValueTransformer
{
    NSImage *audioImage;
    NSImage *compressedImage;
    NSImage *documentImage;
    NSImage *executableImage;
    NSImage *pictureImage;
    NSImage *videoImage;
    NSImage *folderImage;
    NSImage *anyImage;
}
+ (id)defaultFiletypeImageTransformer;
@end

@interface FilenameImageTransformer: NSValueTransformer
{
}
@end

@interface TransferImageTransformer: NSValueTransformer
{
    NSImage *downloadImage;
    NSImage *uploadImage;
    NSImage *idleImage;
    NSImage *errorImage;
}
@end

@interface TruncateTailTransformer : NSValueTransformer
{
}
@end

@interface FriendStatusTransformer : NSValueTransformer
{
}
@end

@interface FriendLastSeenTransformer : NSValueTransformer
{
}
@end

void registerSPTransformers(void);
