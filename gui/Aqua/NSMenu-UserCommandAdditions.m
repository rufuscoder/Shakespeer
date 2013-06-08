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

#import "NSMenu-UserCommandAdditions.h"
#import "SPUserCommand.h"

@implementation NSMenu (UserCommandAdditions)

- (NSMenu *)makeSubMenuIn:(NSMenu *)aMenu withTitle:(NSString *)menuTitle
{
    NSMenu *subMenu = nil;
    NSMenuItem *subMenuItem = [aMenu itemWithTitle:menuTitle];
    if (subMenuItem) {
        /* if one already exists, return it */
        subMenu = [subMenuItem submenu];
    }
    else {
        /* otherwise create the new menu */
        subMenu = [[[NSMenu alloc] initWithTitle:menuTitle] autorelease];
        NSMenuItem *menuItem = [aMenu addItemWithTitle:menuTitle
                                                action:nil
                                         keyEquivalent:@""];
        [menuItem setSubmenu:subMenu];
    }
    
    return subMenu;
}

- (void)addUserCommand:(SPUserCommand *)userCommand action:(SEL)anAction
                target:(id)aTarget staticEntries:(int)staticEntries
{
    if ([userCommand type] == 255) {
        /* erase all sent commands */
        int i;
        for (i = [self numberOfItems]; i > staticEntries; i--) {
            if (i - 1 >= 0)
                [self removeItemAtIndex:i - 1];
        }
    }
    else if ([userCommand type] == 0) {
        /* separator */
        /* prevent multiple consecutive separators */
        int n = [self numberOfItems];
        if (n > 0 && [[self itemAtIndex:n-1] isSeparatorItem] == NO)
            [self addItem:[NSMenuItem separatorItem]];
    }
    else if ([userCommand type] == 1 || [userCommand type] == 2) {
        NSArray *titleArray = [[userCommand title] componentsSeparatedByString:@"\\"];
        unsigned int i;
        NSMenu *subMenu = self;
        for (i = 0; i < [titleArray count]; i++) {
            if (i == [titleArray count] - 1) {
                NSMenuItem *menuItem = [subMenu addItemWithTitle:[titleArray objectAtIndex:i]
                                                          action:anAction
                                                   keyEquivalent:@""];
                [menuItem setTarget:aTarget];
                [menuItem setEnabled:YES];
                [menuItem setRepresentedObject:userCommand];
            }
            else {
                subMenu = [self makeSubMenuIn:subMenu withTitle:[titleArray objectAtIndex:i]];
            }
        }
    }
}

@end

