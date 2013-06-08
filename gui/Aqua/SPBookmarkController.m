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
#import "SPBookmarkController.h"
#import "SPApplicationController.h"
#import "SPSideBar.h"
#import "SPUserDefaultKeys.h"
#import "SPKeychain.h"

extern NSString* SPPublicHubDataType;

@implementation SPBookmarkController

+ (SPBookmarkController *)sharedBookmarkController
{
    static SPBookmarkController *sharedBookmarkController = nil;
    if (sharedBookmarkController == nil) {
        sharedBookmarkController = [[SPBookmarkController alloc] init];
    }
    
    return sharedBookmarkController;
}

- (id)init
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"Bookmarks" owner:self];
        
        NSArray *defaultBookmarks = [[NSUserDefaults standardUserDefaults] arrayForKey:SPBookmarks];
        [self setBookmarks:defaultBookmarks];
        
        // below is bookmark migration stuff
        NSEnumerator *e = [bookmarks objectEnumerator];
        NSMutableDictionary *bm;
        while ((bm = [e nextObject]) != nil) {
            // add the key "name" for all bookmarks if it doesn't exist
            if ([bm objectForKey:@"name"] == nil)
                [bm setObject:[bm objectForKey:@"address"] forKey:@"name"];
            
            // migrate any passwords to the keychain
            if ([bm objectForKey:@"password"]) {
                NSError *error = nil;
                [SPKeychain setPassword:[bm objectForKey:@"password"]
                              forServer:[bm objectForKey:@"address"]
                                account:[bm objectForKey:@"nick"]
                                  error:&error];
                
                [bm removeObjectForKey:@"password"];
            }
        }
        
        // save bookmarks and explicitly synchronize with .plist
        [[NSUserDefaults standardUserDefaults] setObject:bookmarks forKey:SPBookmarks];
        [[NSUserDefaults standardUserDefaults] synchronize];
    }
    
    return self;
}

- (void)awakeFromNib
{
    [bookmarkTable setTarget:self];
    [bookmarkTable setDoubleAction:@selector(connectToSelectedBookmark:)];
    [bookmarkTable setMenu:bookmarkMenu];
    [bookmarkTable registerForDraggedTypes:[NSArray arrayWithObject:SPPublicHubDataType]];
}

- (void)dealloc
{
    [super dealloc];
}

#pragma mark -
#pragma mark Sidebar support

- (NSView *)view
{
    return bookmarkView;
}

- (NSString *)title
{
    return @"Bookmarks";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"bookmarks"];
}

- (NSMenu *)menu
{
    return nil;
}

- (NSArray *)interestingDropTypes
{
  return [NSArray arrayWithObject:SPPublicHubDataType];
}

#pragma mark -
#pragma mark Interface actions

- (void)connectToBookmark:(NSDictionary *)bm
{
    NSString *address = [bm objectForKey:@"address"];
    if ([address length]) {
        NSNumber *encodingIndex = [bm objectForKey:@"encodingIndex"];
        NSString *encoding = nil;
        if (encodingIndex && [encodingIndex intValue] > 0 && /* 0 = default */
           [encodingIndex intValue] < [[self allowedEncodings] count]) {
            encoding = [[self allowedEncodings] objectAtIndex:[encodingIndex intValue]];
        }

        // get password from keychain
        NSError *error = nil;
        NSString *thePassword = [SPKeychain passwordForServer:[bm objectForKey:@"address"]
                                                      account:[bm objectForKey:@"nick"]
                                                        error:&error];

        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:[bm objectForKey:@"nick"]
                                                                      description:[bm objectForKey:@"descriptionString"]
                                                                         password:thePassword
                                                                         encoding:encoding];
    }
}

- (IBAction)connectToSelectedBookmark:(id)sender
{
    NSArray *selectedObjects = [arrayController selectedObjects];
    if ([selectedObjects count]) {
        NSDictionary *bm = [selectedObjects objectAtIndex:0];
        [self connectToBookmark:bm];
    }
}

- (BOOL)addBookmarkWithName:(NSString *)name address:(NSString *)address
{
    return [self addBookmarkWithName:name
                             address:address 
                                nick:@""
                            password:@""
                         description:@""
                         autoconnect:NO
                            encoding:nil];
}

