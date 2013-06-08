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

@interface SPPreferenceController : NSWindowController
{
    // Identity
    IBOutlet NSView *identityView;
    IBOutlet NSTextField *emailField;
    IBOutlet NSTextField *descriptionField;
    IBOutlet NSComboBox *speedField;
    
    // Share
    IBOutlet NSView *sharesView;
    IBOutlet NSArrayController *sharedPathsController;
    IBOutlet NSProgressIndicator *updateSharedPathsIndicator;
    IBOutlet NSPopUpButton *downloadFolderButton;
    IBOutlet NSPopUpButton *incompleteFolderButton;
    IBOutlet NSTextField *slotsField;
    IBOutlet NSButton *slotsPerHubButton;
    IBOutlet NSTextField *rescanShareField;
    IBOutlet NSMenu *duplicatePathsMenu;
    IBOutlet NSTableView *sharedPathsTable;
    
    // Network
    IBOutlet NSView *networkView;
    IBOutlet NSTextField *portField;
    IBOutlet NSTextField *IPAddressField;
    IBOutlet NSProgressIndicator *testConnectionProgress;
    IBOutlet NSTextField *testResults;
    
    // Advanced
    IBOutlet NSView *advancedView;
    IBOutlet NSComboBox *hublistsComboBox;
    IBOutlet NSArrayController *hublistsController;
    IBOutlet NSMenu *magnetHandlerMenu;
    IBOutlet NSPopUpButton *magnetHandlerButton;
    
    NSArray *predefinedDownloadLocations;
    NSMutableArray *sharedPaths;
    NSMutableSet *duplicatePaths;
    NSView *blankView;
    NSToolbar *prefsToolbar;
    NSDictionary *prefItems;
    uint64_t totalShareSize;
    BOOL testInProgress;
    BOOL hashingPaused;
}

+ (SPPreferenceController *)sharedPreferences;

- (void)show;
- (BOOL)isKeyWindow;
- (void)switchToItem:(id)item;

#pragma mark -
#pragma mark Notifications

- (void)shareStatsNotification:(NSNotification *)aNotification;
- (void)shareDuplicateFoundNotification:(NSNotification *)aNotification;
- (void)revealDuplicateInFinder:(NSMenuItem *)selectedItem;

#pragma mark -
#pragma mark Interface Actions

- (IBAction)addSharedPath:(id)sender;
- (IBAction)removeSharedPath:(id)sender;
- (IBAction)updateSharedPaths:(id)sender;
- (IBAction)selectDownloadFolder:(id)sender;
- (IBAction)selectIncompleteFolder:(id)sender;
- (IBAction)setPort:(id)sender;
- (IBAction)setIPAddress:(id)sender;
- (IBAction)setAutomaticPortForwarding:(id)sender;
- (IBAction)setAllowHubIPOverride:(id)sender;
- (IBAction)updateUserInfo:(id)sender;
- (IBAction)testConnection:(id)sender;
- (IBAction)setSlots:(id)sender;
- (IBAction)setConnectionMode:(id)sender;
- (IBAction)setLogLevel:(id)sender;
- (IBAction)setRescanShareInterval:(id)sender;
- (IBAction)setFollowRedirects:(id)sender;
- (IBAction)togglePauseHashing:(id)sender;
- (IBAction)setAutoSearchNewSources:(id)sender;
- (IBAction)setHashingPriority:(id)sender;
- (IBAction)setHublistURL:(id)sender;

#pragma mark -

- (void)addSharedPathsPath:(NSString *)aPath;
- (void)setTotalShareSize:(uint64_t)aNumber;
- (void)updateAllSharedPaths;
- (NSString *)selectFolder;
- (void)updateTestConnectionResultFromErrors:(NSNumber *)wrappedStatus;
- (void)testConnectionThread:(id)args;
- (NSImage *)smallIconForPath:(NSString *)path;
- (void)updateNameAndIconForDownloadFolder;
- (void)updateNameAndIconForIncompleteFolder;
- (void)setDefaultMagnetHandler:(id)sender;

@end
