/*
 * Copyright 2005-2007 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include <stdarg.h>
#include <unistd.h>

#import <SystemConfiguration/SCNetwork.h>

#import "SPApplicationController.h"
#import "SPNetworkPortController.h"
#import "SPTransformers.h"
#import "SPPreferenceController.h"
#import "SPBookmarkController.h"
#import "SPMessagePanel.h"
#import "SPLog.h"
#import "SPMainWindowController.h"
#import "SPQueueController.h"
#import "SPClientBridge.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"
#import "UKCrashReporter.h"

#include "nmdc.h"
#include "log.h"
#ifndef VERSION
# include "../../version.h"
#endif

#define MAX_RECENT_HUBS 10

@implementation SPApplicationController

#pragma mark Singleton implementation

// TODO: this is the recommended singleton implementation code from Apple:
// http://developer.apple.com/documentation/Cocoa/Conceptual/CocoaFundamentals/CocoaObjects/chapter_3_section_10.html
// it should be implemented for all other static sidebar items too

static SPApplicationController *sharedApplicationController = nil;

+ (SPApplicationController *)sharedApplicationController
{
    if (sharedApplicationController == nil) {
        [[self alloc] init]; // assignment not done here
    }
    return sharedApplicationController;
}

+ (id)allocWithZone:(NSZone *)zone
{
    if (sharedApplicationController == nil) {
        sharedApplicationController = [super allocWithZone:zone];
        return sharedApplicationController; // assignment and return on first allocation
    }
    return nil; // on subsequent allocation attempts return nil
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

- (id)retain
{
    return self;
}

- (unsigned)retainCount
{
    return UINT_MAX; // denotes an object that cannot be released
}

- (void)release
{
    // do nothing
}

- (id)autorelease
{
    return self;
}

#pragma mark -

- (id)init
{
    if ((self = [super init])) {
        char *working_directory = get_working_directory();
        sp_log_init(working_directory, "shakespeer-aqua");
        free(working_directory);
        
        sp = sp_create(NULL);
        sp_register_callbacks(sp);
        
        NSString *defaultDownloadFolder;
        SInt32 version;
        Gestalt(gestaltSystemVersionMinor, &version);
        if (version < 5)
            defaultDownloadFolder = @"~/Desktop/Shakespeer Downloads";
        else
            defaultDownloadFolder = @"~/Downloads/Shakespeer Downloads";
        // TODO: The user shouldn't have to care about "incomplete folder", we should use
        // a folder in application support instead
        NSString *defaultIncompleteFolder = [[defaultDownloadFolder stringByExpandingTildeInPath] stringByAppendingPathComponent:@"Incomplete"];
 
        NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                     @"Bookmarks", SPPrefsLastSidebarItem,
                                     @"IdentityItem", SPPrefsLastPrefPane,
                                     defaultDownloadFolder, SPPrefsDownloadFolder,
                                     defaultIncompleteFolder, SPPrefsIncompleteFolder,
                                     @"unconfigured-shakespeer-user", SPPrefsNickname,
                                     @"", SPPrefsEmail,
                                     @"DSL", SPPrefsSpeed,
                                     @"", SPPrefsDescription,
                                     [NSNumber numberWithInt:11], SPPrefsFontSize,
                                     [NSNumber numberWithInt:3], SPPrefsUploadSlots,
                                     [NSNumber numberWithBool:YES], SPPrefsUploadSlotsPerHub,
                                     [NSNumber numberWithInt:1412], SPPrefsPort,
                                     [NSNumber numberWithInt:0], SPPrefsConnectionMode,
                                     [NSArray array], SPPrefsSharedPaths,
                                     [NSArray array], SPPrefsRecentHubs,
                                     [NSArray arrayWithObjects:
                                        @"http://www.hublist.org/PublicHubList.xml.bz2",
                                        @"http://www.hublist.org/hublists/se.PublicHubList.xml.bz2",
                                        @"http://www.dc-resources.com/downloads/hublist.config.bz2",
                                        @"http://www.hublist.org/PublicHubList.config.bz2",
                                        @"http://www.Freenfo.net/PublicHubList.config.bz2",
                                        @"http://www.freeweb.hu/pankeey/dc-hubz/pankeey-dchubz.config.bz2",
                                        @"http://wza.digitalbrains.com/DC/hublist.bz2",
                                        @"http://dcinfo.sytes.net/publichublist.config.bz2",
                                        @"http://dreamland.gotdns.org/PublicHubList.config.bz2",
                                        @"http://gb.hublist.org/PublicHubList.config.bz2",
                                        @"http://www.dchublist.biz/all_hubs.config.bz2",
                                        @"http://dcinfo.sytes.net/hungaryhublist.config.bz2",
                                     nil], SPPrefsHublists,
                                     @"http://www.hublist.org/PublicHubList.xml.bz2", SPPrefsHublistURL,
                                     [NSNumber numberWithBool:NO], SPPrefsKeepServerRunning,
                                     @"Message", SPPrefsLogLevel,
                                     [NSNumber numberWithBool:YES], SPPrefsAutodetectIPAddress,
                                     [NSNumber numberWithBool:YES], SPPrefsAllowHubIPOverride,
                                     [NSNumber numberWithBool:NO], SPPrefsAutomaticPortForwarding,
                                     [NSNumber numberWithBool:YES], SPPrefsShowSmileyIcons,
                                     [NSNumber numberWithFloat:1.0], SPPrefsRescanShareInterval,
                                     [NSNumber numberWithBool:YES], SPPrefsFollowHubRedirects,
                                     [NSNumber numberWithBool:YES], SPPrefsAutoSearchNewSources,
                                     [NSNumber numberWithUnsignedInt:2], SPPrefsHashingPriority,
                                     [NSNumber numberWithBool:NO], SPPrefsDrawerIsVisible,
                                     [NSNumber numberWithInt:100], SPPrefsDrawerHeight,
                                     [NSNumber numberWithBool:NO], SPPrefsRequireOpenSlots,
                                     [NSNumber numberWithBool:NO], SPPrefsRequireTTH,
                                     [NSNumber numberWithBool:YES], SPPrefsSessionRestore,
                                     [NSArray array], SPBookmarks,
                                     [NSArray array], SPFriends,
                                     nil];
        
        [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
        
        if ([[NSUserDefaults standardUserDefaults] stringForKey:@"firstRun"] == nil) {
            NSLog(@"first run: creating default download folders");
            [[NSFileManager defaultManager] createDirectoryAtPath:defaultDownloadFolder attributes:nil];
            [[NSFileManager defaultManager] createDirectoryAtPath:defaultIncompleteFolder attributes:nil];
            [[NSUserDefaults standardUserDefaults] setObject:@"NO" forKey:@"firstRun"];
        }
        
        sp_log_set_level([[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsLogLevel] UTF8String]);
        
        [[NSApplication sharedApplication] setDelegate:self];
        
        lastSearchID = 1;
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(serverDiedNotification:)
                                                     name:SPNotificationServerDied
                                                   object:nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(filelistFinishedNotification:)
                                                     name:SPNotificationFilelistFinished
                                                   object:nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(storedFilelistsNotification:)
                                                     name:SPNotificationStoredFilelists
                                                   object:nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(initCompletionNotification:)
                                                     name:SPNotificationInitCompletion
                                                   object:nil];
    }
    
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)registerUrlHandler
{
    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:self
            andSelector:@selector(getUrl:withReplyEvent:)
          forEventClass:kInternetEventClass
             andEventID:kAEGetURL];
}

- (void)getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSString *urlString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
    NSURL *url = [NSURL URLWithString:urlString];
    if (url == nil) {
        NSLog(@"Malformed URL: %@", urlString);
    }
    else if ([[url scheme] isEqualToString:@"magnet"]) {
        // This is probably a magnet link
        NSString *afterScheme = [urlString substringFromIndex:8];
        NSArray *args = [afterScheme componentsSeparatedByString:@"&"];
        
        NSEnumerator *e = [args objectEnumerator];
        NSString *arg = nil;
        while ((arg = [e nextObject])) {
            NSArray *components = [arg componentsSeparatedByString:@"="];
            if ([components count] == 2) {
                NSString *key = [components objectAtIndex:0];
                NSString *value = [components objectAtIndex:1];
                
                if ([key hasPrefix:@"xt"]) {
                    // We've found the TTH, now let's check that it's a TTH magnet
                    if ([value hasPrefix:@"urn:tree:tiger:"]) {
                        // It's definitely a TTH magnet.  Lets run a search for the TTH.
                        NSString *tth = [value substringFromIndex:15];
                        [mainWindowController performSearchFor:tth
                                                          size:nil
                                               sizeRestriction:SHARE_SIZE_NONE
                                                          type:1
                                                    hubAddress:nil];
                    }
                }
            }
        }
    }
    else {
        NSString *address = nil;
        if ([url port])
            address = [NSString stringWithFormat:@"%@:%@", [url host], [url port]];
        else
            address = [url host];
        
        [self connectWithAddress:address
                            nick:[url user]
                     description:nil
                        password:[url password]
                        encoding:nil];
    }
}

- (BOOL)setupSphubdConnection
{
    NSString *remoteSphubdAddress = nil;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsConnectRemotely] == YES) {
        remoteSphubdAddress = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsRemoteSphubdAddress];
    }

    if (remoteSphubdAddress && [remoteSphubdAddress length] > 0) {
        if (sp_connect_remote(sp, [remoteSphubdAddress UTF8String]) != 0) {
            SPLog(@"Failed to connect to remote sphubd");
            /* TODO: Must return YES here */
            return NO;
        }
    }
