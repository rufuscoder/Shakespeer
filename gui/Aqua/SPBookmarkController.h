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

@interface SPBookmarkController : NSObject <SPSideBarItem>
{
    IBOutlet NSView *bookmarkView;
    IBOutlet NSArrayController *arrayController;
    IBOutlet NSTableView *bookmarkTable;
    IBOutlet NSMenu *bookmarkMenu;
    
    IBOutlet NSWindow *newBookmark;
    IBOutlet NSTextField *newBookmarkName;
    IBOutlet NSTextField *newBookmarkAddress;
    IBOutlet NSTextField *newBookmarkNickname;
    IBOutlet NSTextField *newBookmarkPassword;
    IBOutlet NSTextField *newBookmarkDescription;
    IBOutlet NSButton *newBookmarkAutoconnect;
    IBOutlet NSPopUpButton *newBookmarkEncoding;
    
    IBOutlet NSWindow *editBookmark;
    IBOutlet NSTextField *editBookmarkName;
    IBOutlet NSTextField *editBookmarkAddress;
    IBOutlet NSTextField *editBookmarkNickname;
    IBOutlet NSTextField *editBookmarkPassword;
    IBOutlet NSTextField *editBookmarkDescription;
    IBOutlet NSButton *editBookmarkAutoconnect;
    IBOutlet NSPopUpButton *editBookmarkEncoding;
    
    NSMutableArray *bookmarks;
}

+ (SPBookmarkController *)sharedBookmarkController;

- (void)connectToBookmark:(NSDictionary *)bm;
- (IBAction)connectToSelectedBookmark:(id)sender;

- (void)setBookmarks:(NSArray *)anArray;

// short way to add bookmark
- (BOOL)addBookmarkWithName:(NSString *)name address:(NSString *)address;

// verbose way
- (BOOL)addBookmarkWithName:(NSString *)aName
                    address:(NSString *)anAddress
                       nick:(NSString *)aNick
                   password:(NSString *)aPassword
                description:(NSString *)aDescription
                autoconnect:(BOOL)shouldAutoconnect
                   encoding:(NSString *)anEncoding;

- (void)autoConnectBookmarks;
- (NSString *)passwordForHub:(NSString *)aHubAddress nick:(NSString *)aNick;
- (NSDictionary *)bookmarkForHub:(NSString *)address;

- (NSArray *)allowedEncodings;
- (IBAction)setEncoding:(id)sender;

- (void)newBookmarkSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)newBookmarkShow:(id)sender;
- (IBAction)newBookmarkExecute:(id)sender;
- (IBAction)newBookmarkCancel:(id)sender;

- (void)editBookmarkSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)editBookmarkShow:(id)sender;
- (IBAction)editBookmarkExecute:(id)sender;
- (IBAction)editBookmarkCancel:(id)sender;

- (IBAction)duplicateBookmark:(id)sender;

- (void)removeBookmarkShow:(id)sender;
- (void)removeBookmarkSheetDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end
