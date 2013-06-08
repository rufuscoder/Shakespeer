/* vim: ft=objc
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

#import "SPUserCommand.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"

@implementation SPUserCommand

- (id)initWithTitle:(NSString *)aTitle
            command:(NSString *)aCommand
               type:(int)aType
            context:(int)aContext
                hub:(NSString *)anAddress
{
    if ((self = [super init])) {
        title = [aTitle retain];
        command = [aCommand retain];
        address = [anAddress retain];
        type = aType;
        context = aContext;
    }
    
    return self;
}

- (void)dealloc
{
    [title release];
    [command release];
    [address release];
    [super dealloc];
}

- (NSString *)title
{
    return title;
}

- (NSString *)command
{
    return command;
}

- (int)type
{
    return type;
}

- (int)context
{
    return context;
}

- (NSString *)address
{
    return address;
}

- (void)executeForNicks:(NSArray *)nickArray myNick:(NSString *)aMyNick
{
    /* replace all %[mynick] placeholders */
    NSMutableString *mutableCommand = [command mutableCopy];
    [mutableCommand replaceOccurrencesOfString:@"%[mynick]"
                                    withString:aMyNick
                                       options:NSCaseInsensitiveSearch
                                         range:NSMakeRange(0, [mutableCommand length])];

    /* replace all %[line:xxx] placeholders */
    while (1) {
        NSRange lineRange = [mutableCommand rangeOfString:@"%\x5bline:"];
        if (lineRange.location == NSNotFound)
            break;
        unsigned s = lineRange.location + lineRange.length;
        NSRange nameRange = [mutableCommand rangeOfString:@"\x5d"
                                                  options:0
                                                    range:NSMakeRange(s, [mutableCommand length] - s)];
        if (nameRange.location == NSNotFound)
            break;
        lineRange.length += nameRange.location - s + 1;

        NSString *param = [[SPMainWindowController sharedMainWindowController]
            requestUserCommandParameter:[mutableCommand substringWithRange:NSMakeRange(s, nameRange.location - s)]
                                  title:title];
        if (param == nil) {
            [mutableCommand release];
            return;
        }

        [mutableCommand replaceOccurrencesOfString:[mutableCommand substringWithRange:lineRange]
                                 withString:param
                                    options:NSCaseInsensitiveSearch
                                      range:NSMakeRange(0, [mutableCommand length])];
    }

    NSLog(@"instantiating usercommand [%@]", mutableCommand);

    /* handle cases where there are no nick parameters */
    if ([nickArray count] == 0) {
        if ([mutableCommand rangeOfString:@"%\x5b"].location != NSNotFound)
            NSLog(@"No nick parameters, skipping user command [%s]", mutableCommand);
        else
            [[SPApplicationController sharedApplicationController] sendRawCommand:mutableCommand toHub:address];

        [mutableCommand release];
        
        return;
    }

    NSEnumerator *e = [nickArray objectEnumerator];
    NSDictionary *nickParameter;

    /* handle nick-once type of commands: make nicks unique */
    if ([self type] == 2) {
        NSMutableDictionary *uniqueNicks = [[[NSMutableDictionary alloc] init] autorelease];
        while ((nickParameter = [e nextObject]) != nil)
            [uniqueNicks setObject:nickParameter forKey:[nickParameter objectForKey:@"nick"]];
        
        e = [uniqueNicks objectEnumerator];
    }

    while ((nickParameter = [e nextObject]) != nil) {
        NSMutableString *nickCommand = [mutableCommand mutableCopy];
        NSEnumerator *npe = [nickParameter keyEnumerator];
        NSString *param;
        
        while ((param = [npe nextObject]) != nil) {
            [nickCommand replaceOccurrencesOfString:[NSString stringWithFormat:@"%%[%@]", param]
                                         withString:[nickParameter objectForKey:param]
                                            options:NSCaseInsensitiveSearch
                                              range:NSMakeRange(0, [nickCommand length])];
        }

        [[SPApplicationController sharedApplicationController] sendRawCommand:nickCommand toHub:address];
        [nickCommand release];
    }
    
    [mutableCommand release];
}

@end
