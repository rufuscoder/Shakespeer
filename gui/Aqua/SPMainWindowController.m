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

#import "SPMainWindowController.h"
#import "SPMainWindowController-Toolbar.h"
#import "SPApplicationController.h"
#import "SPHubController.h"
#import "SPSearchWindowController.h"
#import "SPChatWindowController.h"
#import "SPFilelistController.h"
#import "SPBookmarkController.h"
#import "SPQueueController.h"
#import "SPUserCommand.h"
#import "SPGrowlBridge.h"
#import "SPSideBar.h"
#import "SPPublicHubsController.h"
#import "SPFriendsController.h"
#import "SPTransferController.h"
#import "SPUserDefaultKeys.h"
#import "SPNotificationNames.h"
#import "MenuButton.h"

#include "util.h"

#define SIDEBAR_MIN_SIZE 100
#define SIDEBAR_MAX_SIZE 300

@implementation SPMainWindowController

+ (id)sharedMainWindowController
{
    static id sharedMainWindowController = nil;

    if (sharedMainWindowController == nil) {
        sharedMainWindowController = [[SPMainWindowController alloc] init];
    }

    return sharedMainWindowController;
}

- (id)init
{
    self = [super initWithWindowNibName:@"MainWindow"];
    if (self) {
        privateChats = [[NSMutableDictionary alloc] init];
        currentSearchType = SHARE_TYPE_ANY;
        [self setWindowFrameAutosaveName:@"MainWindow"];
        hubs = [[NSMutableDictionary alloc] init];
    }
    
    return self;
}

- (void)windowDidLoad
{
    [sideBar setDelegate:self];
    
    NSString *lastItem = [[NSUserDefaults standardUserDefaults] objectForKey:@"lastSidebarItem"];
    
    [self openHublist:self];
    [self openFriends:self];
    [self openBookmarks:self];
    [self openDownloads:self];
    [self openUploads:self];
    
    if ([lastItem isEqualToString:@"Public Hubs"])
        [self openHublist:self];
    else if ([lastItem isEqualToString:@"Friends"])
        [self openFriends:self];
    else if ([lastItem isEqualToString:@"Bookmarks"])
        [self openBookmarks:self];
    else if ([lastItem isEqualToString:@"Downloads"])
        [self openDownloads:self];
    else if ([lastItem isEqualToString:@"Uploads"])
        [self openUploads:self];
    else
        [self openBookmarks:self];
    
    [self initializeToolbar];

    /* initialize and register with Growl */
    [SPGrowlBridge sharedGrowlBridge];

    /* set the drawer height */
    NSSize contentSize = [transferDrawer contentSize];
    contentSize.height = [[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsDrawerHeight];
    [transferDrawer setContentSize:contentSize];

    [transferArrayController bind:@"contentArray" toObject:transferController withKeyPath:@"transfers" options:nil];

    [[toolbarSearchField cell] setSearchMenuTemplate:searchFieldMenu];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(hubAddNotification:)
                                                 name:SPNotificationHubAdd
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(hubRedirectNotification:)
                                                 name:SPNotificationHubRedirect
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(hubDisconnectedNotification:)
                                                 name:SPNotificationHubDisconnected
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(downloadFinishedNotification:)
                                                 name:SPNotificationDownloadFinished
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(filelistFinishedNotification:)
                                                 name:SPNotificationFilelistFinished
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(statusMessageNotification:)
                                                 name:SPNotificationStatusMessage
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(needPasswordNotification:)
                                                 name:SPNotificationNeedPassword
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(startChatNotification:)
                                                 name:SPNotificationStartChat
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(endChatNotification:)
                                                 name:SPNotificationEndChat
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(userCommandNotification:)
                                                 name:SPNotificationUserCommand
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(shareStatsNotification:)
                                                 name:SPNotificationShareStats
                                               object:nil];
}

