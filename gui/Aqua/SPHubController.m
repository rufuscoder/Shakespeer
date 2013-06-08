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

#import "SPHubController.h"
#import "SPMainWindowController.h"
#import "SPApplicationController.h"
#import "SPBookmarkController.h"
#import "SPFriendsController.h"
#import "SPUser.h"
#import "SPUserCommand.h"
#import "SPTransformers.h"
#import "SPLog.h"
#import "URLMutableAttributedString.h"
#import "NSMutableAttributedString-SmileyAdditions.h"
#import "SPPreferenceController.h"
#import "NSMenu-UserCommandAdditions.h"
#import "SPGrowlBridge.h"
#import "SPSideBar.h"
#import "NSTextView-ChatFormattingAdditions.h"
#import "NSStringExtensions.h"

#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

#include "util.h"

@interface SPHubController (Private)
 - (void)filterUsersWithString:(NSString *)filter;
 - (NSArray *)usersWithFilter:(NSString *)filter startsWithSearchOnly:(BOOL)startsWithSearch stringArray:(BOOL)stringArray;
 - (void)ensureUpdated;
@end

@implementation SPHubController

- (id)initWithAddress:(NSString *)anAddress nick:(NSString *)aNick
{
    self = [super initWithWindowNibName:@"Hub"];
    if (self) {
        usersTree = [MHSysTree new];
        filteredUsers = [NSMutableArray new];
        nops = 0;
        totsize = 0ULL;
        needUpdating = NO;
        address = [anAddress retain];
        name = [anAddress retain];
        nick = [aNick retain];
        [self setConnected];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userUpdateNotification:)
                                                     name:SPNotificationUserUpdate
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLoginNotification:)
                                                     name:SPNotificationUserLogin
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLogoutNotification:)
                                                     name:SPNotificationUserLogout
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(publicMessageNotification:)
                                                     name:SPNotificationPublicMessage
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubRedirectNotification:)
                                                     name:SPNotificationHubRedirect
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userCommandNotification:)
                                                     name:SPNotificationUserCommand
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubnameChangedNotification:)
                                                     name:SPNotificationHubnameChanged
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubDisconnectedNotification:)
                                                     name:SPNotificationHubDisconnected
                                                   object:nil];
    }

    return self;
}

- (void)awakeFromNib 
{
    [userTable setTarget:self];
    [userTable setDoubleAction:@selector(browseUser:)];

    [tcNick retain];
    [tcShare retain];
    [tcTag retain];
    [tcSpeed retain];
    [tcDescription retain];
    [tcEmail retain];
    [tcIcon retain];

    NSArray *tcs = [userTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];

        if (tc == tcNick)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcShare)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcTag)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcSpeed)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcDescription)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcEmail)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcIcon)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
    }
    
    updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                   target:self
                                                 selector:@selector(updateUserTable:)
                                                 userInfo:nil
                                                  repeats:YES];

    [[userTable headerView] setMenu:columnsMenu];

    numStaticNickMenuEntries = [nickMenu numberOfItems];
    [[[chatView enclosingScrollView] verticalScroller] setFloatValue:1.0];
}

- (NSArray *)usersWithFilter:(NSString *)filter /* Substring to search for. */
        startsWithSearchOnly:(BOOL)startsWithSearch /* Search for 'filter' only in beginning of nick? */
                 stringArray:(BOOL)returnStringArray /* Decides whether to return an array of SPNicks or NSStrings. The array is unsorted. */
{ 
    NSArray *usersTreeSnapshot = [usersTree allObjects];
    if ([filter isEqualToString:@""])
        return usersTreeSnapshot;

    NSArray *filterCriteria = nil;
    if (!startsWithSearch) {
        // for AND-searches, split all substrings up as the search criteria.
        filterCriteria = [filter componentsSeparatedByString:@" "];
    }
    
    NSMutableArray *foundUsers = [NSMutableArray arrayWithCapacity:[usersTreeSnapshot count]];
    NSEnumerator *e = [usersTreeSnapshot objectEnumerator];
    SPUser *user;
    while ((user = [e nextObject])) {
        BOOL add = YES;
        
        if (startsWithSearch) {
            if ([[user nick] rangeOfString:filter options:(NSAnchoredSearch | NSCaseInsensitiveSearch)].location == NSNotFound) {
                add = NO;
            }
        } 
        else {
            // AND-search 
            NSEnumerator *f = [filterCriteria objectEnumerator];
            NSString *criterion;
            while ((criterion = [f nextObject])) {
                if ([criterion length] > 0 &&
                   [[user nick] rangeOfString:criterion options:NSCaseInsensitiveSearch].location == NSNotFound) {
                    add = NO;
                    break;
                }
            }
        }
        
        if (add) {
            // Add either only the nick, or the SPUser, depending on preferred return value.
            if (returnStringArray)
                [foundUsers addObject:[user nick]];
            else
                [foundUsers addObject:user];
        }
    }
    return foundUsers;
}

