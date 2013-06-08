/* vim: ft=objc
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#import "NSMutableAttributedString-SmileyAdditions.h"

@implementation NSMutableAttributedString (SmileyAdditions)

- (void)replaceSmilies
{
    static NSDictionary *smilies = nil;

    if (smilies == nil) {
        smilies = [[NSDictionary alloc] initWithObjectsAndKeys:
                                                   @"face-smile.png", @":-)",
                                                   @"face-smile.png", @":)",
                                                   @"face-happy.png", @":-D",
                                                   @"face-sad.png", @":-(",
                                                   @"face-sad.png", @":(",
                                                   @"face-silly.png", @"=-)",
                                                   @"face-silly.png", @"=)",
                                                   @"face-wink.png", @";-)",
                                                   @"face-wink.png", @";)",
                                                   @"face14.png", @":-X",
                                                   @"face5.png", @":-O",
                                                   @"face17.png", @":-$",
                                                   @"face10.png", @":-P",
                                                   @"face10.png", @":p",
                                                   @"face10.png", @":P",
                                                   @"face9.png", @":-/",
                                                   @"face13.png", @":-*",
                                                   @"face-crying.png", @":'-(",
                                                   @"face-crying.png", @":'(",
                                                   nil];
    }

    BOOL found;
    do {
        found = NO;
        NSEnumerator *e = [smilies keyEnumerator];
        NSString *smiley;
        while ((smiley = [e nextObject])) {
            NSRange smileyRange = [[self mutableString] rangeOfString:smiley];
            if (smileyRange.location != NSNotFound) {
                NSString *filename = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:[smilies objectForKey:smiley]];
                NSFileWrapper *myFileWrapper = [[[NSFileWrapper alloc] initWithPath:filename] autorelease];
                NSTextAttachment *myTextAtt = [[[NSTextAttachment alloc] initWithFileWrapper:myFileWrapper] autorelease];
                NSAttributedString *myAttStr = [NSAttributedString attributedStringWithAttachment:myTextAtt];

                [self replaceCharactersInRange:smileyRange withAttributedString:myAttStr];
                found = YES;
            }
        }
    } while (found);
}

@end
