/* vim: ft=objc
 *
 * Copyright 2008 Kevan Carstensen <kevan@isnotajoke.com>
 *
 * Much of the code within is taken from code previously found in 
 * SPChatWindowController.m and SPHubController.m, both created by
 * Martin Hedenfalk, who presumably also holds the copyright to them.
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
 *
 */

#import "NSTextView-ChatFormattingAdditions.h"
#import "NSMutableAttributedString-SmileyAdditions.h"
#import "URLMutableAttributedString.h"
#import "SPUserDefaultKeys.h"
#import "SPApplicationController.h"

@implementation NSTextView (ChatFormattingAdditions)

- (void)addChatMessage:(NSMutableAttributedString *)aMessage {
    [aMessage detectURLs:[NSColor blueColor]];
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsShowSmileyIcons]) {
        [aMessage replaceSmilies];
    }
    float oldPosition = [[[self enclosingScrollView] verticalScroller] 
                         floatValue];
    [[self textStorage] appendAttributedString:aMessage];
    // Respect the user's scroll choices.
    if (oldPosition == 1.0) {
        [self scrollRangeToVisible:
         NSMakeRange([[self textStorage] length], 0)];
    }
}

#pragma mark Formatting methods

- (void)addStatusMessage:(NSString *)aMessage {
    NSMutableAttributedString *attrmsg = [[NSMutableAttributedString alloc] 
                                          initWithString:aMessage];
    
    [attrmsg addAttribute:NSForegroundColorAttributeName
                    value:[NSColor orangeColor]
                    range:NSMakeRange(0, [aMessage length])];
    
    [self addChatMessage:attrmsg];
    [attrmsg release];
}

- (void)addPublicMessage:(NSString *)aMessage
                fromNick:(NSString *)aNick 
                  myNick:(NSString *)myNick {
    NSString *dateString = [[NSDate date] descriptionWithCalendarFormat:@"%H:%M" 
                                                               timeZone:nil 
                                                                 locale:nil];
    BOOL meMessage = NO;
    NSString *msg;
    if ([aMessage hasPrefix:@"/me "]) {
        msg = [NSString stringWithFormat:@"[%@] %@ %@\n",
               dateString, aNick, 
               [aMessage substringWithRange:NSMakeRange(4, [aMessage length] - 4)]];
        meMessage = TRUE;
    }
    else {
        msg = [NSString stringWithFormat:@"[%@] <%@> %@\n", dateString, aNick, aMessage];
    }
    NSMutableAttributedString *attrmsg = [[[NSMutableAttributedString alloc] 
                                           initWithString:msg] autorelease];
    
    NSColor *textColor;
    if ([aNick isEqualToString:myNick]) {
        textColor = [NSColor blueColor];
    }
    // See if the author of this message is a friend.
    // If so, color the friend's name in purple.
    else {
        BOOL isFriend = NO;
        NSArray *friends = [[NSUserDefaults standardUserDefaults] arrayForKey:SPFriends];
        NSEnumerator *e = [friends objectEnumerator];
        NSMutableDictionary *friend = nil;
        while ((friend = [e nextObject])) {
            NSString *friendName = [friend objectForKey:@"name"];
            if ([friendName isEqualToString:aNick]) {
                isFriend = YES;
                break;
            }
        }
        
        if (isFriend) {
            textColor = [NSColor purpleColor];
        }
        else {
            textColor = [NSColor redColor];
        }
    }
    unsigned int dateLength = [dateString length] + 3;
    if (meMessage) {
        [attrmsg addAttribute:NSForegroundColorAttributeName
                        value:textColor
                        range:NSMakeRange(dateLength, [attrmsg length] - dateLength)];
    }
    else {
        [attrmsg addAttribute:NSForegroundColorAttributeName
                        value:textColor
                        range:NSMakeRange(dateLength, 2 + [aNick length])];
    }
    
    [self addChatMessage:attrmsg];
}

- (void)clear {
    [[self textStorage] setAttributedString:[[[NSMutableAttributedString alloc] initWithString:@""] autorelease]];
}

@end