- (BOOL)addBookmarkWithName:(NSString *)aName
                    address:(NSString *)anAddress
                       nick:(NSString *)aNick
                   password:(NSString *)aPassword
                description:(NSString *)aDescription
                autoconnect:(BOOL)shouldAutoconnect
                   encoding:(NSString *)anEncoding
{
    NSMutableDictionary *bm = [NSMutableDictionary dictionaryWithCapacity:3];
    [bm setObject:aName forKey:@"name"];
    [bm setObject:anAddress forKey:@"address"];
    [bm setObject:aNick forKey:@"nick"];
    [bm setObject:[NSNumber numberWithBool:shouldAutoconnect] forKey:@"autoConnect"];
    [bm setObject:aDescription forKey:@"descriptionString"];
    
    // get encoding index for encoding string
    NSNumber *encodingIndex = [NSNumber numberWithInt:[[self allowedEncodings] indexOfObject:anEncoding]];
    if ([encodingIndex intValue] < 0 || [encodingIndex intValue] >= [[self allowedEncodings] count])
        [bm setObject:[NSNumber numberWithInt:0] forKey:@"encodingIndex"];
    else
        [bm setObject:encodingIndex forKey:@"encodingIndex"];

    [self setBookmarks:[bookmarks arrayByAddingObject:bm]];
    
    // if a password was supplied, add it to the keychain
    if ([aPassword isEqualToString:@""] == NO) {
        NSError *error = nil;
        [SPKeychain setPassword:aPassword forServer:anAddress account:aNick error:&error];
    }
    
    // save bookmarks and explicitly synchronize with .plist
    [[NSUserDefaults standardUserDefaults] setObject:bookmarks forKey:SPBookmarks];
    [[NSUserDefaults standardUserDefaults] synchronize];
    
    return YES;
}

- (void)autoConnectBookmarks
{
    NSEnumerator *e = [bookmarks objectEnumerator];
    NSMutableDictionary *bm;
    while ((bm = [e nextObject]) != nil) {
        if ([[bm objectForKey:@"autoConnect"] boolValue] == YES) {
            [self connectToBookmark:bm];
        }
    }
}

- (void)setBookmarks:(NSArray *)anArray
{
    if (anArray != bookmarks) {
        [bookmarks release];

        bookmarks = [[NSMutableArray alloc] initWithCapacity:[anArray count]];

        /* deep mutable copy of the bookmarks in user defaults */
        NSEnumerator *e = [anArray objectEnumerator];
        NSDictionary *bm;
        while ((bm = [e nextObject])) {
            NSMutableDictionary *mutable_bm = [bm mutableCopy];
            if ([mutable_bm objectForKey:@"encodingIndex"] == nil) {
                /* set encoding to default if no encoding set */
                [mutable_bm setObject:[NSNumber numberWithInt:0] forKey:@"encodingIndex"];
            }
            [bookmarks addObject:mutable_bm];
            [mutable_bm release];
        }
    }

    [[NSUserDefaults standardUserDefaults] setObject:bookmarks forKey:SPBookmarks];
}

- (NSString *)passwordForHub:(NSString *)aHubAddress nick:(NSString *)aNick
{
    NSString *thePassword = nil;
    NSEnumerator *e = [bookmarks objectEnumerator];
    NSDictionary *bm;
    while ((bm = [e nextObject]) != nil) {
        if ([[bm objectForKey:@"address"] isEqualToString:aHubAddress] &&
           [[bm objectForKey:@"nick"] isEqualToString:aNick]) {
            // get password from keychain
            NSError *error = nil;
            thePassword = [SPKeychain passwordForServer:[bm objectForKey:@"address"]
                                                account:[bm objectForKey:@"nick"]
                                                  error:&error];
        }
    }
    
    return thePassword;
}

- (NSDictionary *)bookmarkForHub:(NSString *)address
{
    NSDictionary *bm = nil;
    NSEnumerator *bookmarksEnumerator = [bookmarks objectEnumerator];
    while ((bm = [bookmarksEnumerator nextObject])) {
      if ([[bm objectForKey:@"address"] isEqualToString:address])
        return bm;
    }
    return nil;
}

- (NSArray *)allowedEncodings
{
    return [NSArray arrayWithObjects:@"Default",
           @"CP1250",
           @"CP1251",
           @"CP1252",
           @"CP1253",
           @"CP1254",
           @"CP1255",
           @"CP1256",
           @"CP1257",
           @"CP1258",
           nil];
}

- (IBAction)setEncoding:(id)sender
{
    NSLog(@"setEncoding: sender = %@", sender);
}

- (void)tableDidRecieveEnterKey:(id)sender
{
    [self connectToSelectedBookmark:sender];
}

#pragma mark -
#pragma mark New bookmark sheet window

