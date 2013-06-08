/* vim: ft=objc
 *
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

#import <Cocoa/Cocoa.h>
#import "NSStringExtensions.h"
#import "SPSideBar.h"

@class SPOutlineView;

@interface SPSearchWindowController : NSWindowController <SPSideBarItem>
{
    IBOutlet SPOutlineView *searchResultTable;
    IBOutlet NSTextField *statusField;
    IBOutlet NSButton *requireTTHButton;
    IBOutlet NSButton *requireOpenSlotButton;
    IBOutlet NSSearchField *searchField;

    NSString *searchString;
    NSString *searchSize;
    int sizeRestriction;
    int searchType;
    NSString *hubAddress;
    NSTimer *updateTimer;

    int tag;
    NSMutableArray *searchResults;
    NSMutableArray *arrangedSearchResults;
    NSMutableArray *searchResultsDelta;
    NSMutableDictionary *searchResultsIndex;
    NSString *lastFilterString;
    int lastFlags;
    BOOL needUpdating;
    BOOL highlighted;
    
    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSTableColumn *tcNick;
    IBOutlet NSTableColumn *tcSize;
    IBOutlet NSTableColumn *tcTTH;
    IBOutlet NSTableColumn *tcSlots;
    IBOutlet NSTableColumn *tcPath;
    IBOutlet NSTableColumn *tcHub;
    IBOutlet NSTableColumn *tcSpeed;

    IBOutlet NSMenu *contextMenu;
    IBOutlet NSMenuItem *menuItemCopyTTH;
}

- (IBAction)toggleColumn:(id)sender;
- (IBAction)doSearch:(id)sender;
- (IBAction)downloadFile:(id)sender;
- (IBAction)downloadParentDirectory:(id)sender;
- (IBAction)getFilelist:(id)sender;
- (IBAction)autoMatchFilelist:(id)sender;
- (IBAction)copyTTH:(id)sender;
- (IBAction)startPrivateChat:(id)sender;
- (IBAction)filter:(id)sender;
- (IBAction)relaunchSearch:(id)sender;

- (NSImage *)image;
- (NSString *)title;

- (id)initWithString:(NSString *)aSearchString size:(NSString *)aSearchSize
     sizeRestriction:(int)aSizeRestriction type:(int)aSearchType
          hubAddress:(NSString *)aHubAddress;
- (void)newSearchWithString:(NSString *)aSearchString size:(NSString *)aSearchSize
            sizeRestriction:(int)aSizeRestriction type:(int)aSearchType
                 hubAddress:(NSString *)aHubAddress;
- (void)filterSearchResults:(NSString *)filterString flags:(int)flags;
- (void)sortArray:(NSMutableArray *)anArray usingDescriptors:(NSArray *)sortDescriptors;

- (int)searchID;

@end