- (void)filterUsersWithString:(NSString *)newFilter
{
    [filteredUsers removeAllObjects];
    [filteredUsers addObjectsFromArray:[self usersWithFilter:newFilter startsWithSearchOnly:NO stringArray:NO]];
}

- (void)ensureUpdated
{
    [self updateUserTable:nil];
}

- (void)updateUserTable:(NSTimer *)aTimer
{
    if (needUpdating && isShowing) {
        needUpdating = NO;
        // update filteredUsers and table view, and obey any current filter
        [self filterUsersWithString:[nickFilter stringValue]];
        [userTable reloadData];
        // update some stats
        [hubStatisticsField setStringValue:[NSString stringWithFormat:@"%lu users, %u ops, %s",
            [usersTree count], nops, str_size_human(totsize)]];
    }
}

- (void)dealloc 
{
    [[SPApplicationController sharedApplicationController] disconnectFromAddress:address];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [usersTree release];
    [filteredUsers release];
    [address release];
    [name release];
    [nick release];
    [encoding release];
    [descriptionString release];
    [nickAutocompleteEnumerator release];
    [savedNickStart release];
    
    [tcNick release];
    [tcShare release];
    [tcTag release];
    [tcSpeed release];
    [tcDescription release];
    [tcEmail release];
    [tcIcon release];
    
    [super dealloc];
}

- (NSString *)address
{
    return address;
}

- (NSString *)nick
{
    return nick;
}

- (void)setName:(NSString *)aName
{
    if (name != aName) {
        [name release];
        name = [aName retain];
    }
}

- (void)setDescriptionString:(NSString *)aDescription
{
    if (aDescription != descriptionString) {
        [descriptionString release];
        descriptionString = [aDescription retain];
    }
}

- (void)setConnected
{
    disconnected = FALSE;
}

- (void)setEncoding:(NSString *)anEncoding
{
    if (encoding != anEncoding) {
        [encoding release];
        encoding = [anEncoding retain];
    }
}

#pragma mark -
#pragma mark Sidebar support

- (void)unbindControllers
{
    [updateTimer invalidate];
}

- (NSView *)view
{
    return [[self window] contentView];
}

- (BOOL)canClose
{
    return YES;
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"TableHub"];
}

- (NSString *)title
{
    return [self name];
}

- (NSString *)name
{
    return name;
}

- (NSString *)sectionTitle
{
    return @"Hubs";
}

- (BOOL)isHighlighted
{
    return highlighted;
}

- (void)setHighlighted:(BOOL)aFlag
{
    highlighted = aFlag;
}

- (NSMenu *)menu
{
    return nickMenu;
}

- (void)viewBecameSelected
{
    isShowing = YES;
    [self ensureUpdated];
}

- (void)viewBecameDeselected
{
    isShowing = NO;
}


#pragma mark -

#pragma mark Chat wrappers

// These two functions are a way (probably not the only way, nor the best way) to solve the problem of 
// having the last line in each of them duplicated in a half-dozen places within this file.
-(void)addPublicMessage:(NSString *)aMessage fromNick:(NSString *)aNick
{
    [chatView addPublicMessage:aMessage
                      fromNick:aNick
                        myNick:nick];
    [[SPMainWindowController sharedMainWindowController] highlightItem:self];
}

- (void)addStatusMessage:(NSString *)aMessage
{
    [chatView addStatusMessage:aMessage];
    [[SPMainWindowController sharedMainWindowController] highlightItem:self];
}

#pragma mark Sphubd notifications

- (SPUser *)findUserWithNick:(NSString *)aNick
{
    SPUser *cmpUser = [SPUser userWithNick:aNick isOperator:NO];
    SPUser *foundUser = [usersTree find:cmpUser];
    if (!foundUser) {
        // no user with the above criteria. let's see if there's an op
        // with the same nick.
        [cmpUser setIsOperator:YES];
        foundUser = [usersTree find:cmpUser];
    }
    return foundUser;
}

