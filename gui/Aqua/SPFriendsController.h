/* vim: ft=objc
 *
 * Copyright 2008 Markus Amalthea Magnuson <markus.magnuson@gmail.com>
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

@interface SPFriendsController : NSObject <SPSideBarItem>
{
	NSMutableArray *friends;

	IBOutlet NSArrayController *friendsController;
    IBOutlet NSView *friendsView;
    IBOutlet NSTableView *friendsTable;
	IBOutlet NSMenu *friendMenu;

	IBOutlet NSWindow *newFriendSheet;
    IBOutlet NSTextField *newFriendNameField;
    IBOutlet NSTextField *newFriendCommentsField;

    IBOutlet NSWindow *editFriendSheet;
    IBOutlet NSTextField *editFriendNameField;
    IBOutlet NSTextField *editFriendCommentsField;
}

+ (SPFriendsController *)sharedFriendsController;

- (void)setFriends:(NSArray *)newFriends;
- (void)addFriendWithName:(NSString *)name comments:(NSString *)comments;
- (void)updateFriendTable;

- (void)userUpdateNotification:(NSNotification *)aNotification;
- (void)userLogoutNotification:(NSNotification *)aNotification;
- (void)applicationWillTerminate;

- (IBAction)browseUser:(id)sender;
- (IBAction)sendPrivateMessage:(id)sender;

- (void)newFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)newFriendShow:(id)sender;
- (IBAction)newFriendExecute:(id)sender;
- (IBAction)newFriendCancel:(id)sender;

- (void)editFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)editFriendShow:(id)sender;
- (IBAction)editFriendExecute:(id)sender;
- (IBAction)editFriendCancel:(id)sender;

- (void)removeFriendShow:(id)sender;
- (void)removeFriendSheetDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end
