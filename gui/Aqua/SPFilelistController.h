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

#import <Cocoa/Cocoa.h>
#import "SPSideBar.h"
#include "spclient.h"

@class SPOutlineView;

@interface SPFilelistController : NSObject <SPSideBarItem>
{
    IBOutlet NSView *filelistView;
    IBOutlet SPOutlineView *filelist;
    IBOutlet NSSearchField *filterField;
    IBOutlet NSButton *flatButton;
    
    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSMenuItem *magnetMenu;
    IBOutlet NSTableColumn *tcSize;
    IBOutlet NSTableColumn *tcTTH;
    IBOutlet NSTableColumn *tcFilename;

    NSMutableArray *rootItems;
    NSMutableArray *arrangedRootItems;
    BOOL flatStructure;

    NSString *nick;
    NSString *hubAddress;

    fl_dir_t *root;
}
- (id)initWithFile:(NSString *)aPath nick:(NSString *)aNick hub:(NSString *)aHubAddress;
- (void)unbindControllers;
- (NSView *)view;
- (NSImage *)image;
- (NSString *)title;
- (NSMutableArray *)setFiles:(fl_dir_t *)dir;
- (void)sortArray:(NSMutableArray *)anArray usingDescriptors:(NSArray *)sortDescriptors;

- (void)onDoubleClick:(id)sender;
- (IBAction)toggleColumn:(id)sender;
- (IBAction)downloadSelectedItems:(id)sender;
- (IBAction)copyMagnet:(id)sender;
- (IBAction)filter:(id)sender;
- (IBAction)refresh:(id)sender;

@end