- (void)userLoginNotification:(NSNotification *)aNotification
{
    NSDictionary *userinfo = [aNotification userInfo];
    if ([[userinfo objectForKey:@"hubAddress"] isEqualToString:address]) {
        BOOL isOp = [[userinfo objectForKey:@"isOperator"] boolValue];
        NSString *theNick = [userinfo objectForKey:@"nick"];
        SPUser *user = [SPUser userWithNick:theNick
                                description:[userinfo objectForKey:@"description"]
                                        tag:[userinfo objectForKey:@"tag"]
                                      speed:[userinfo objectForKey:@"speed"]
                                      email:[userinfo objectForKey:@"email"]
                                       size:[userinfo objectForKey:@"size"]
                                 isOperator:isOp
                                 extraSlots:[[userinfo objectForKey:@"extraSlots"] unsignedIntValue]];

        [usersTree addObject:user];
        needUpdating = YES;

        totsize += [[userinfo objectForKey:@"size"] unsignedLongLongValue];
        if (isOp)
            nops++;
    }
}

- (void)userUpdateNotification:(NSNotification *)aNotification
{
    NSDictionary *userinfo = [aNotification userInfo];
    if ([[userinfo objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *theNick = [userinfo objectForKey:@"nick"];
        SPUser *user = [self findUserWithNick:theNick];
        if (!user) {
            // for ops, we get a user-update notification before we even know they exist.
            [self userLoginNotification:aNotification];
        }
        else {
            totsize -= [user size];
            BOOL oldOperatorFlag = [user isOperator];
            BOOL newOperatorFlag = [[userinfo objectForKey:@"isOperator"] boolValue];

            if (oldOperatorFlag)
                nops--;

            if (oldOperatorFlag != newOperatorFlag) {
                /* Ouch, operator status changed, which affects sort ordering. */
                /* Remove and re-insert the entry. */
                [user retain];
                [usersTree removeObject:user];
                [user setIsOperator:newOperatorFlag];
                [usersTree addObject:user];
                [user release];
            }

            /* Now update the attributes. */

            [user setDescription:[userinfo objectForKey:@"description"]];
            [user setTag:[userinfo objectForKey:@"tag"]];
            [user setSpeed:[userinfo objectForKey:@"speed"]];
            [user setEmail:[userinfo objectForKey:@"email"]];
            [user setSize:[userinfo objectForKey:@"size"]];
            [user setExtraSlots:[[userinfo objectForKey:@"extraSlots"] unsignedIntValue]];

            totsize += [user size];
            if (newOperatorFlag)
                nops++;

            needUpdating = YES;
        }
    }
}

- (void)userLogoutNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *theNick = [[aNotification userInfo] objectForKey:@"nick"];
        SPUser *user = [self findUserWithNick:theNick];
        if (user) {
            totsize -= [user size];
            if ([user isOperator])
                nops--;

            [usersTree removeObject:user];
        }
        needUpdating = YES;
    }
}

- (void)publicMessageNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *aNick = [[aNotification userInfo] objectForKey:@"nick"];
        NSString *aMessage = [[aNotification userInfo] objectForKey:@"message"];
        [self addPublicMessage:aMessage fromNick:aNick];
    }
}

- (void)hubRedirectNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        [self willChangeValueForKey:@"title"];
        [address release];
        address = [[[aNotification userInfo] objectForKey:@"newAddress"] retain];
        [self didChangeValueForKey:@"title"];
        [self setName:address];
        [self addStatusMessage:[NSString stringWithFormat:@"Redirected to hub %@\n", address]];
    }
}

- (void)executeHubUserCommand:(id)sender
{
    NSArray *nicks = nil;

    int row = [userTable selectedRow];
    if (row != -1) {
        SPUser *user = [filteredUsers objectAtIndex:row];

        NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithCapacity:1];
        [parameters setObject:[user nick] forKey:@"nick"];

        nicks = [NSArray arrayWithObject:parameters];
    }

    [[sender representedObject] executeForNicks:nicks myNick:nick];
}