- (void)awakeFromNib
{
    [tcUser retain];
    [tcStatus retain];
    [tcTimeLeft retain];
    [tcSpeed retain];
    [tcFilename retain];
    [tcSize retain];
    [tcPath retain];
    [tcHub retain];
    
    NSArray *tcs = [transferDrawerTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];
        
        if (tc == tcUser)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcStatus)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcTimeLeft)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcSpeed)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcFilename)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcSize)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcPath)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
        else if (tc == tcHub)
            [[columnsMenu itemWithTag:7] setState:NSOnState];
    }
    
    [[transferDrawerTable headerView] setMenu:columnsMenu];
    
    // Only show active downloads and uploads in table
    [transferArrayController setFilterPredicate:
        [NSPredicate predicateWithFormat:@"state BETWEEN {0, 1}"]];
    [transferArrayController rearrangeObjects];
}

- (void)dealloc
{
    [statusMessageTimer invalidate];
    [statusMessageTimer release];
    [privateChats release];
    [hubs release];
    
    // Free table columns
    [tcUser release];
    [tcStatus release];
    [tcTimeLeft release];
    [tcSpeed release];
    [tcFilename release];
    [tcSize release];
    [tcPath release];
    [tcHub release];
    
    [super dealloc];
}

- (void)clearStatusMessage:(NSTimer *)aTimer
{
    [statusBar setStringValue:@""];
}

- (void)highlightItem:(id)anItem
{
    if (anItem != currentSidebarItem) {
        if ([anItem respondsToSelector:@selector(setHighlighted:)]) {
            [anItem setHighlighted:YES];
            [sideBar setNeedsDisplay];
        }
    }
}

- (NSDictionary *)connectedHubs
{
    return [[hubs copy] autorelease];
}

- (id)hubWithAddress:(NSString *)aHubAddress
{
    return [hubs objectForKey:aHubAddress];
}

- (NSArray *)userCommandsForHub:(NSString *)aHubAddress
{
    return [userCommands objectForKey:aHubAddress];
}

- (void)syncHubsToDisk
{
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsSessionRestore]) {
        // saves the array of addresses to prefs, so we can easily reconnect to them at the next start.
        [[NSUserDefaults standardUserDefaults] setObject:[hubs allKeys] forKey:SPPrefsLastConnectedHubs];
    }
}

- (void)restoreLastHubSession
{
    // retrieve the list of all servers that were connected the last session.
    NSArray *lastConnectedHubs = [[NSUserDefaults standardUserDefaults] arrayForKey:SPPrefsLastConnectedHubs];
    if (lastConnectedHubs) {
        // suppress hublist syncs while we're doing this.
        restoringLastConnectedHubs = YES;
    
        NSEnumerator *addressEnumerator = [lastConnectedHubs objectEnumerator];
        NSString *currentAddress = nil;
        // reopen each connection. if any of the addresses are bookmarked, we can use password/nick, etc
        // from that.
        while ((currentAddress = [addressEnumerator nextObject])) {
            NSDictionary *hubBookmark = [[SPBookmarkController sharedBookmarkController] bookmarkForHub:currentAddress];
            if (hubBookmark) {
                // we had this hub in our bookmarks, so let the bookmark controller deal with it, so it can
                // reuse the correct nick, password, etc.
                [[SPBookmarkController sharedBookmarkController] connectToBookmark:hubBookmark];
            } else {
                [[SPApplicationController sharedApplicationController] connectWithAddress:currentAddress
                                                                                     nick:nil
                                                                              description:nil
                                                                                 password:nil
                                                                                 encoding:nil];
            }
        } // while
        
        restoringLastConnectedHubs = NO;
    }
}

#pragma mark -
#pragma mark Sphubd notifications

- (void)shareStatsNotification:(NSNotification *)aNotification
{
    NSString *path = [[aNotification userInfo] objectForKey:@"path"];

    if ([path isEqualToString:@""]) {
        uint64_t size = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
        uint64_t totsize = [[[aNotification userInfo] objectForKey:@"totsize"] unsignedLongLongValue];
        uint64_t dupsize = [[[aNotification userInfo] objectForKey:@"dupsize"] unsignedLongLongValue];
        uint64_t uniqsize = totsize - dupsize;
        unsigned percentComplete = (unsigned)(100 * ((double)size / (uniqsize ? uniqsize : 1)));

        NSString *msg = [NSString stringWithFormat:@"Sharing %s (%u%% hashed)",
                         str_size_human(size), percentComplete];

        [statisticsBar setStringValue:msg];
    }
}

