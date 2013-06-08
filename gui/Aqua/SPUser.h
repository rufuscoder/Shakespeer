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

@interface SPUser : NSObject
{
    NSString *nick;
    NSAttributedString *displayNick;
    NSAttributedString *speed;
    NSAttributedString *description;
    NSAttributedString *tag;
    NSAttributedString *email;
    uint64_t size;
    BOOL isOperator;
    unsigned extraSlots;
}

- (id)initWithNick:(NSString *)aNick
       description:(NSString *)aDescription
       tag:(NSString *)aTag
       speed:(NSString *)aSpeed
       email:(NSString *)anEmail
       size:(NSNumber *)aSize
       isOperator:(BOOL)isOp
       extraSlots:(unsigned)anExtraSlots;

+ (id)userWithNick:(NSString *)aNick
       description:(NSString *)aDescription
       tag:(NSString *)aTag
       speed:(NSString *)aSpeed
       email:(NSString *)anEmail
       size:(NSNumber *)aSize
       isOperator:(BOOL)isOp
       extraSlots:(unsigned)anExtraSlots;

+ (id)userWithNick:(NSString *)aNick isOperator:(BOOL)isOp;

- (NSAttributedString *)descriptionString;
- (NSAttributedString *)tag;
- (NSAttributedString *)speed;
- (NSAttributedString *)email;
- (NSString *)nick;
- (NSAttributedString *)displayNick;
- (uint64_t)size;
- (unsigned)extraSlots;
- (BOOL)isOperator;
- (NSComparisonResult)compare:(SPUser *)aUser;

- (void)updateDisplayNick;
- (void)setDescription:(NSString *)aDescription;
- (void)setTag:(NSString *)aTag;
- (void)setSpeed:(NSString *)aSpeed;
- (void)setEmail:(NSString *)anEmail;
- (void)setSize:(NSNumber *)aSize;
- (void)setIsOperator:(BOOL)aFlag;
- (void)setExtraSlots:(int)aValue;

@end

