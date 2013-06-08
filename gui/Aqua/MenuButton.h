/* vim: ft=objc
 * MenuButton.h
 * Fire
 *
 * Created by Colter Reed on Thu Nov 08 2001.
 * Copyright (c) 2001-2003 Fire Development Team and/or epicware, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import <Cocoa/Cocoa.h>

@interface MenuButton : NSButton
{
    NSTimer *clickHoldTimer;
    IBOutlet NSMenu *menu;
    BOOL menuDidDisplay;
    NSTimeInterval menuDelay;
    NSToolbarItem* toolbarItem;
    NSControlSize _controlSize;
    NSImage* _originalImage;
}

- (void)mouseDown:(NSEvent *)theEvent;
- (void)mouseUp:(NSEvent *)theEvent;

- (void)setMenuDelay:(NSTimeInterval)aDelay;
- (NSTimeInterval)menuDelay;

- (void)setMenu:(NSMenu *)aMenu;
- (NSMenu *)menu;

- (void)displayMenu;

- (void)setControlSize:(NSControlSize)size;
- (void)setToolbarItem:(NSToolbarItem*)item;

@end