- (void)userCommandNotification:(NSNotification *)aNotification
{
    int context = [[[aNotification userInfo] objectForKey:@"context"] intValue];
    NSString *ucHub = [[aNotification userInfo] objectForKey:@"hubAddress"];
    
    if (userCommands == nil) {
        userCommands = [[NSMutableDictionary alloc] initWithCapacity:1];
    }
    
    NSMutableArray *ucArray = [userCommands objectForKey:ucHub];
    if (ucArray == nil) {
        ucArray = [[NSMutableArray alloc] initWithCapacity:1];
        [userCommands setObject:ucArray forKey:ucHub];
    }
    
    int type = [[[aNotification userInfo] objectForKey:@"type"] intValue];
    NSString *title = [[aNotification userInfo] objectForKey:@"description"];
    NSString *command = [[aNotification userInfo] objectForKey:@"command"];
    
    SPUserCommand *uc = [[SPUserCommand alloc] initWithTitle:title
                                                     command:command
                                                        type:type
                                                     context:context
                                                         hub:ucHub];
    
    [ucArray addObject:[uc autorelease]];
}

- (void)endChatNotification:(NSNotification *)aNotification
{
    NSString *chatNick = [[aNotification userInfo] objectForKey:@"nick"];
    NSString *chatHubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];
    NSString *chatKey = [NSString stringWithFormat:@"%@@%@", chatNick, chatHubAddress];
    [privateChats removeObjectForKey:chatKey];
}

- (void)startChatNotification:(NSNotification *)aNotification
{
    NSString *chatRemoteNick = [[aNotification userInfo] objectForKey:@"remote_nick"];
    NSString *chatMyNick = [[aNotification userInfo] objectForKey:@"my_nick"];
    NSString *chatHubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];
    NSString *chatKey = [NSString stringWithFormat:@"%@@%@", chatRemoteNick, chatHubAddress];

    SPChatWindowController *chatController = [privateChats objectForKey:chatKey];
    if (chatController == nil) {
        chatController = [[SPChatWindowController alloc] initWithRemoteNick:chatRemoteNick
                                                                        hub:chatHubAddress
                                                                     myNick:chatMyNick];
        if (chatController == nil) {
            NSLog(@"failed to create chat controller");
        }
        else {
            [sideBar addItem:chatController];
            /* [sideBar displayItem:chatController]; */
            [privateChats setObject:chatController forKey:chatKey];
            [chatController release];
        }
    }
}

- (void)needPasswordNotification:(NSNotification *)aNotification
{
    passwordHub = [[aNotification userInfo] objectForKey:@"hubAddress"];
    passwordNick = [[aNotification userInfo] objectForKey:@"nick"];

    NSString *storedPassword = [[SPBookmarkController sharedBookmarkController]
        passwordForHub:passwordHub nick:passwordNick];
    if (storedPassword && [storedPassword length] > 0) {
        [[SPApplicationController sharedApplicationController]
            setPassword:storedPassword
                 forHub:passwordHub];
        return;
    }

    [passwordNickField setStringValue:passwordNick];
    [passwordHubField setStringValue:passwordHub];

    [NSApp beginSheet:passwordWindow
           modalForWindow:[self window]
           modalDelegate:nil
           didEndSelector:nil
           contextInfo:nil];

    [NSApp runModalForWindow:passwordWindow];

    [NSApp endSheet:passwordWindow];
    [passwordWindow orderOut:self];
}

- (IBAction)acceptPassword:(id)sender
{
    [NSApp stopModal];
    [[SPApplicationController sharedApplicationController] setPassword:[passwordField stringValue]
                                                                forHub:passwordHub];
}

- (IBAction)cancelPassword:(id)sender
{
    [NSApp stopModal];
    [[SPApplicationController sharedApplicationController] disconnectFromAddress:passwordHub];
}

- (void)setStatusMessage:(NSString *)message
{
    [statusMessageTimer invalidate];
    [statusMessageTimer release];

    statusMessageTimer = [[NSTimer scheduledTimerWithTimeInterval:30
                                                           target:self
                                                         selector:@selector(clearStatusMessage:)
                                                         userInfo:nil
                                                          repeats:NO] retain];
    
    [statusBar setStringValue:message];
}