- (void)userCommandNotification:(NSNotification *)aNotification
{
    if ([address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        int context = [[[aNotification userInfo] objectForKey:@"context"] intValue];
        if ((context & 3) > 0) /* context 1 (hub) or 2 (user) */
        {
            int type = [[[aNotification userInfo] objectForKey:@"type"] intValue];
            NSString *title = [[aNotification userInfo] objectForKey:@"description"];
            NSString *command = [[aNotification userInfo] objectForKey:@"command"];

            SPUserCommand *uc = [[SPUserCommand alloc] initWithTitle:title
                                                             command:command
                                                                type:type
                                                             context:context
                                                                 hub:address];

            [nickMenu addUserCommand:[uc autorelease]
                              action:@selector(executeHubUserCommand:)
                              target:self
                       staticEntries:numStaticNickMenuEntries];
        }
    }
}

- (void)hubnameChangedNotification:(NSNotification *)aNotification
{
    if ([address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        [self setName:[[aNotification userInfo] objectForKey:@"newHubname"]];
    }
}

- (void)hubDisconnectedNotification:(NSNotification *)aNotification
{
    if (!disconnected &&
       [address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        disconnected = YES;
        [usersTree removeAllObjects];
        needUpdating = YES;
        nops = 0;
        totsize = 0ULL;
        [self addStatusMessage:@"Disconnected from hub!\n"];
    }
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)grantExtraSlot:(id)sender
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        [[SPApplicationController sharedApplicationController] grantExtraSlotToNick:[user nick]];
    }
}

- (IBAction)bookmarkHub:(id)sender
{
    NSDictionary *existingBookmark = [[SPBookmarkController sharedBookmarkController] bookmarkForHub:address];
    if (existingBookmark != nil) 
        [self addStatusMessage:[NSString stringWithFormat:@"Hub %@ already added to bookmarks\n", address]];
    else {
        BOOL result = [[SPBookmarkController sharedBookmarkController] addBookmarkWithName:address
                                                                                   address:address
                                                                                      nick:nick
                                                                                  password:@""
                                                                               description:descriptionString
                                                                               autoconnect:NO
                                                                                  encoding:nil];
        if (result)
            [self addStatusMessage:[NSString stringWithFormat:@"Hub %@ added to bookmarks\n", address]];
    }
}

- (IBAction)sendMessage:(id)sender
{
    NSMutableString *message = [[[sender stringValue] mutableCopy] autorelease];

    if ([message hasPrefix:@"/"] && ![message hasPrefix:@"/me "]) {
        /* this is a command */
        NSArray *args = [message componentsSeparatedByString:@" "];
        NSString *cmd = [args objectAtIndex:0];
        
        // COMMAND: /fav
        if ([cmd isEqualToString:@"/fav"] || [cmd isEqualToString:@"/favorite"]) {
            [self bookmarkHub:self];
        }
        
        // COMMAND: /clear
        else if ([cmd isEqualToString:@"/clear"]) {
            [chatView clear];
        }
        
        // COMMAND: /help
        else if ([cmd isEqualToString:@"/help"]) {
            [self addStatusMessage:@"Available commands:\n"
              "  /fav or /favorite: add this hub as a bookmark\n"
              "  /clear: clear the chat window\n"
              "  /pm <nick>: start a private chat with <nick>\n"
              "  /refresh: rescan shared files\n"
              "  /join <address>: connect to a hub\n"
              "  /search <keywords>: search for keywords\n"
              "  /userlist <nick>: load nicks filelist\n"
              "  /reconnect: reconnect to disconnected hub\n"
              "  /np: show current track in iTunes\n"];
        }
        
        // COMMAND: /pm
        else if ([cmd isEqualToString:@"/pm"]) {
            if ([args count] == 2) {
                sendNotification(SPNotificationStartChat,
                        @"remote_nick", [args objectAtIndex:1],
                        @"hubAddress", address,
                        @"my_nick", nick,
                        nil);
            }
            else
                [self addStatusMessage:@"No nick specified\n"];
        }
        
        // COMMAND: /refresh
        else if ([cmd isEqualToString:@"/refresh"]) {
            [[SPPreferenceController sharedPreferences] updateAllSharedPaths];
        }
        
        // COMMAND: /join
        else if ([cmd isEqualToString:@"/join"]) {
            if ([args count] >= 2) {
                [[SPApplicationController sharedApplicationController] connectWithAddress:[args objectAtIndex:1]
                                                                                     nick:nil
                                                                              description:nil
                                                                                 password:nil
                                                                                 encoding:nil];
            }
            else
                [self addStatusMessage:@"No hub address specified\n"];
        }
        
        // COMMAND: /search
        else if ([cmd isEqualToString:@"/search"]) {
            if ([args count] > 1) {
                [[SPMainWindowController sharedMainWindowController]
                    performSearchFor:[message substringFromIndex:8]
                                size:nil
                     sizeRestriction:SHARE_SIZE_NONE
                                type:SHARE_TYPE_ANY
                          hubAddress:address];
            }
            else
            {
                [self addStatusMessage:@"No search keywords specified\n"];
            }
        }
        
        // COMMAND: /userlist
        else if ([cmd isEqualToString:@"/userlist"]) {
            if ([args count] >= 2) {
                [[SPApplicationController sharedApplicationController]
                    downloadFilelistFromUser:[args objectAtIndex:1]
                                       onHub:address
                                 forceUpdate:NO
                                   autoMatch:NO];
            }
            else
            {
                [self addStatusMessage:@"No nick specified\n"];
            }
        }
        
        // COMMAND: /reconnect
        else if ([cmd isEqualToString:@"/reconnect"]) {
            if (disconnected == NO) {
                [self addStatusMessage:@"Still connected\n"];
            }
            else
            {
                [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                                     nick:nick
                                                                              description:descriptionString
                                                                                 password:nil /* TODO: can't we store the password? */
                                                                                 encoding:encoding];
            }
        }
        
        // COMMAND: /np
        else if ([cmd isEqualToString:@"/np"]) {
            NSString *theMessage = [NSString stringWithNowPlayingMessage];
            [[SPApplicationController sharedApplicationController] sendPublicMessage:theMessage 
                                                                               toHub:address];
        } // np
        else {
            [self addStatusMessage:[NSString stringWithFormat:@"Unknown command: %@\n", cmd]];
        }
    }
    else {
        if (disconnected) {
            [self addStatusMessage:@"Disconnected from hub, use /reconnect to reconnect\n"];
        }
        else {
            [[SPApplicationController sharedApplicationController] sendPublicMessage:message
                                                                               toHub:address];
        }
    }

    [sender setStringValue:@""];
    [self focusChatInput];
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector
{ 
    // tab invokes nick autocompletion
    if (control == (NSControl*)chatInput && commandSelector == @selector(insertTab:))
    {
        NSString *inputLine = [chatInput stringValue];
        if ([inputLine isEqualToString:@""]) {
            // if the input field is empty, move focus
            return NO;
        }

        NSArray *inputWords = [inputLine componentsSeparatedByString:@" "]; // get all our words
        NSString *word = (NSString*)[inputWords lastObject]; // get the last word
        
        if (!nickAutocompleteEnumerator) {
            NSArray *usersWithFilter = [self usersWithFilter:word 
                                        startsWithSearchOnly:YES 
                                                 stringArray:YES];
            
            if ([usersWithFilter count] == 1 && 
                [word isEqualToString:[usersWithFilter objectAtIndex:0]]) {
                // Special case: The textfield matches exactly one person, whose name
                // is already filled in. This will happen on the end of the list of autocomplete nicks. 
                // Since it's a no-op, don't bother getting the enumerator and replacing the contents
                // of the textfield (with the same value).
                ;
            }
            else {
                // Hold on to the filtered list of nicks, so we can continue enumerating later.
                nickAutocompleteEnumerator = [[usersWithFilter objectEnumerator] retain];
                // save the start of the nick we've typed in
                savedNickStart = [word retain];
            }
        }
        
        if (nickAutocompleteEnumerator) {
            NSString *nextNick = [nickAutocompleteEnumerator nextObject];
            if (nextNick) {
                // now we rebuild the string and set it
                [chatInput setStringValue:[NSString stringWithFormat:@"%@%@", [inputLine substringToIndex:[inputLine length] - [word length]], nextNick]];
            }
            else {
                [nickAutocompleteEnumerator release];
                nickAutocompleteEnumerator = nil;
            }
        }
        
        if (!nickAutocompleteEnumerator) {
            // End of session - no more hits so we wrap around to the saved start of a nick
            [chatInput setStringValue:[NSString stringWithFormat:@"%@%@", [inputLine substringToIndex:[inputLine length] - [word length]], savedNickStart]];
            // release the saved nick
            [savedNickStart release];
            savedNickStart = nil;
        }
        
        return YES;
    }
    
    return NO;
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    if ([notification object] == chatInput && nickAutocompleteEnumerator) {
        // user changed text, so clear current autocomplete session
        [nickAutocompleteEnumerator release];
        nickAutocompleteEnumerator = nil;
    }
}

- (void)focusChatInput
{
    /* keep the text field the first responder (ie, give it input focus) */
    [[chatInput window] performSelector:@selector(makeFirstResponder:)
                          withObject:chatInput
                          afterDelay:0];
}

- (void)processFilelist:(id)sender autoMatch:(BOOL)autoMatchFlag
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        [[SPApplicationController sharedApplicationController] downloadFilelistFromUser:[user nick]
                                                                                  onHub:address
                                                                            forceUpdate:NO
                                                                              autoMatch:autoMatchFlag];
    }
}

