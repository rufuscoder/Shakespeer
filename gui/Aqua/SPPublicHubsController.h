/* vim: ft=objc
 * Mac DC++. An Aqua user interface for DC++.
 * Copyright (C) 2004 Jonathan Jansson, jonathan.dator@home.se
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
#import "SPSideBar.h"
#include "hublist.h"

@class FilteringArrayController;

/*!
 * @class PublicHubsController
 * @abstract Does what Windows/PublicHubsFrm.cpp do in DC++
 * @discussion
 * PublicHubsController is a subclass of NSTabViewItem. After initWithIdentifier: is called
 * it is ready to be put in an NSTabView.
 */
@interface SPPublicHubsController : NSObject <SPSideBarItem>
{
    IBOutlet NSView *hubListView;
    IBOutlet FilteringArrayController *arrayController;
    IBOutlet NSTableView *hubTable;
    IBOutlet NSButton *refreshButton;
    
    IBOutlet NSMenu *contextMenu;
    
    NSMenu *columnsMenu;
    NSMutableArray *allTableColumns;
        
    NSMutableArray *hubs;
    BOOL refreshInProgress;
}

- (id)init;
- (void)unbindControllers;
- (id)view;
- (NSString *)title;
- (NSImage *)image;
- (void)setHubsFromList:(hublist_t *)hublist;
- (void)addHubsToBookmarks:(id)sender;

- (IBAction)toggleColumn:(id)sender;
- (IBAction)tableDoubleActionConnect:(id)sender;
- (IBAction)connect:(id)sender;
- (IBAction)refresh:(id)sender;

@end