- (void)statusMessageNotification:(NSNotification *)aNotification
{
    [self setStatusMessage:[[aNotification userInfo] objectForKey:@"message"]];
}

- (void)filelistFinishedNotification:(NSNotification *)aNotification
{
    NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];
    NSString *filename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    NSString *hubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];

    SPFilelistController *fl = [[SPFilelistController alloc] initWithFile:filename
                                                                     nick:nick
                                                                      hub:hubAddress];
    if (fl == nil) {
        NSLog(@"failed to create filelist controller");
    }
    else {
        [sideBar addItem:fl];
        /* [sideBar displayItem:fl]; */
        [fl release];
    }
}

- (void)hubRedirectNotification:(NSNotification *)aNotification
{
    NSString *hubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];
    NSString *newAddress = [[aNotification userInfo] objectForKey:@"newAddress"];

    SPHubController *hubController = [hubs objectForKey:hubAddress];
    if (hubController) {
        [hubs removeObjectForKey:hubAddress];
        [hubs setObject:hubController forKey:newAddress];
        [self syncHubsToDisk];
    }
}

- (void)hubAddNotification:(NSNotification *)aNotification
{
    NSString *hubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];

    /* try to find a matching hub already in the sidebar */
    SPHubController *hubController = nil;
    NSArray *hubsArray = [sideBar itemsInSection:@"Hubs"]; /* TODO: oh no! */
    NSEnumerator *e = [hubsArray objectEnumerator];
    SPHubController *hub;
    while ((hub = [e nextObject]) != nil) {
        if ([[hub address] isEqualToString:hubAddress]) {
            hubController = hub;
            break;
        }
    }

    /* none found, create a new one */
    if (hubController == nil) {
        hubController = [[SPHubController alloc] initWithAddress:hubAddress
                                                            nick:[[aNotification userInfo] objectForKey:@"nick"]];
        [hubController setDescriptionString:[[aNotification userInfo] objectForKey:@"description"]];
        [hubController setName:[[aNotification userInfo] objectForKey:@"name"]];
        [hubController setEncoding:[[aNotification userInfo] objectForKey:@"encoding"]];
        [sideBar addItem:hubController];
        /* [sideBar displayItem:hubController]; */
        [hubs setObject:hubController forKey:hubAddress];
        [hubController release];
    }
    else {
        [hubController setConnected];
    }
    
    // if we're reconnecting to the last connected hubs, we don't want to sync our half-restored
    // session to disk.
    if (!restoringLastConnectedHubs)
        [self syncHubsToDisk];
}

- (void)hubDisconnectedNotification:(NSNotification *)aNotification
{
    NSString *hubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];

    NSString *msg = [NSString stringWithFormat:@"Disconnected from hub %@", hubAddress];
    [[SPGrowlBridge sharedGrowlBridge] notifyWithName:SP_GROWL_HUB_CONNECTION_CLOSED
                                          description:msg];

    [self syncHubsToDisk];
}

- (void)downloadFinishedNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];

    NSString *msg = [NSString stringWithFormat:@"Finished downloading %@", targetFilename];
    [[SPGrowlBridge sharedGrowlBridge] notifyWithName:SP_GROWL_DOWNLOAD_FINISHED
                                          description:msg];
}

#pragma mark -
#pragma mark Connect sheet window

- (void)connectSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction)connectShow:(id)sender
{
    [NSApp beginSheet:connectWindow
       modalForWindow:[self window]
        modalDelegate:self
       didEndSelector:@selector(connectSheetDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
    
    // give visual hint that default nickname is used if the user doesn't specify one in this dialogue
    [[connectNickname cell] setPlaceholderString:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname]];
    
    // last filled in info should still be filled in, but let's select the address field
    [connectAddress selectText:self];
}

- (IBAction)connectExecute:(id)sender
{
    [NSApp endSheet:connectWindow];
    
    NSString *address = [connectAddress stringValue];
    
    // TODO: This check can be removed once the connectWithAddress method has all these checks
    if (address && [address length]) {
        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:[connectNickname stringValue]
                                                                      description:nil
                                                                         password:[connectPassword stringValue]
                                                                         encoding:nil];
    }
}