- (IBAction)browseUser:(id)sender
{
    [self processFilelist:sender autoMatch:NO];
}

- (IBAction)autoMatchFilelist:(id)sender
{
    [self processFilelist:sender autoMatch:YES];
}

- (IBAction)startPrivateChat:(id)sender
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        sendNotification(SPNotificationStartChat,
                @"remote_nick", [user nick],
                @"hubAddress", address,
                @"my_nick", nick,
                nil);
    }
}

- (IBAction)addFriend:(id)sender
{
    int clickedRow = [userTable selectedRow];
    if (clickedRow == -1)
        return;
    
    SPUser *clickedUser = [filteredUsers objectAtIndex:clickedRow];
    if (clickedUser) {
        NSString *newFriendNick = [clickedUser nick];
        [[SPFriendsController sharedFriendsController] addFriendWithName:newFriendNick comments:@""];
    }
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcNick; break;
        case 1: tc = tcShare; break;
        case 2: tc = tcTag; break;
        case 3: tc = tcSpeed; break;
        case 4: tc = tcDescription; break;
        case 5: tc = tcEmail; break;
        case 6: tc = tcIcon; break;
    }
    if (tc == nil)
        return;

    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [userTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [userTable removeTableColumn:tc];
    }
}

- (IBAction)filter:(id)sender
{
    [self filterUsersWithString:[sender stringValue]];
    [userTable reloadData];
}