- (void)newBookmarkSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction)newBookmarkShow:(id)sender
{
    // set default bookmark name
    [newBookmarkName setStringValue:@"New bookmark"];
    
    // empty address, nick and password
    [newBookmarkAddress setStringValue:@""];
    [newBookmarkNickname setStringValue:@""];
    [newBookmarkPassword setStringValue:@""];
    [newBookmarkDescription setStringValue:@""];
    [newBookmarkAutoconnect setState:NSOffState];
    
    // load list of encodings
    [newBookmarkEncoding addItemsWithTitles:[self allowedEncodings]];
    [newBookmarkEncoding selectItemAtIndex:0];
    
    // visual hint for default nickname and description
    [[newBookmarkNickname cell] setPlaceholderString:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname]];
    [[newBookmarkDescription cell] setPlaceholderString:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDescription]];
    
    // select the name field
    [newBookmarkName selectText:self];
    
    // launch sheet
    [NSApp beginSheet:newBookmark
       modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
        modalDelegate:self
       didEndSelector:@selector(newBookmarkSheetDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
}

- (IBAction)newBookmarkExecute:(id)sender
{
    // the user must fill in an address
    if ([[newBookmarkAddress stringValue] length] == 0) {
        NSBeep();
        [newBookmarkAddress selectText:self];
        
        return;
    }
    
    [NSApp endSheet:newBookmark];
    
    // if the name wasn't supplied, use address as name
    if ([[newBookmarkName stringValue] length] == 0)
        [newBookmarkName setStringValue:[newBookmarkAddress stringValue]];
    
    [self addBookmarkWithName:[newBookmarkName stringValue]
                      address:[newBookmarkAddress stringValue]
                         nick:[newBookmarkNickname stringValue]
                     password:[newBookmarkPassword stringValue]
                  description:[newBookmarkDescription stringValue]
                  autoconnect:[newBookmarkAutoconnect state]
                     encoding:[newBookmarkEncoding titleOfSelectedItem]];
}

- (IBAction)newBookmarkCancel:(id)sender
{
    [NSApp endSheet:newBookmark];
}

#pragma mark -
#pragma mark Drag & drop support

- (NSDragOperation)tableView:(NSTableView *)tv validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op 
{
    // do not accept drops on other rows, just below/above
    if (op == NSTableViewDropOn)
        return NSDragOperationNone;

    return NSDragOperationEvery;
}

- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)operation
{
    NSPasteboard* pboard = [info draggingPasteboard];
    NSData *draggingData = [pboard dataForType:SPPublicHubDataType];
    
    if (draggingData) {
        NSArray* hubs = [NSKeyedUnarchiver unarchiveObjectWithData:draggingData];
        NSEnumerator *e = [hubs objectEnumerator];
        NSDictionary *currentHub = nil;
        while ((currentHub = [e nextObject])) {
            [self addBookmarkWithName:[[currentHub objectForKey:@"name"] string]
                              address:[[currentHub objectForKey:@"address"] string]];
        }
        
        if (currentHub)
            return YES;
    }

    return NO;
}

#pragma mark -
#pragma mark Edit bookmark sheet window

- (void)editBookmarkSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction)editBookmarkShow:(id)sender
{
    // read values from the selected bookmark
    NSArray *selectedObjects = [arrayController selectedObjects];
    if ([selectedObjects count]) {
        NSDictionary *bm = [selectedObjects objectAtIndex:0];
        
        // get password from keychain
        NSError *error = nil;
        NSString *thePassword = [SPKeychain passwordForServer:[bm objectForKey:@"address"]
                                                      account:[bm objectForKey:@"nick"]
                                                        error:&error];
        
        [editBookmarkName setStringValue:[bm objectForKey:@"name"]];
        [editBookmarkAddress setStringValue:[bm objectForKey:@"address"]];
        [editBookmarkNickname setStringValue:[bm objectForKey:@"nick"]];
        [editBookmarkPassword setStringValue:thePassword];
        [editBookmarkDescription setStringValue:[bm objectForKey:@"descriptionString"]];
        [editBookmarkAutoconnect setState:[[bm objectForKey:@"autoConnect"] boolValue]];
        
        // load list of encodings and select current one
        [editBookmarkEncoding addItemsWithTitles:[self allowedEncodings]];
        NSNumber *encodingIndex = [bm objectForKey:@"encodingIndex"];
        [editBookmarkEncoding selectItemAtIndex:[encodingIndex intValue]];
        
        // visual hint for default nickname and description
        [[editBookmarkNickname cell] setPlaceholderString:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname]];
        [[editBookmarkDescription cell] setPlaceholderString:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDescription]];
    
        // select the name field
        [editBookmarkName selectText:self];
        
        // launch sheet
        [NSApp beginSheet:editBookmark
           modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
            modalDelegate:self
           didEndSelector:@selector(editBookmarkSheetDidEnd:returnCode:contextInfo:)
              contextInfo:nil];
    }
}