- (IBAction)connectCancel:(id)sender
{
    [NSApp endSheet:connectWindow];
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)openHublist:(id)sender
{
    if (!publicHubsController) {
        publicHubsController = [[SPPublicHubsController alloc] init];
    }
    [sideBar addItem:publicHubsController];
    [sideBar displayItem:publicHubsController];
}

- (IBAction)openFriends:(id)sender
{
    SPFriendsController *friendsController = [SPFriendsController sharedFriendsController];
    [sideBar addItem:friendsController];
    [sideBar displayItem:friendsController];
}

- (IBAction)openBookmarks:(id)sender
{
    bookmarkController = [SPBookmarkController sharedBookmarkController];
    [sideBar addItem:bookmarkController];
    [sideBar displayItem:bookmarkController];
}

- (IBAction)openDownloads:(id)sender
{
    if (!queueController)
        queueController = [SPQueueController sharedQueueController];
    [sideBar addItem:queueController];
    [sideBar displayItem:queueController];
}

- (IBAction)openUploads:(id)sender
{
    if (!transferController) {
        transferController = [[SPTransferController alloc] init];
    }
    [sideBar addItem:transferController];
    [sideBar displayItem:transferController];
}

- (IBAction)toggleTransferDrawer:(id)sender
{
    [transferDrawer toggle:self];
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcUser; break;
        case 1: tc = tcStatus; break;
        case 2: tc = tcTimeLeft; break;
        case 3: tc = tcSpeed; break;
        case 4: tc = tcFilename; break;
        case 5: tc = tcSize; break;
        case 6: tc = tcPath; break;
        case 7: tc = tcHub; break;
    }
    if (tc == nil)
        return;
    
    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [transferDrawerTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [transferDrawerTable removeTableColumn:tc];
    }
}



- (void)performSearchFor:(NSString *)searchString
                    size:(NSString *)searchSize
         sizeRestriction:(int)sizeRestriction
                    type:(int)searchType
              hubAddress:(NSString *)hubAddress
{
    SPSearchWindowController *searchController = [[SPSearchWindowController alloc]
                                                  initWithString:searchString
                                                  size:searchSize
                                                  sizeRestriction:sizeRestriction
                                                  type:searchType
                                                  hubAddress:hubAddress];
    
    [sideBar addItem:searchController];
    [sideBar displayItem:searchController];
    [searchController release];
}

- (void)updateSearchFieldMenu
{
    NSString *placeholderString = [NSString stringWithFormat:@"Search %@",
                               [[searchFieldMenu itemWithTag:currentSearchType] title]];

    [[toolbarSearchField cell] setSearchMenuTemplate:searchFieldMenu];
    [[toolbarSearchField cell] setPlaceholderString:placeholderString];

    [[advSearchField cell] setSearchMenuTemplate:searchFieldMenu];
    [[advSearchField cell] setPlaceholderString:placeholderString];
}

- (IBAction)quickSearchExecute:(id)sender
{
    NSString *searchString = [toolbarSearchField stringValue];

    if ([searchString length]) {
        [self performSearchFor:searchString
                          size:nil
               sizeRestriction:SHARE_SIZE_NONE
                          type:currentSearchType
                    hubAddress:nil];
        [self updateSearchFieldMenu];
    }
}

- (IBAction)setSearchType:(id)sender
{
    currentSearchType = [(NSMenuItem *)sender tag];
    int i;
    for (i = 1; i <= 8; i++)
        [[searchFieldMenu itemWithTag:i] setState:NSOffState];
    [[searchFieldMenu itemWithTag:currentSearchType] setState:NSOnState];

    [self updateSearchFieldMenu];
}

- (void)closeCurrentSidebarItem
{
    [sideBar closeSelectedItem:self];
    currentSidebarItem = nil;
}

- (void)prevSidebarItem
{
    [sideBar selectPreviousItem];
}

- (void)nextSidebarItem
{
    [sideBar selectNextItem];
}

#pragma mark -
#pragma mark NSSplitView delegates

- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedMin
       ofSubviewAt:(int)offset
{
    if (sender == sidebarSplitView)
        return SIDEBAR_MIN_SIZE;
    return proposedMin;
}

