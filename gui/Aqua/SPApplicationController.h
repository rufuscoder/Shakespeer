/* vim: ft=objc fdm=indent foldnestmax=1
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

#include <spclient.h>

void sendNotification(NSString *notificationName, NSString *key1, id arg1, ...);

@class SPMainWindowController;

@interface SPApplicationController : NSObject
{
    BOOL connectedToBackend;
    SPMainWindowController *mainWindowController;
    NSDictionary *dispatchTable;
    NSData *socketEOL;
    int lastSearchID;
    sp_t *sp;
    CFSocketRef sphubdSocket;
    CFRunLoopSourceRef sphubdRunLoopSource;

    IBOutlet NSMenu *menuOpenRecent;
    IBOutlet NSMenu *menuFilelists;
    IBOutlet NSWindow *initWindow;
    IBOutlet NSTextField *initMessage;
    IBOutlet NSMenuItem *menuItemNextSidebarItem;
    IBOutlet NSMenuItem *menuItemPrevSidebarItem;
}

+ (SPApplicationController *)sharedApplicationController;

- (IBAction)connectToBackendServer:(id)sender;
- (IBAction)showPreferences:(id)sender;
- (IBAction)toggleDrawer:(id)sender;
- (IBAction)openSPWebpage:(id)sender;
- (IBAction)openSPForums:(id)sender;
- (IBAction)reportBug:(id)sender;
- (IBAction)donate:(id)sender;
- (IBAction)showLogfiles:(id)sender;
- (IBAction)showConnectView:(id)sender;
- (IBAction)showPublicHubsView:(id)sender;
- (IBAction)showFriendsView:(id)sender;
- (IBAction)showBookmarksView:(id)sender;
- (IBAction)showDownloadsView:(id)sender;
- (IBAction)showUploadsView:(id)sender;
- (IBAction)showMainWindow:(id)sender;
- (IBAction)closeCurrentWindow:(id)sender;
- (IBAction)showAdvancedSearch:(id)sender;
- (IBAction)rescanSharedFolders:(id)sender;

- (IBAction)prevSidebarItem:(id)sender;
- (IBAction)nextSidebarItem:(id)sender;
- (IBAction)showServerMessages:(id)sender;

- (void)connectWithAddress:(NSString *)anAddress
                      nick:(NSString *)aNick
               description:(NSString *)aDescription
                  password:(NSString *)aPassword
                  encoding:(NSString *)anEncoding;
- (void)disconnectFromAddress:(NSString *)anAddress;
- (void)sendPublicMessage:(NSString *)aMessage toHub:(NSString *)hubAddress;
- (int)searchHub:(NSString *)aHubAddress
       forString:(NSString *)aSearchString
            size:(uint64_t)aSize
 sizeRestriction:(int)aSizeRestriction
        fileType:(int)aFileType;
- (int)searchAllHubsForString:(NSString *)aSearchString
                          size:(uint64_t)aSize
               sizeRestriction:(int)aSizeRestriction
                      fileType:(int)aFileType;
- (int)searchHub:(NSString *)aHubAddress forTTH:(NSString *)aTTH;
- (int)searchAllHubsForTTH:(NSString *)aTTH;
- (void)downloadFile:(NSString *)aFilename withSize:(NSNumber *)aSize
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalFile:(NSString *)aLocalFilename
                 TTH:(NSString *)aTTH;
- (void)downloadDirectory:(NSString *)aDirectory
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalDirectory:(NSString *)aLocalFilename;
- (void)downloadFilelistFromUser:(NSString *)aNick onHub:(NSString *)aHubAddress
                     forceUpdate:(BOOL)forceUpdateFlag autoMatch:(BOOL)autoMatchFlag;
- (void)removeSource:(NSString *)targetFilename nick:(NSString *)aNick;
- (void)removeQueue:(NSString *)targetFilename;
- (void)removeDirectory:(NSString *)targetDirectory;
- (void)removeFilelistForNick:(NSString *)aNick;
- (void)removeAllSourcesWithNick:(NSString *)aNick;
- (void)addSharedPath:(NSString *)aPath;
- (void)removeSharedPath:(NSString *)aPath;
- (void)cancelTransfer:(NSString *)targetFilename;

#pragma mark Recent hubs

- (void)recentOpenHub:(id)sender;
- (void)addRecentHub:(NSString *)anAddress;
- (NSArray *)recentHubs;
- (IBAction)clearRecentHubs:(id)sender;

#pragma mark -

- (void)setPort:(int)aPort;
- (void)setIPAddress:(NSString *)anIPAddress;
- (void)setAllowHubIPOverride:(BOOL)allowOverride;
- (void)setPassword:(NSString *)aPassword forHub:(NSString *)aHubAddress;
- (void)updateUserEmail:(NSString *)email description:(NSString *)description speed:(NSString *)speed;
- (void)sendPrivateMessage:(NSString *)theMessage toNick:(NSString *)theNick hub:(NSString *)hubAddress;
- (void)sendRawCommand:(NSString *)rawCommand toHub:(NSString *)hubAddress;
- (void)setSlots:(int)slots perHub:(BOOL)perHubFlag;
- (void)setPassiveMode;
- (void)forgetSearch:(int)searchID;
- (void)versionMismatch:(NSString *)serverVersion;
- (void)setLogLevel:(NSString *)aLogLevel;
- (void)setPriority:(unsigned int)priority onTarget:(NSString *)targetFilename;
- (void)setRescanShareInterval:(unsigned int)rescanShareInterval;
- (void)setFollowHubRedirects:(BOOL)followFlag;
- (void)setAutoSearchNewSources:(BOOL)autoSearchFlag;
- (void)addRecentFilelist:(NSString *)nick;
- (void)grantExtraSlotToNick:(NSString *)nick;
- (void)pauseHashing;
- (void)resumeHashing;
- (void)setHashingPriority:(unsigned int)prio;
- (void)setDownloadFolder:(NSString *)downloadFolder;
- (void)setIncompleteFolder:(NSString *)incompleteFolder;
- (void)registerUrlHandler;
- (BOOL)setupSphubdConnection;
- (void)removeSphubdConnection;

@end