else { 
#ifdef BUILD_PROFILE
            NSString *executable = @"sphubd";
#else
            NSString *executable = @"debug-sphubd.sh";
#endif
        NSString *launchPath = [NSString stringWithFormat:@"%@/%@",
                [[NSBundle mainBundle] resourcePath], executable];
        const char *workDir = "~/Library/Application Support/ShakesPeer";

        if (sp_connect(sp, workDir, [launchPath UTF8String]) != 0) {
            NSLog(@"Failed to execute sphubd");
            return NO;
        }
    }

    /* TODO: this is just silly! We should not use any libevent stuff here! */
    sp->output = evbuffer_new();

    /* Attach the socket to the run loop */
    CFSocketContext spContext;
    spContext.version = 0;
    spContext.info = sp; /* user data passed to the callbacks */
    spContext.retain = nil;
    spContext.release = nil;
    spContext.copyDescription = nil;

    sphubdSocket = CFSocketCreateWithNative(kCFAllocatorDefault, sp->fd,
            kCFSocketReadCallBack | kCFSocketWriteCallBack, sp_callback, &spContext);
    if (sphubdSocket == NULL) {
        NSLog(@"Failed to create a CFSocket");
        return NO;
    }
    sp->user_data = sphubdSocket;
    sphubdRunLoopSource = CFSocketCreateRunLoopSource(kCFAllocatorDefault, sphubdSocket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sphubdRunLoopSource, kCFRunLoopDefaultMode);

    /* disable the write callback (only enable if sp_send_string returns EAGAIN, see SPClientBridge.m) */
    CFSocketDisableCallBacks(sphubdSocket, kCFSocketWriteCallBack);

    [self setLogLevel:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsLogLevel]];
    int num_shared_paths= [[[NSUserDefaults standardUserDefaults] arrayForKey:SPPrefsSharedPaths] count];
    sp_send_expect_shared_paths(sp, num_shared_paths);

    return YES;
}

