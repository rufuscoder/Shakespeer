/* vim: ft=objc
 *
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
#import "MHSysTree.h"
#import "SPSideBar.h"

@class FilteringArrayController;
@class SPUser;

/* There is one HubController created for each hub you're connected to. */
@interface SPHubController : NSWindowController <SPSideBarItem>
{
    IBOutlet NSTextView *chatView;
    IBOutlet NSTextField *chatInput;
    IBOutlet NSTableView *userTable;
    IBOutlet NSScrollView *userScrollView;
    IBOutlet NSTextField *hubStatisticsField;
    IBOutlet NSSearchField *nickFilter;
    IBOutlet NSMenu *nickMenu;

    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSTableColumn *tcNick;
    IBOutlet NSTableColumn *tcShare;
    IBOutlet NSTableColumn *tcTag;
    IBOutlet NSTableColumn *tcSpeed;
    IBOutlet NSTableColumn *tcDescription;
    IBOutlet NSTableColumn *tcEmail;
    IBOutlet NSTableColumn *tcIcon;

    NSString *name;
    NSString *address;
    NSString *nick;
    NSString *descriptionString;
    
    // all users on this hub. this tree is automatically sorted by invoking compare:
    // on all objects that are inserted. in our case it's full of SPUser objects.
    MHSysTree *usersTree;
    // a subset of allUsers. the primary datasource for the tableview.
    // it is updated (from usersTree) on intervals, to batch updates to the table view somewhat.
    NSMutableArray *filteredUsers;
    
    // mechanism used to batch updates of allUsers and the users tableview.
    BOOL needUpdating;
    NSTimer *updateTimer;
    
    // is this hub selected? if not, we can skip doing a bunch of work.
    BOOL isShowing; 
    
    BOOL highlighted;
    BOOL disconnected;
    NSString *encoding;
    int numStaticNickMenuEntries;
    unsigned nops;
    uint64_t totsize;
    
    // when this is non-null, we're autocompleting nicks. it's enumerating through
    // an already-filtered list that is built on the first tab press.
    NSEnumerator *nickAutocompleteEnumerator;
    // this is the saved start of a nick completion
    NSString *savedNickStart;
}

- (IBAction)sendMessage:(id)sender;
- (IBAction)startPrivateChat:(id)sender;
- (IBAction)addFriend:(id)sender;
- (IBAction)browseUser:(id)sender;
- (IBAction)autoMatchFilelist:(id)sender;
- (IBAction)toggleColumn:(id)sender;
- (IBAction)bookmarkHub:(id)sender;
- (IBAction)grantExtraSlot:(id)sender;
- (IBAction)filter:(id)sender;

- (SPUser *)findUserWithNick:(NSString *)aNick;
- (void)updateUserTable:(NSTimer *)aTimer;
- (id)initWithAddress:(NSString *)anAddress nick:(NSString *)aNick;
- (NSImage *)image;
- (NSString *)title;
- (NSString *)address;
- (NSString *)name;
- (NSString *)nick;
- (BOOL)isHighlighted;
- (void)setHighlighted:(BOOL)aFlag;
- (void)setDescriptionString:(NSString *)aDescription;
- (void)setName:(NSString *)aName;
- (void)unbindControllers;
- (void)focusChatInput;
- (void)setConnected;
- (void)setEncoding:(NSString *)anEncoding;

@end