#pragma mark -
#pragma mark NSSplitView delegates

- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedMin
       ofSubviewAt:(int)offset
{
    return 100;
}

- (float)splitView:(NSSplitView *)sender constrainMaxCoordinate:(float)proposedMax
       ofSubviewAt:(int)offset
{
    return proposedMax - 100;
}

- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
    return YES;
}

- (void)splitView:(id)sender resizeSubviewsWithOldSize:(NSSize)oldSize
{
    NSRect newFrame = [sender frame];
    float dividerThickness = [sender dividerThickness];

    NSView *firstView = [[sender subviews] objectAtIndex:0];
    NSView *secondView = [[sender subviews] objectAtIndex:1];

    NSRect firstFrame = [firstView frame];
    NSRect secondFrame = [secondView frame];

    /* keep nick list in constant width */
    firstFrame.size.width = newFrame.size.width - (secondFrame.size.width + dividerThickness);
    firstFrame.size.height = newFrame.size.height;

    if (firstFrame.size.width < 0) {
        firstFrame.size.width = 0;
        secondFrame.size.width = newFrame.size.width - firstFrame.size.width - dividerThickness;
    }

    secondFrame.origin.x = firstFrame.size.width + dividerThickness;

    [firstView setFrame:firstFrame];
    [secondView setFrame:secondFrame];
    [sender adjustSubviews];
}

#pragma mark -
#pragma mark NSTableView data source

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [filteredUsers count];
}

- (BOOL)tableView:(NSTableView *)aTableView shouldSelectTableColumn:(NSTableColumn *)aTableColumn
{
    // never allow column selection, since our data source (MHSysTree) compares users by using
    // compare: on every object, which doesn't care about our sort descriptors.
    return NO;
}

- (id)tableView:(NSTableView *)aTableView
 objectValueForTableColumn:(NSTableColumn *)aTableColumn
            row:(int)rowIndex
{
    SPUser *user = [filteredUsers objectAtIndex:rowIndex];

    if (user) {
        NSString *identifier = [aTableColumn identifier];
        if ([identifier isEqualToString:@"nick"]) {
            return [user displayNick];
        }
        else if ([identifier isEqualToString:@"size"]) {
            return [NSString stringWithUTF8String:str_size_human([user size])];
        }
        else if ([identifier isEqualToString:@"tag"]) {
            return [user tag];
        }
        else if ([identifier isEqualToString:@"speed"]) {
            return [user speed];
        }
        else if ([identifier isEqualToString:@"email"]) {
            return [user email];
        }
        else if ([identifier isEqualToString:@"descriptionString"]) {
            return [user descriptionString];
        }
        else if ([identifier isEqualToString:@"icon"]) {
            return [[NickImageTransformer defaultNickImageTransformer] transformedValue:user];
        }
    }

    return nil;
}

@end