- (void)removeSphubdConnection
{
    SPLog(@"removing connection with sphubd");

    CFSocketInvalidate(sphubdSocket);
    CFRelease (sphubdSocket);
    sphubdSocket = NULL;

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sphubdRunLoopSource, kCFRunLoopDefaultMode);
    CFRelease(sphubdRunLoopSource);
    sphubdRunLoopSource = NULL;
}

- (void)versionMismatchAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode
                       contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertSecondButtonReturn) {
        [[NSApplication sharedApplication] terminate:self];
    }
    else {
        sp_send_shutdown(sp);
        sleep(2);
        [self setupSphubdConnection];
    }
}

- (void)versionMismatch:(NSString *)serverVersion
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle:@"Restart"];
    [alert addButtonWithTitle:@"Quit"];
    [alert setMessageText:@"Server version mismatch"];
    [alert setInformativeText:
       [NSString stringWithFormat:@"The server is running version %@, but version %s is required",
        serverVersion, VERSION]];
    [alert setAlertStyle:NSCriticalAlertStyle];

    /* Restarting requires us to reset some status, eg deallocate all hubs,
     * since this is not yet implemented, leave it disabled for now
     */
    [[[alert buttons] objectAtIndex:0] setEnabled:NO];

    [alert beginSheetModalForWindow:[mainWindowController window] modalDelegate:self
                     didEndSelector:@selector(versionMismatchAlertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (IBAction)connectToBackendServer:(id)sender
{
    if ([self setupSphubdConnection] == NO) {
        SPLog(@"Unable to exec/connect to sphubd!");

        NSAlert *alert = [NSAlert alertWithMessageText:@"Unable to start/connect to backend server"
                                         defaultButton:@"Exit"
                                       alternateButton:@"Show preferences"
                                           otherButton:nil
                             informativeTextWithFormat:@""];
        [alert setAlertStyle:NSCriticalAlertStyle];
        int rc = [alert runModal];
        if (rc == 1) {
            exit(1);
        }
        else if (rc == 0) {
            [self showPreferences:self];
        }
    }
    else {
        mainWindowController = [SPMainWindowController sharedMainWindowController];

        /* Must initialize the queue controller because it may get queue notifications
         * during startup of the backend. */
        [SPQueueController sharedQueueController];

	[initMessage setStringValue:@"Initializing databases..."];

        /* add the stored recent hubs to the menu */
        NSArray *recentHubs = [[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsRecentHubs];
        // use the reverse enumerator so that the most recent hub is at the top
        NSEnumerator *e = [recentHubs reverseObjectEnumerator];
        NSString *recentHub;
        while ((recentHub = [e nextObject]) != nil) {
            [self addRecentHub:recentHub];
        }

        connectedToBackend = YES;
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];
    BOOL mainWindowVisible = [[mainWindowController window] isVisible];
    
    if (action == @selector(connectToBackendServer:))
        return !connectedToBackend;
    else if (action == @selector(showConnectView:) ||
             action == @selector(showAdvancedSearch:) ||
             action == @selector(showPublicHubsView:) ||
             action == @selector(showFriendsView:) ||
             action == @selector(showBookmarksView:) ||
             action == @selector(showDownloadsView:) ||
             action == @selector(showUploadsView:) ||
             action == @selector(prevSidebarItem:) ||
             action == @selector(nextSidebarItem:) ||
             action == @selector(toggleDrawer:) ||
             action == @selector(openRecentFilelist:) ||
             action == @selector(recentOpenHub:))
        return (connectedToBackend && mainWindowVisible);
    else if (action == @selector(closeCurrentWindow:))
        return [NSApp keyWindow] != nil;
    else if (action == @selector(showMainWindow:))
        return !mainWindowVisible;
    
    return YES;
}

- (void)checkDownloadFolder
{
    NSUserDefaults *standardUserDefaults = [NSUserDefaults standardUserDefaults];
    NSString *downloadFolder = [[standardUserDefaults stringForKey:SPPrefsDownloadFolder] stringByExpandingTildeInPath];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if(![fileManager fileExistsAtPath:downloadFolder isDirectory:&isDir] || !isDir) {
	NSLog(@"download folder not found or not a directory: %@", downloadFolder);
	NSAlert *alert = [NSAlert alertWithMessageText:@"Download folder not found."
                                         defaultButton:@"Go to preferences"
				       alternateButton:@"Ignore"
				       otherButton:nil
				       informativeTextWithFormat:@"The folder %@ was not found, or is not a folder. If this folder is on an external disk, verify that the disk is correctly attached. If you choose to ignore this, downloads will not work.", [downloadFolder stringByAbbreviatingWithTildeInPath]];

	int rc = [alert runModal];
	if(rc == NSAlertDefaultReturn) {
	    [[SPPreferenceController sharedPreferences] switchToItem:@"ShareItem"];
	    [self showPreferences:self];
	}
    }
}

- (void)awakeFromNib
{
    // XXX: can these live in the main window's awakeFromNib instead?
    
    /* set Command-Down Arrow to select next sidebar item */
    unichar down = NSDownArrowFunctionKey;
    [menuItemNextSidebarItem setKeyEquivalent:[NSString stringWithCharacters:&down length:1]];
    [menuItemNextSidebarItem setKeyEquivalentModifierMask:NSCommandKeyMask];

    /* set Command-Up Arrow to select previous sidebar item */
    unichar up = NSUpArrowFunctionKey;
    [menuItemPrevSidebarItem setKeyEquivalent:[NSString stringWithCharacters:&up length:1]];
    [menuItemPrevSidebarItem setKeyEquivalentModifierMask:NSCommandKeyMask];
    
    // start the serial (ordered) startup (we'll setup the sphubd connection, ports, and other things)
    sendNotification(SPNotificationInitCompletion, @"level", [NSNumber numberWithInt:25], nil);
}
 
 - (void)applicationDidFinishLaunching:(NSNotification *)aNotification
 {
	 NSLog(@"applicationDidFinishLaunching: checking for recent sphubd crashes");
	 UKCrashReporterCheckForCrash(@"sphubd");
	 NSLog(@"applicationDidFinishLaunching: checking for recent sphashd crashes");
	 UKCrashReporterCheckForCrash(@"sphashd");
	 NSLog(@"applicationDidFinishLaunching: checking for recent gui crashes");
	 UKCrashReporterCheckForCrash(nil);
 }

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    [self registerUrlHandler];
}

#pragma mark Recent hubs

- (void)recentOpenHub:(id)sender
{
    [self connectWithAddress:[sender title] nick:nil description:nil password:nil encoding:nil];
}

- (void)addRecentHub:(NSString *)anAddress
{
    /* The list of recent hubs has a limit defined by MAX_RECENT_HUBS at
    the top of this file. The actual menu (in MainMenu.nib) also has
    a "Clear menu" item and a separator at the very bottom. New items
    are always inserted at the top (index 0), as in the recent items menu
    in Finder. Items are deleted from the bottom of the list. Only unique
    addresses will be added to the list. */
    
    // check if the given address is already in the list
    if ([[self recentHubs] indexOfObject:anAddress] == NSNotFound) {
        int numberOfItems = [[self recentHubs] count];
        
        // only keep ten recent hubs
        if (numberOfItems >= MAX_RECENT_HUBS) {
            /* remove all items from the 11th to the last if there are
            more than 10 (this will only be run once for each user,
            because we didn't have any limit until now) */
            if (numberOfItems > MAX_RECENT_HUBS) {
                int i;
                int numberOfExcessItems = numberOfItems - MAX_RECENT_HUBS;
                for (i = 0; i < numberOfExcessItems; i++) {
                    [menuOpenRecent removeItemAtIndex:numberOfItems - 1];
                }
            }
            
            // otherwise, just remove the first one
            [menuOpenRecent removeItemAtIndex:numberOfItems - 1];
        }
        
        // insert the item at the top of the list
        NSMenuItem *hubMenuItem = [menuOpenRecent insertItemWithTitle:anAddress
                                                               action:@selector(recentOpenHub:)
                                                        keyEquivalent:@""
                                                              atIndex:0];
        [hubMenuItem setTarget:self];
        
        // write the list of recent hubs to preferences
        [[NSUserDefaults standardUserDefaults] setObject:[self recentHubs]
                                                  forKey:SPPrefsRecentHubs];
    }
}

// returns the list of recent hubs as an array of strings
- (NSArray *)recentHubs
{
    NSMutableArray *recentHubs = [NSMutableArray array];
    
    NSEnumerator *e = [[menuOpenRecent itemArray] objectEnumerator];
    NSMenuItem *currentItem;
    while ((currentItem = [e nextObject]) != nil) {
        if ([[currentItem title] isNotEqualTo:@"Clear menu"] && ![currentItem isSeparatorItem])
            [recentHubs addObject:[currentItem title]];
    }
    
    return recentHubs;
}

- (IBAction)clearRecentHubs:(id)sender
{
    // clear the list of recent hubs
    NSEnumerator *e = [[menuOpenRecent itemArray] objectEnumerator];
    NSMenuItem *currentItem;
    while ((currentItem = [e nextObject]) != nil) {
        if ([[currentItem title] isNotEqualTo:@"Clear menu"] && ![currentItem isSeparatorItem])
            [menuOpenRecent removeItem:currentItem];
    }
    
    // write our now empty list to preferences
    [[NSUserDefaults standardUserDefaults] setObject:[self recentHubs]
                                              forKey:SPPrefsRecentHubs];
}

#pragma mark -
#pragma mark sphubd command functions

- (void)connectWithAddress:(NSString *)anAddress
                      nick:(NSString *)aNick
               description:(NSString *)aDescription
                  password:(NSString *)aPassword
                  encoding:(NSString *)anEncoding
{
    // TODO: Since this is (supposed to be) the canonical method for connecting
    // to hubs, all necessary checks should be done here, i.e. make sure that
    // all passed variables are sane.
    
    NSString *nick;
    if (aNick == nil || [aNick length] == 0)
        nick = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname];
    else
        nick = aNick;

    NSString *description;
    if (aDescription == nil || [aDescription length] == 0)
        description = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDescription];
    else
        description = aDescription;

    NSString *email = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsEmail];
    NSString *speed = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsSpeed];
    int passive = [[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsConnectionMode];

    sp_send_connect(sp, [anAddress UTF8String], [nick UTF8String], [email UTF8String],
            [description UTF8String], [speed UTF8String], passive, [aPassword UTF8String], [anEncoding UTF8String]);

    /* add the hub to the recent hubs menu */
    [self addRecentHub:anAddress];
}

