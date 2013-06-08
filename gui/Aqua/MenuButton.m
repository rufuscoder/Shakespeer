/*
 * MenuButton.m
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

#import "MenuButton.h"

@implementation MenuButton

- (id)initWithFrame:(NSRect)frameRect
{
    if ((self = [super initWithFrame:frameRect])) {
        _controlSize = NSRegularControlSize;
        [self setButtonType:NSMomentaryPushButton];
        [self setBordered:NO];
        [self setImagePosition:NSImageOnly];
        [[self cell] setHighlightsBy:NSPushInCellMask];
        [[self cell] setShowsStateBy:NSContentsCellMask];
        [self setState:0];
    }

    return self;
}

- (void)dealloc
{
    [clickHoldTimer invalidate];
    [clickHoldTimer autorelease];
    [menu autorelease];
    [toolbarItem release];
    [_originalImage release];

    clickHoldTimer = nil;
    menu = nil;

    [super dealloc];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    if (![self isEnabled])
        return;
    
    if (!menu) {
        [super mouseDown:theEvent];
        return;
    }
    
    [self highlight:YES];
    [clickHoldTimer invalidate];
    [clickHoldTimer autorelease];
    menuDidDisplay = NO;
    clickHoldTimer = [[NSTimer scheduledTimerWithTimeInterval:menuDelay
                                                       target:self
                                                     selector:@selector(displayMenu)
                                                     userInfo:nil
                                                      repeats:NO] retain];
}

- (void)mouseUp:(NSEvent *)theEvent
{
    [clickHoldTimer invalidate];
    [clickHoldTimer autorelease];
    clickHoldTimer = nil;
    
    if (!menuDidDisplay && ([theEvent type] & NSLeftMouseUp))
        [self sendAction:[self action] to:[self target]];
    
    if (menuDidDisplay && ([theEvent type] & NSLeftMouseUp))
        menuDidDisplay = NO;
    
    [self highlight:NO];
    [super mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    return;
}

- (void) setMenuDelay:(NSTimeInterval) aDelay
{
    menuDelay = aDelay;
}

- (NSTimeInterval) menuDelay
{
    return menuDelay;
}

- (void)setMenu:(NSMenu *)aMenu
{
    [menu autorelease];
    menu = [aMenu copy];
    [self setEnabled:(menu != nil)];
}

- (NSMenu *)menu
{
    return menu;
}

- (void)displayMenu
{
    [NSMenu popUpContextMenu:menu withEvent:[[NSApplication sharedApplication] currentEvent] forView:self];
    menuDidDisplay = YES;
    [self mouseUp:[[NSApplication sharedApplication] currentEvent]];
}

- (void)setImage:(NSImage*)image
{
    NSImage *newImage;
    [_originalImage release];
    _originalImage = [image copy];
    newImage = [image copy];
    [newImage setScalesWhenResized:NO];
    [newImage setSize:NSMakeSize(28,22)];
    [super setImage:newImage];
    [newImage release];
}

- (void)setControlSize:(NSControlSize)size
{
    NSImage* newImage = [[_originalImage copy] autorelease];
    [newImage setScalesWhenResized:NO];
    [toolbarItem setMinSize:NSMakeSize(28,22)];
    [toolbarItem setMaxSize:NSMakeSize(28,22)];
    [newImage setSize:NSMakeSize(28,22)];
    _controlSize = size;
    [super setImage:newImage];
}

- (void)setFrame:(NSRect)frameRect
{
    NSImage* newImage = [[_originalImage copy] autorelease];
    [newImage setScalesWhenResized:NO];
    [newImage setSize:frameRect.size];
    [super setImage:newImage];
    [super setFrame:frameRect];
}

- (NSControlSize)controlSize
{
    return _controlSize;
}

- (void)setToolbarItem:(NSToolbarItem*)item
{
    [toolbarItem release];
    toolbarItem = [item retain];
}

@end
