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

@class SPSideBar;
@class SPPublicHubsController;
@class SPFriendsController;
@class SPTransferController;
@class MenuButton;

@interface SPMainWindowController : NSWindowController
{
    NSToolbar *toolbar;
	
    SPPublicHubsController *publicHubsController;
    id bookmarkController;
    id queueController;
    SPTransferController *transferController;
	
    NSString *passwordHub, *passwordNick;
    NSTimer *statusMessageTimer;
    NSMutableDictionary *privateChats;
    int currentSearchType;
    id currentSidebarItem;
    NSMutableDictionary *hubs;
    NSMutableDictionary *userCommands;
    
    // set when we're autoconnecting to the last connected hubs, so we won't try to
    // sync it while we're in the middle of resetting the last session.
    BOOL restoringLastConnectedHubs;

    IBOutlet SPSideBar *sideBar;
    IBOutlet NSTextField *statusBar;
    IBOutlet NSTextField *statisticsBar;
    IBOutlet NSWindow *passwordWindow;
    IBOutlet NSTextField *passwordField;
    IBOutlet NSTextField *passwordNickField;
    IBOutlet NSTextField *passwordHubField;
    IBOutlet NSSplitView *sidebarSplitView;
    
    IBOutlet NSDrawer *transferDrawer;
    IBOutlet NSArrayController *transferArrayController;
    IBOutlet NSTableView *transferDrawerTable;
    
    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSTableColumn *tcUser;
    IBOutlet NSTableColumn *tcStatus;
    IBOutlet NSTableColumn *tcTimeLeft;
    IBOutlet NSTableColumn *tcSpeed;
    IBOutlet NSTableColumn *tcFilename;
    IBOutlet NSTableColumn *tcSize;
    IBOutlet NSTableColumn *tcPath;
    IBOutlet NSTableColumn *tcHub;

    /* The searchFieldMenu is used both by toolbarSearchField and advSearchField */
    IBOutlet NSMenu *searchFieldMenu;

    IBOutlet NSView *toolbarSearchView;
    IBOutlet NSSearchField *toolbarSearchField;
    
    IBOutlet NSWindow *connectWindow;
    IBOutlet NSTextField *connectAddress;
    IBOutlet NSTextField *connectNickname;
    IBOutlet NSSecureTextField *connectPassword;

    IBOutlet NSWindow *advSearchWindow;
    IBOutlet NSSearchField *advSearchField;
    IBOutlet NSPopUpButton *advSearchSizeType;
    IBOutlet NSTextField *advSearchSize;
    IBOutlet NSPopUpButton *advSearchSizeSuffix;
    IBOutlet NSPopUpButton *advSearchHubs;

    /* UserCommand parameter window */
    IBOutlet NSWindow *UCParamWindow;
    IBOutlet NSTextField *UCParamTextField;
    IBOutlet NSTextField *UCParamCommandTitle;
    IBOutlet NSTextField *UCParamName;
}

+ (id)sharedMainWindowController;

- (IBAction)openHublist:(id)sender;
- (IBAction)openFriends:(id)sender;
- (IBAction)openBookmarks:(id)sender;
- (IBAction)openDownloads:(id)sender;
- (IBAction)openUploads:(id)sender;

- (IBAction)toggleTransferDrawer:(id)sender;
- (IBAction)toggleColumn:(id)sender;
- (IBAction)quickSearchExecute:(id)sender;
- (IBAction)acceptPassword:(id)sender;
- (IBAction)cancelPassword:(id)sender;
- (IBAction)setSearchType:(id)sender;

- (IBAction)connectShow:(id)sender;
- (IBAction)connectExecute:(id)sender;
- (IBAction)connectCancel:(id)sender;

- (IBAction)advSearchShow:(id)sender;
- (IBAction)advSearchExecute:(id)sender;
- (IBAction)advSearchCancel:(id)sender;

- (IBAction)cancelTransfer:(id)sender;
- (IBAction)removeSource:(id)sender;
- (IBAction)removeQueue:(id)sender;
- (IBAction)removeAllSourcesWithNick:(id)sender;
- (IBAction)browseUser:(id)sender;
- (IBAction)privateMessage:(id)sender;

- (NSString *)requestUserCommandParameter:(NSString *)paramName title:(NSString *)title;
- (IBAction)acceptUCParam:(id)sender;
- (IBAction)cancelUCParam:(id)sender;

- (void)prevSidebarItem;
- (void)nextSidebarItem;

- (NSDictionary *)connectedHubs;
- (id)hubWithAddress:(NSString *)aHubAddress;

// persists all currently connected hubs to prefs, so we will reconnect to them
// at the next start.
- (void)syncHubsToDisk;

- (void)restoreLastHubSession;

- (NSArray *)userCommandsForHub:(NSString *)aHubAddress;

- (void)closeCurrentSidebarItem;
- (void)highlightItem:(id)anItem;
- (void)setStatusMessage:(NSString *)message;

- (void)performSearchFor:(NSString *)searchString
                    size:(NSString *)searchSize
         sizeRestriction:(int)sizeRestriction
                    type:(int)searchType
              hubAddress:(NSString *)hubAddress;
@end