- (void)disconnectFromAddress:(NSString *)anAddress
{
    sp_send_disconnect(sp, [anAddress UTF8String]);
}

- (void)sendPublicMessage:(NSString *)aMessage toHub:(NSString *)hubAddress
{
    char *msg = nmdc_escape([aMessage UTF8String]);
    sp_send_public_message(sp, [hubAddress UTF8String], msg);
    free(msg);
}

- (int)searchHub:(NSString *)aHubAddress
       forString:(NSString *)aSearchString
            size:(uint64_t)aSize
 sizeRestriction:(int)aSizeRestriction
        fileType:(int)aFileType
{
    char *search_string = nmdc_escape([aSearchString UTF8String]);
    sp_send_search(sp, [aHubAddress UTF8String], search_string,
            aSize, aSizeRestriction, aFileType, lastSearchID);
    free(search_string);
    return lastSearchID++;
}

- (int)searchAllHubsForString:(NSString *)aSearchString
                          size:(uint64_t)aSize
               sizeRestriction:(int)aSizeRestriction
                      fileType:(int)aFileType
{
    char *search_string = nmdc_escape([aSearchString UTF8String]);
    sp_send_search_all(sp, search_string, aSize, aSizeRestriction, aFileType, lastSearchID);
    free(search_string);
    return lastSearchID++;
}

