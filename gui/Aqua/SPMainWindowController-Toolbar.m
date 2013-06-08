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

#import "SPMainWindowController-Toolbar.h"
#import "MenuButton.h"

@implementation SPMainWindowController (SPMainWindowControllerToolbar)

- (void)initializeToolbar
{
    NSToolbar *mainToolbar = [[NSToolbar alloc] initWithIdentifier:@"mainToolbar"];
    [mainToolbar autorelease];
    [mainToolbar setDelegate:self];
    [mainToolbar setAllowsUserCustomization:YES];
    [mainToolbar setAutosavesConfiguration:YES];
    [[self window] setToolbar:mainToolbar];
    [[toolbarSearchField cell] setSearchMenuTemplate:searchFieldMenu];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSString *)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

    [item setTag:0];
    [item setTarget:self];

    if ([itemIdentifier isEqualToString:@"quickSearchItem"]) {
        NSRect fRect = [toolbarSearchView frame];
        [item setLabel:@"Quick Search"];
        [item setToolTip:@"Quick Search"];
        [item setView:toolbarSearchView];
        [item setMinSize:fRect.size];
        [item setMaxSize:fRect.size];
    }
    else if ([itemIdentifier isEqualToString:@"advSearchItem"]) {
        [item setLabel:@"Advanced Search"];
        [item setToolTip:@"Advanced Search"];
        [item setImage:[NSImage imageNamed:@"ToolbarAdvancedSearch"]];
        [item setAction:@selector(advSearchShow:)];
    }
    else if ([itemIdentifier isEqualToString:@"toggleDrawer"]) {
        [item setLabel:@"Toggle transfer drawer"];
        [item setToolTip:@"Toggle transfer drawer"];
        [item setImage:[NSImage imageNamed:@"ToolbarDrawer"]];
        [item setAction:@selector(toggleTransferDrawer:)];
    }
    else if ([itemIdentifier isEqualToString:@"connectButton"]) {
        [item setLabel:@"Connect to hub"];
        [item setToolTip:@"Connect to hub"];
        [item setImage:[NSImage imageNamed:@"ToolbarConnect"]];
        [item setAction:@selector(connectShow:)];
    }

    [item setPaletteLabel:[item label]];

    return [item autorelease];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
                    NSToolbarSeparatorItemIdentifier,
                    NSToolbarSpaceItemIdentifier,
                    NSToolbarFlexibleSpaceItemIdentifier,
                    NSToolbarCustomizeToolbarItemIdentifier,
                    @"quickSearchItem",
                    @"advSearchItem",
                    @"toggleDrawer",
                    @"connectButton",
                    nil];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
                    @"connectButton",
                    @"toggleDrawer",
                    NSToolbarFlexibleSpaceItemIdentifier,
                    @"quickSearchItem",
                    @"advSearchItem",
                    nil];
}

@end