- (IBAction)editBookmarkExecute:(id)sender
{
    // the user must fill in an address
    if ([[editBookmarkAddress stringValue] length] == 0) {
        NSBeep();
        [editBookmarkAddress selectText:self];
        
        return;
    }
    
    [NSApp endSheet:editBookmark];
    
    // if the name wasn't supplied, use address as name
    if ([[editBookmarkName stringValue] length] == 0)
        [editBookmarkName setStringValue:[editBookmarkAddress stringValue]];
    
    NSArray *selectedObjects = [arrayController selectedObjects];
    if ([selectedObjects count]) {
        NSMutableDictionary *bm = [selectedObjects objectAtIndex:0];
        [bm setObject:[editBookmarkName stringValue] forKey:@"name"];
        [bm setObject:[editBookmarkAddress stringValue] forKey:@"address"];
        [bm setObject:[editBookmarkNickname stringValue] forKey:@"nick"];
        [bm setObject:[NSNumber numberWithBool:[editBookmarkAutoconnect state]] forKey:@"autoConnect"];
        [bm setObject:[editBookmarkDescription stringValue] forKey:@"descriptionString"];
        [bm setObject:[NSNumber numberWithInt:[editBookmarkEncoding indexOfSelectedItem]] forKey:@"encodingIndex"];
        
        [bookmarks replaceObjectAtIndex:[bookmarkTable selectedRow] withObject:bm];
        
        // modify the password in keychain
        NSError *error = nil;
        [SPKeychain setPassword:[editBookmarkPassword stringValue]
                      forServer:[editBookmarkAddress stringValue]
                        account:[editBookmarkNickname stringValue]
                          error:&error];
    
        // save bookmarks and explicitly synchronize with .plist
        [[NSUserDefaults standardUserDefaults] setObject:bookmarks forKey:SPBookmarks];
        [[NSUserDefaults standardUserDefaults] synchronize];
    }
}

- (IBAction)editBookmarkCancel:(id)sender
{
    [NSApp endSheet:editBookmark];
}

#pragma mark -
#pragma mark Duplicate bookmark

- (IBAction)duplicateBookmark:(id)sender
{
    NSArray *selectedObjects = [arrayController selectedObjects];
    if ([selectedObjects count]) {
        NSDictionary *bm = [selectedObjects objectAtIndex:0];
        
        // Duplicate the selected bookmark
        [self addBookmarkWithName:[[bm objectForKey:@"name"] stringByAppendingString:@" copy"]
                          address:[bm objectForKey:@"address"]
                             nick:@""
                         password:@""
                      description:[bm objectForKey:@"descriptionString"]
                      autoconnect:[[bm objectForKey:@"autoConnect"] boolValue]
                         encoding:[[self allowedEncodings] objectAtIndex:[[bm objectForKey:@"encodingIndex"] intValue]]];
    }
}

#pragma mark -
#pragma mark Remove bookmark sheet window

- (void)removeBookmarkShow:(id)sender
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:@"Are you sure you want to delete this bookmark?"];
    [alert setAlertStyle:NSWarningAlertStyle];
    
    [alert beginSheetModalForWindow:[[SPMainWindowController sharedMainWindowController] window]
                      modalDelegate:self
                     didEndSelector:@selector(removeBookmarkSheetDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (void)removeBookmarkSheetDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertFirstButtonReturn) {
        NSArray *selectedObjects = [arrayController selectedObjects];
        if ([selectedObjects count]) {
            NSMutableDictionary *bm = [selectedObjects objectAtIndex:0];
            NSString *server = [bm objectForKey:@"address"];
            NSString *account = [bm objectForKey:@"nick"];
            
            // delete the bookmark
            [arrayController removeObject:bm];
            
            // delete the password in keychain
            NSError *error = nil;
            [SPKeychain deletePasswordForServer:server
                                        account:account
                                          error:&error];
            
            // save bookmarks and explicitly synchronize with .plist
            [[NSUserDefaults standardUserDefaults] setObject:bookmarks forKey:SPBookmarks];
            [[NSUserDefaults standardUserDefaults] synchronize];
        }
    }
    
    [[alert window] orderOut:self];
}

@end