- (int)searchHub:(NSString *)aHubAddress forTTH:(NSString *)aTTH
{
    sp_send_search(sp, [aHubAddress UTF8String], [[NSString stringWithFormat:@"TTH:%@", aTTH] UTF8String],
            0ULL, SHARE_SIZE_MIN, SHARE_TYPE_TTH, lastSearchID);
    return lastSearchID++;
}

- (int)searchAllHubsForTTH:(NSString *)aTTH
{
    sp_send_search_all(sp, [[NSString stringWithFormat:@"TTH:%@", aTTH] UTF8String], 0ULL, SHARE_SIZE_MIN,
            SHARE_TYPE_TTH, lastSearchID);
    return lastSearchID++;
}

- (void)downloadFile:(NSString *)aFilename withSize:(NSNumber *)aSize
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalFile:(NSString *)aLocalFilename
                 TTH:(NSString *)aTTH
{
    sp_send_download_file(sp,
            [aHubAddress UTF8String], [aNick UTF8String], [aFilename UTF8String],
            [aSize unsignedLongLongValue], [aLocalFilename UTF8String], [aTTH UTF8String]);
}

- (void)downloadDirectory:(NSString *)aDirectory
            fromNick:(NSString *)aNick
               onHub:(NSString *)aHubAddress
         toLocalDirectory:(NSString *)aLocalDirectory;
{
    sp_send_download_directory(sp,
            [aHubAddress UTF8String], [aNick UTF8String], [aDirectory UTF8String],
            [aLocalDirectory UTF8String]);
}

