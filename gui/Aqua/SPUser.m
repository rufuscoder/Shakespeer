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

#import "SPUser.h"
#import "NSStringExtensions.h"

@implementation SPUser

- (id)initWithNick:(NSString *)aNick
       description:(NSString *)aDescription
               tag:(NSString *)aTag
             speed:(NSString *)aSpeed
             email:(NSString *)anEmail
              size:(NSNumber *)aSize
        isOperator:(BOOL)isOp
       extraSlots:(unsigned)anExtraSlots
{
    if ((self = [super init])) {
        nick = [aNick copy];
        [self setDescription:aDescription];
        [self setTag:aTag];
        [self setSpeed:aSpeed];
        [self setEmail:anEmail];
        [self setSize:aSize];
        [self setIsOperator:isOp];
        [self setExtraSlots:anExtraSlots];
        [self updateDisplayNick];
    }
    
    return self;
}

+ (id)userWithNick:(NSString *)aNick
       description:(NSString *)aDescription
               tag:(NSString *)aTag
             speed:(NSString *)aSpeed
             email:(NSString *)anEmail
              size:(NSNumber *)aSize
        isOperator:(BOOL)isOp
       extraSlots:(unsigned)anExtraSlots
{
    return [[[SPUser alloc] initWithNick:aNick
                             description:aDescription
                                     tag:aTag
                                   speed:aSpeed
                                   email:anEmail
                                    size:aSize
                              isOperator:isOp
                              extraSlots:anExtraSlots] autorelease];
}

+ (id)userWithNick:(NSString *)aNick isOperator:(BOOL)isOp
{
    return [self userWithNick:aNick
                 description:nil
                 tag:nil
                 speed:nil
                 email:nil
                 size:nil
                 isOperator:isOp
                 extraSlots:0];
}

- (void)dealloc
{
    [nick release];
    [displayNick release];
    [description release];
    [tag release];
    [speed release];
    [email release];
    [super dealloc];
}

- (NSAttributedString *)descriptionString
{
    return description;
}

- (NSString *)nick
{
    return nick;
}

- (NSAttributedString *)displayNick
{
    return displayNick;
}

- (uint64_t)size
{
    return size;
}

- (BOOL)isOperator
{
    return isOperator;
}

- (NSComparisonResult)compare:(SPUser *)aUser
{
    int op_diff = ([aUser isOperator] ? 1 : 0) - (isOperator ? 1 : 0);

    if (op_diff == 0)
        return [nick caseInsensitiveCompare:[aUser nick]];
    return op_diff;
}

- (NSAttributedString *)tag
{
    return tag;
}

- (NSAttributedString *)speed
{
    return speed;
}

- (NSAttributedString *)email
{
    return email;
}

- (unsigned)extraSlots
{
    return extraSlots;
}

- (void)setDescription:(NSString *)aDescription
{
    [description release];
    description = [aDescription truncatedString:NSLineBreakByTruncatingTail];
}

- (void)setTag:(NSString *)aTag
{
    [tag release];
    tag = [aTag truncatedString:NSLineBreakByTruncatingTail];
}

- (void)setSpeed:(NSString *)aSpeed
{
    [speed release];
    speed = [aSpeed truncatedString:NSLineBreakByTruncatingTail];
}

- (void)setEmail:(NSString *)anEmail
{
    [email release];
    email = [anEmail truncatedString:NSLineBreakByTruncatingTail];
}

- (void)setSize:(NSNumber *)aSize
{
    size = [aSize unsignedLongLongValue];
}

- (void)setIsOperator:(BOOL)aFlag
{
    isOperator = aFlag;
}

- (void)setExtraSlots:(int)aValue
{
    if (aValue != extraSlots) {
        extraSlots = aValue;
        [self updateDisplayNick];
    }
}

- (void)updateDisplayNick
{
    [displayNick release];

    if (extraSlots > 0) {
        displayNick = [[NSString stringWithFormat:@"%@ (+%u)", nick, extraSlots]
                                  truncatedString:NSLineBreakByTruncatingTail];
    }
    else {
        displayNick = [nick truncatedString:NSLineBreakByTruncatingTail];
    }
}

- (NSString *)description
{
  return [NSString stringWithFormat:@"nick = %@, operator = %@", nick, isOperator ? @"YES" : @"NO"];
}

@end