- (float)splitView:(NSSplitView *)sender constrainMaxCoordinate:(float)proposedMax
       ofSubviewAt:(int)offset
{
    if (sender == sidebarSplitView)
        return SIDEBAR_MAX_SIZE;
    return proposedMax;
}

- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
    return NO;
}

- (void)splitView:(id)sender resizeSubviewsWithOldSize:(NSSize)oldSize
{
    NSRect newFrame = [sender frame];
    float dividerThickness = [sender dividerThickness];

    NSView *firstView = [[sender subviews] objectAtIndex:0];
    NSView *secondView = [[sender subviews] objectAtIndex:1];

    NSRect firstFrame = [firstView frame];
    NSRect secondFrame = [secondView frame];

    if (sender == sidebarSplitView) {
        /* keep sidebar in constant width */
        secondFrame.size.width = newFrame.size.width - (firstFrame.size.width + dividerThickness);
        secondFrame.size.height = newFrame.size.height;
    }

    [secondView setFrame:secondFrame];
    [sender adjustSubviews];
}

#pragma mark -
#pragma mark SPSideBar delegates

- (void)sideBar:(SPSideBar *)sideBar didDeselectItem:(id)sideBarItem
{
    // notify the previously selected item that it is
    // being deselected, this is useful for invalidating timers etc.
    if ([sideBarItem respondsToSelector:@selector(viewBecameDeselected)])
        [sideBarItem performSelector:@selector(viewBecameDeselected)];
}

- (void)sideBar:(SPSideBar *)sideBar didSelectItem:(id)sideBarItem
{
    currentSidebarItem = sideBarItem;
    [[self window] setTitle:[NSString stringWithFormat:@"ShakesPeer - %@", [sideBarItem title]]];
    
    // TODO: The following should be the canonical method, and each sidebar item
    // should be responsible for the proper actions (like focusChatInput etc.)
    if ([sideBarItem respondsToSelector:@selector(viewBecameSelected)])
        [sideBarItem performSelector:@selector(viewBecameSelected)];
    
    if ([sideBarItem respondsToSelector:@selector(focusChatInput)])
        [sideBarItem focusChatInput];
    
    [[NSUserDefaults standardUserDefaults] setObject:[sideBarItem title] forKey:@"lastSidebarItem"];
}

- (void)sideBar:(SPSideBar *)aSideBar willCloseItem:(id)sideBarItem
{
    if ([sideBarItem isKindOfClass:[SPHubController class]]) {
        [hubs removeObjectForKey:[(SPHubController *)sideBarItem address]];
    }
}

- (void)sideBar:(SPSideBar *)aSideBar didCloseItem:(id)sideBarItem
{
    if ([sideBarItem respondsToSelector:@selector(sectionTitle)]) {
        NSString *sectionTitle = [sideBarItem sectionTitle];
        if ([sideBar numberOfItemsInSection:sectionTitle] == 0)
            [sideBar removeItem:sectionTitle];
    }
}

#pragma mark -
#pragma mark Transfer drawer delegates

- (NSSize)drawerWillResizeContents:(NSDrawer *)transferDrawer toSize:(NSSize)contentSize
{
    [[NSUserDefaults standardUserDefaults] setInteger:contentSize.height forKey:@"drawerHeight"];

    return contentSize;
}

#pragma mark -
#pragma mark Advanced Search Sheet
- (void)advSearchSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
    [self updateSearchFieldMenu];
    [toolbarSearchField setNeedsDisplay];
}