- (void)downloadFilelistFromUser:(NSString *)aNick onHub:(NSString *)aHubAddress
                     forceUpdate:(BOOL)forceUpdateFlag autoMatch:(BOOL)autoMatchFlag
{
    sp_send_download_filelist(sp, [aHubAddress UTF8String], [aNick UTF8String], forceUpdateFlag, autoMatchFlag);
}

- (void)removeSource:(NSString *)targetFilename nick:(NSString *)aNick
{
    sp_send_queue_remove_source(sp, [targetFilename UTF8String], [aNick UTF8String]);
}

- (void)removeQueue:(NSString *)targetFilename
{
    sp_send_queue_remove_target(sp, [targetFilename UTF8String]);
}

- (void)removeDirectory:(NSString *)targetDirectory
{
    sp_send_queue_remove_directory(sp, [targetDirectory UTF8String]);
}

- (void)removeFilelistForNick:(NSString *)aNick
{
    sp_send_queue_remove_filelist(sp, [aNick UTF8String]);
}

- (void)removeAllSourcesWithNick:(NSString *)aNick
{
    sp_send_queue_remove_nick(sp, [aNick UTF8String]);
}

- (void)addSharedPath:(NSString *)aPath;
{
    sp_send_add_shared_path(sp, [aPath UTF8String]);
}

- (void)removeSharedPath:(NSString *)aPath
{
    sp_send_remove_shared_path(sp, [aPath UTF8String]);
}

- (void)setPort:(int)aPort
{
    sp_send_set_port(sp, aPort);
}

- (void)setIPAddress:(NSString *)anIPAddress
{
    if (anIPAddress && [anIPAddress length] > 0)
        sp_send_set_ip_address(sp, [anIPAddress UTF8String]);
    else
        sp_send_set_ip_address(sp, "auto-detect");
}

- (void)setAllowHubIPOverride:(BOOL)allowOverride
{
    sp_send_set_allow_hub_ip_override(sp, allowOverride);
}

- (void)setPassword:(NSString *)aPassword forHub:(NSString *)aHubAddress
{
    sp_send_set_password(sp, [aHubAddress UTF8String], [aPassword UTF8String]);
}

- (void)updateUserEmail:(NSString *)email description:(NSString *)description speed:(NSString *)speed
{
    sp_send_update_user_info(sp, [speed UTF8String], [description UTF8String], [email UTF8String]);
}

- (void)sendPrivateMessage:(NSString *)theMessage toNick:(NSString *)theNick hub:(NSString *)hubAddress
{
    char *msg = nmdc_escape([theMessage UTF8String]);
    sp_send_private_message(sp, [hubAddress UTF8String], [theNick UTF8String], msg);
    free(msg);
}

- (void)setSlots:(int)slots perHub:(BOOL)perHubFlag
{
    sp_send_set_slots(sp, slots, perHubFlag);
}

- (void)setPassiveMode
{
    sp_send_set_passive(sp, 1);
}

- (void)forgetSearch:(int)searchID
{
    sp_send_forget_search(sp, searchID);
}

- (void)cancelTransfer:(NSString *)targetFilename
{
    sp_send_cancel_transfer(sp, [targetFilename UTF8String]);
}

- (void)sendRawCommand:(NSString *)rawCommand toHub:(NSString *)hubAddress
{
    char *cmd = nmdc_escape([rawCommand UTF8String]);
    sp_send_raw_command(sp, [hubAddress UTF8String], cmd);
    free(cmd);
}

- (void)setLogLevel:(NSString *)aLogLevel
{
    sp_send_log_level(sp, [aLogLevel UTF8String]);
}

- (void)setPriority:(unsigned int)priority onTarget:(NSString *)targetFilename
{
    sp_send_set_priority(sp, [targetFilename UTF8String], priority);
}

- (void)setRescanShareInterval:(unsigned int)rescanShareInterval
{
    sp_send_rescan_share_interval(sp, rescanShareInterval);
}

- (void)setFollowHubRedirects:(BOOL)followFlag
{
    sp_send_set_follow_redirects(sp, followFlag ? 1 : 0);
}

- (void)setAutoSearchNewSources:(BOOL)autoSearchFlag
{
    sp_send_set_auto_search(sp, autoSearchFlag ? 1 : 0);
}

- (void)grantExtraSlotToNick:(NSString *)nick
{
    sp_send_grant_slot(sp, [nick UTF8String]);
}

- (void)pauseHashing
{
    sp_send_pause_hashing(sp);
}

- (void)resumeHashing
{
    sp_send_resume_hashing(sp);
}

- (void)setHashingPriority:(unsigned int)prio
{
    sp_send_set_hash_prio(sp, prio);
}

- (void)setDownloadFolder:(NSString *)downloadFolder
{
    sp_send_set_download_directory(sp, [downloadFolder UTF8String]);
}

- (void)setIncompleteFolder:(NSString *)incompleteFolder
{
    sp_send_set_incomplete_directory(sp, [incompleteFolder UTF8String]);
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)showPreferences:(id)sender
{
    [[SPPreferenceController sharedPreferences] show];
}

- (IBAction)toggleDrawer:(id)sender
{
    [mainWindowController toggleTransferDrawer:self];
}

- (IBAction)openSPWebpage:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/"]];
}

- (IBAction)openSPForums:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/forum/"]];
}

- (IBAction)reportBug:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://code.google.com/p/shakespeer/wiki/GuidelinesForReportingBugs"]];
}

- (IBAction)donate:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/donate.html"]];
}

- (IBAction)showLogfiles:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[@"~/Library/Logs/sphubd.log" stringByExpandingTildeInPath] withApplication:@"Console.app"];
}

- (IBAction)showConnectView:(id)sender
{
    [mainWindowController connectShow:sender];
}

- (IBAction)showAdvancedSearch:(id)sender
{
    [mainWindowController advSearchShow:sender];
}

- (IBAction)showServerMessages:(id)sender
{
    SPMessagePanel *mp = [SPMessagePanel sharedMessagePanel];
    [mp show];
}

- (IBAction)rescanSharedFolders:(id)sender
{
    [[SPPreferenceController sharedPreferences] updateAllSharedPaths];
}

- (IBAction)showMainWindow:(id)sender
{
    [mainWindowController showWindow:self];
}

- (IBAction)closeCurrentWindow:(id)sender
{
    if ([[SPPreferenceController sharedPreferences] isKeyWindow]) {
        [[SPPreferenceController sharedPreferences] close];
    }
    else if ([[SPMessagePanel sharedMessagePanel] isKeyWindow]) {
        [[SPMessagePanel sharedMessagePanel] close];
    }
    else {
        [[NSApp keyWindow] performClose:self];
    }
}

#pragma mark -
#pragma mark Sidebar actions

- (IBAction)showPublicHubsView:(id)sender
{
    [mainWindowController openHublist:self];
}

- (IBAction)showFriendsView:(id)sender
{
    [mainWindowController openFriends:self];
}

- (IBAction)showBookmarksView:(id)sender
{
    [mainWindowController openBookmarks:self];
}

- (IBAction)showDownloadsView:(id)sender
{
    [mainWindowController openDownloads:self];
}

- (IBAction)showUploadsView:(id)sender
{
    [mainWindowController openUploads:self];
}

- (IBAction)prevSidebarItem:(id)sender
{
    [mainWindowController prevSidebarItem];
}

- (IBAction)nextSidebarItem:(id)sender
{
    [mainWindowController nextSidebarItem];
}

#pragma mark -
#pragma mark NSApplication delegate methods

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    [[SPNetworkPortController sharedInstance] shutdown];
    
    SPLog(@"shutting down application");
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsKeepServerRunning] == NO) {
        /* read in pid from pidfile */
        sp_send_shutdown(sp);
        /* 'kill -0 pid' until sphubd is dead, with timeout */
    }
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsSessionRestore] == NO) {
		// if the pref for autconnecting to last connected hubs is off, make sure 
		// we clear any currently cached list on shutdown.
		[[NSUserDefaults standardUserDefaults] setObject:[NSArray array] forKey:SPPrefsLastConnectedHubs];
	}
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication
                    hasVisibleWindows:(BOOL)flag
{
    if (!flag)
        [[mainWindowController window] makeKeyAndOrderFront:nil];

	return NO;
}

#pragma mark -
#pragma mark Notifications