- (IBAction)advSearchShow:(id)sender
{
    /* fill in the connected hubs in the hubs popup menu */
    /* first remove all hub items (except the first "All hubs" item) */
    while ([advSearchHubs numberOfItems] > 1) {
        [advSearchHubs removeItemAtIndex:1];
    }
    NSArray *hubsArray = [sideBar itemsInSection:@"Hubs"]; /* TODO: oh no! */
    NSMutableArray *hubTitlesArray = [NSMutableArray arrayWithCapacity:[hubsArray count]];
    NSEnumerator *e = [hubsArray objectEnumerator];
    SPHubController *hub;
    while ((hub = [e nextObject]) != nil) {
        [hubTitlesArray addObject:[hub address]];
    }
    [advSearchHubs addItemsWithTitles:hubTitlesArray];

    [NSApp beginSheet:advSearchWindow
       modalForWindow:[self window]
        modalDelegate:self
       didEndSelector:@selector(advSearchSheetDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
}

- (IBAction)advSearchExecute:(id)sender
{
    NSString *searchString = [advSearchField stringValue];
    if ([searchString length] == 0)
        return;
    
    [NSApp endSheet:advSearchWindow];

    if (sender != advSearchField) {
        /* Need to manually add the search string to the recent searches menu
         * of the search field(s) */
        NSArray *recentSearches = [advSearchField recentSearches];
        if ([recentSearches containsObject:searchString] == NO) {
            NSArray *newRecentSearches = [NSArray arrayWithObject:searchString];
            NSArray *mergedRecentSearches = [newRecentSearches arrayByAddingObjectsFromArray:recentSearches];
            [advSearchField setRecentSearches:mergedRecentSearches];
            [toolbarSearchField setRecentSearches:mergedRecentSearches];
        }
        /* TODO: otherwise we should move it to the top */
    }

    share_size_restriction_t sizeRestriction = SHARE_SIZE_NONE;
    NSString *searchSizeString = [advSearchSize stringValue];

    if ([searchSizeString length]) {
        /* convert the string to an integer and apply the suffix
        */
        uint64_t searchSize = [searchSizeString unsignedLongLongValue];
        int suffix = [advSearchSizeSuffix indexOfSelectedItem];
        if (suffix < 0)
            suffix = 0;
        if (suffix > 0) {
            searchSize = searchSize << (10 * suffix);
        }

        /* convert it back to a string */
        NSNumber *searchSizeNumber = [NSNumber numberWithUnsignedLongLong:searchSize];
        searchSizeString = [searchSizeNumber stringValue];

        sizeRestriction = [advSearchSizeType indexOfSelectedItem] + 1;
    }

    NSString *hubAddress = nil;
    if ([advSearchHubs indexOfSelectedItem] > 0) {
        hubAddress = [advSearchHubs titleOfSelectedItem];
    }

    [self performSearchFor:searchString
                      size:searchSizeString
           sizeRestriction:sizeRestriction
                      type:currentSearchType
                hubAddress:hubAddress];

}

- (IBAction)advSearchCancel:(id)sender
{
    [NSApp endSheet:advSearchWindow];
}

#pragma mark -
#pragma mark Transfer drawer menu

- (IBAction)cancelTransfer:(id)sender
{
    [transferController cancelTransfersInArray:[transferArrayController selectedObjects]];
}

- (IBAction)removeSource:(id)sender
{
    [transferController removeSourcesInArray:[transferArrayController selectedObjects]];
}

- (IBAction)removeQueue:(id)sender
{
    [transferController removeQueuesInArray:[transferArrayController selectedObjects]];
}

- (IBAction)removeAllSourcesWithNick:(id)sender
{
    [transferController removeAllSourcesWithNicksInArray:[transferArrayController selectedObjects]];
}

- (IBAction)browseUser:(id)sender
{
    [transferController browseUserInArray:[transferArrayController selectedObjects]];
}

- (IBAction)privateMessage:(id)sender
{
    [transferController privateMessageInArray:[transferArrayController selectedObjects]];
}


#pragma mark -
#pragma mark UserCommand parameter window

- (NSString *)requestUserCommandParameter:(NSString *)paramName title:(NSString *)title
{
    [UCParamCommandTitle setStringValue:title];
    [UCParamName setStringValue:[NSString stringWithFormat:@"Enter %@:", paramName]];

    [NSApp beginSheet:UCParamWindow
           modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
           modalDelegate:nil
           didEndSelector:nil
           contextInfo:nil];

    int rc = [NSApp runModalForWindow:UCParamWindow];

    [NSApp endSheet:UCParamWindow];
    [UCParamWindow orderOut:self];

    return rc == 0 ? [UCParamTextField stringValue] : nil;
}

- (IBAction)acceptUCParam:(id)sender
{
    [NSApp stopModalWithCode:0];
}

- (IBAction)cancelUCParam:(id)sender
{
    [NSApp stopModalWithCode:1];
}

@end