- (void)initCompletionNotification:(NSNotification *)aNotification
{
    int level = [[[aNotification userInfo] objectForKey:@"level"] intValue];

    NSLog(@"got init completion level %i", level);
    if (level == 25)
    {
        [initMessage setStringValue:@"Setting up port configuration..."];
        
        // setup port forwarding stuff, if it is on in prefs.
        if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutomaticPortForwarding])
            [[SPNetworkPortController sharedInstance] startup];
        
        // advance to the next level...
        sendNotification(SPNotificationInitCompletion, @"level", [NSNumber numberWithInt:50], nil);
    }
    else if (level == 50)
    {
        [initMessage setStringValue:@"Connecting to backend server..."];
        
        /* create the server messages panel, but don't show it
         * this is needed, 'cause it must subscribe to statusMessage notifications */
        [SPMessagePanel sharedMessagePanel];

        /* [menuItemRecentHubs setEnabled:NO]; */
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(versionResultReceived:)
                                                     name:@"DTCVMResultNotification"
                                                   object:nil];
    
        registerSPTransformers();

        /* Allocate the preference controller, but don't show it. This is needed
         * because sphubd will send messages that the prefs controller is listening
         * to. */
        [SPPreferenceController sharedPreferences];

        [self connectToBackendServer:self];
    }
    else if(level == 100)
    {
        [initMessage setStringValue:@"Scanning shared folders..."];

        /* register shared paths with sphubd */
        NSArray *sharedPaths = [[NSUserDefaults standardUserDefaults] arrayForKey:SPPrefsSharedPaths];
        NSEnumerator *e = [sharedPaths objectEnumerator];
        NSString *sharedPath;
        while ((sharedPath = [e nextObject]) != nil)
            sp_send_add_shared_path(sp, [sharedPath UTF8String]);

        sp_send_forget_search(sp, 0);
        sp_send_transfer_stats_interval(sp, 1);
        if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutodetectIPAddress] == NO) {
            sp_send_set_ip_address(sp,
                [[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsExternalIPAddress] UTF8String]);
        }
        sp_send_set_allow_hub_ip_override(sp, [[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAllowHubIPOverride]);

        [self setSlots:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsUploadSlots]
                perHub:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsUploadSlotsPerHub]];

        [self setRescanShareInterval:[[NSUserDefaults standardUserDefaults] floatForKey:SPPrefsRescanShareInterval] * 3600];
        [self setFollowHubRedirects:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsFollowHubRedirects]];
        [self setAutoSearchNewSources:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutoSearchNewSources]];
        [self setHashingPriority:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsHashingPriority]];
        [self setDownloadFolder:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDownloadFolder]];
        [self setIncompleteFolder:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsIncompleteFolder]];
    }
    else if(level == 200)
    {
        /* remove startup window and show main window */
        [initWindow orderOut:self];

        [mainWindowController showWindow:self];
        [self checkDownloadFolder];

        [[SPBookmarkController sharedBookmarkController] autoConnectBookmarks];
        [mainWindowController restoreLastHubSession];
    }
}

- (void)serverDiedAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertSecondButtonReturn) {
        [[NSApplication sharedApplication] terminate:self];
    }
    else {
        [self setupSphubdConnection];
    }
}

- (void)serverDiedNotification:(NSNotification *)aNotification
{
    SPLog(@"server died!");

    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle:@"Restart"];
    [alert addButtonWithTitle:@"Quit"];
    [alert setMessageText:@"Server has unexpectedly died"];
    [alert setInformativeText:@"You've found a bug ;-)"];
    [alert setAlertStyle:NSCriticalAlertStyle];

    /* Restarting requires us to reset some status, eg deallocate all hubs,
     * since this is not yet implemented, leave it disabled for now
     */
    [[[alert buttons] objectAtIndex:0] setEnabled:NO];

    [alert beginSheetModalForWindow:[mainWindowController window] modalDelegate:self
                     didEndSelector:@selector(serverDiedAlertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (void)storedFilelistsNotification:(NSNotification *)aNotification
{
    NSArray *nicks = [[aNotification userInfo] objectForKey:@"nicks"];

    NSEnumerator *e = [nicks objectEnumerator];
    NSString *nick;
    while ((nick = [e nextObject]) != nil) {
        [self addRecentFilelist:nick];
    }
}

- (void)openRecentFilelist:(id)sender
{
    NSString *nick = [sender representedObject];

    /* if this is truly a recent filelist, it won't actually be downloaded by
     * sphubd */
    [self downloadFilelistFromUser:nick onHub:nil forceUpdate:NO autoMatch:NO];
}

- (void)addRecentFilelist:(NSString *)nick
{
    NSMenuItem *browseMenuItem = [menuFilelists itemWithTitle:nick];
    if (browseMenuItem == nil) {
        browseMenuItem = (NSMenuItem *)[menuFilelists addItemWithTitle:nick
                                                                action:@selector(openRecentFilelist:)
                                                         keyEquivalent:@""];
    }
    [browseMenuItem setTarget:self];
    [browseMenuItem setRepresentedObject:nick];
}

- (void)filelistFinishedNotification:(NSNotification *)aNotification
{
    NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];
    [self addRecentFilelist:nick];
}

@end

