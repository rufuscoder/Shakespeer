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

#import "SPPreferenceController.h"
#import "SPApplicationController.h"
#import "SPNetworkPortController.h"
#import "SPLog.h"
#import "SPUserDefaultKeys.h"
#import "SPNotificationNames.h"
#import "NSMenu-MassRemovalAdditions.h"

#include "test_connection.h"

static float ToolbarHeightForWindow(NSWindow *window);

/* this code is from the apple documentation... */
static float ToolbarHeightForWindow(NSWindow *window)
{
    NSToolbar *toolbar = [window toolbar];
    float toolbarHeight = 0.0;
    NSRect windowFrame;

    if (toolbar && [toolbar isVisible]) {
        windowFrame = [NSWindow contentRectForFrameRect:[window frame]
                                              styleMask:[window styleMask]];
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[window contentView] frame]);
    }

    return toolbarHeight;
}

@implementation SPPreferenceController

- (id)init
{
    if ((self = [super initWithWindowNibName:@"Preferences"])) {
        // Define preference labels and identifiers
        prefItems = [[NSDictionary alloc] initWithObjectsAndKeys:
                     @"Identity", @"IdentityItem",
                     @"Share", @"ShareItem",
                     @"Network", @"NetworkItem",
                     @"Advanced", @"AdvancedItem",
                     nil];
        
        // Define default download locations
        predefinedDownloadLocations = [[NSArray alloc] initWithObjects:
                                       @"~/Desktop",
                                       @"~/Documents",
                                       @"~/Movies",
                                       @"~/Music",
                                       @"~/Pictures",
                                       nil];
        
        // Load shared paths
        sharedPaths = [[NSMutableArray alloc] init];
        [self setTotalShareSize:0LL];
        
        duplicatePaths = [[NSMutableSet alloc] init];
        
        NSEnumerator *e = [[[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths] objectEnumerator];
        NSString *path;
        while ((path = [e nextObject]) != nil) {
            [self addSharedPathsPath:path];
        }
        
        // Register for notifications. Note that the responding methods can not contain any code that interacts with objects in the nib-file, since they are not loaded yet. Any such code should go in the windowDidLoad method instead.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(shareStatsNotification:)
                                                     name:SPNotificationShareStats
                                                   object:nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(shareDuplicateFoundNotification:)
                                                     name:SPNotificationShareDuplicateFound
                                                   object:nil];
    }
    
    return self;
}

- (void)windowDidLoad
{
    // Setup toolbar
    blankView = [[NSView alloc] init];
    prefsToolbar = [[NSToolbar alloc] initWithIdentifier:@"prefsToolbar"];
    [prefsToolbar autorelease];
    [prefsToolbar setDelegate:self];
    [prefsToolbar setAllowsUserCustomization:NO];
    [prefsToolbar setAutosavesConfiguration:NO];
    [[self window] setToolbar:prefsToolbar];
    
    // Load last viewed pane
    [self switchToItem:[[NSUserDefaults standardUserDefaults] objectForKey:@"lastPrefPane"]];
    
    // Localize predefined download location names and add icons
    NSEnumerator *e = [predefinedDownloadLocations objectEnumerator];
    NSString *path;
    while ((path = [e nextObject]) != nil) {
        NSString *name = [path lastPathComponent];
        
        // Set the icon and name
        [[downloadFolderButton itemWithTitle:name] setImage:[self smallIconForPath:path]];
        [[downloadFolderButton itemWithTitle:name] setTitle:[[NSFileManager defaultManager] displayNameAtPath:path]];
    }
    
    // Set name and icon for current download folder
    [self updateNameAndIconForDownloadFolder];
    
    // Set name and icon for current incomplete folder
    [self updateNameAndIconForIncompleteFolder];
    
    // Add a contextual menu to the share table, for revealing duplicates
    [sharedPathsTable setTarget:self];
    [sharedPathsTable setMenu:duplicatePathsMenu];
    
    // Make a list of all applications that can handle magnet links
    NSArray *handlers = [(NSArray *)LSCopyAllHandlersForURLScheme(CFSTR("magnet")) autorelease];
    if (handlers) {
        [magnetHandlerMenu removeAllItems];
        
        // We'll remember the default handler first of all, it can then be
        // selected immediately when inserted to save us from an additional loop
        // later. If no default handler is found we'll set ShakesPeer as the default handler.
        NSString *defaultHandlerId = [(NSString *)LSCopyDefaultHandlerForURLScheme(CFSTR("magnet")) autorelease];
        if (!defaultHandlerId) {
            NSString *shakesPeerIdentifier = [[NSBundle mainBundle] bundleIdentifier];
            LSSetDefaultHandlerForURLScheme(CFSTR("magnet"), (CFStringRef)shakesPeerIdentifier);
        }
        
        NSEnumerator *e = [handlers objectEnumerator];
        NSString *identifier = nil;
        while ((identifier = [e nextObject])) {
            NSString *absolutePath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:identifier];
            NSString *title = [[NSFileManager defaultManager] displayNameAtPath:absolutePath];
            
            NSMenuItem *newItem = [[NSMenuItem alloc] initWithTitle:title
                                                             action:@selector(setDefaultMagnetHandler:)
                                                      keyEquivalent:@""];
            [newItem setImage:[self smallIconForPath:absolutePath]];
            [newItem setRepresentedObject:identifier];
            
            [magnetHandlerMenu addItem:newItem];
            
            // Select it if it was the default one
            if ([identifier isEqualToString:defaultHandlerId])
                [magnetHandlerButton selectItem:newItem];
            
            [newItem release];
        }
    }
    else {
        NSLog(@"No magnet link handlers found");
    }
    
    [self setWindowFrameAutosaveName:@"PreferenceWindow"];
}

+ (SPPreferenceController *)sharedPreferences
{
    static SPPreferenceController *sharedPreferenceController = nil;
    if (sharedPreferenceController == nil) {
        sharedPreferenceController = [[SPPreferenceController alloc] init];
    }
    return sharedPreferenceController;
}

- (void)dealloc
{
    [sharedPaths release];
    [predefinedDownloadLocations release];
    [prefItems release];
    [super dealloc];
}

- (void)show
{
    [[self window] makeKeyAndOrderFront:self];
}

- (void)close
{
    [[self window] performClose:self];
}

- (BOOL)isKeyWindow
{
    return [[self window] isKeyWindow];
}

#pragma mark -
#pragma mark Toolbar and preference panes

- (void)resizeWindowToSize:(NSSize)newSize
{
    NSRect aFrame;

    float newHeight = newSize.height + ToolbarHeightForWindow([self window]);
    float newWidth = newSize.width;

    aFrame = [NSWindow contentRectForFrameRect:[[self window] frame]
                                     styleMask:[[self window] styleMask]];

    aFrame.origin.y += aFrame.size.height;
    aFrame.origin.y -= newHeight;
    aFrame.size.height = newHeight;
    aFrame.size.width = newWidth;

    aFrame = [NSWindow frameRectForContentRect:aFrame styleMask:[[self window] styleMask]];

    [[self window] setFrame:aFrame display:YES animate:YES];
}

- (void)switchToView:(NSView *)view
{
    NSSize newSize = [view frame].size;
    
    [[self window] setContentView:blankView];
    [self resizeWindowToSize:newSize];
    [[self window] setContentView:view];
}

- (void)switchToItem:(id)item
{
    NSView *view = nil;
    NSString *identifier;
    
    // If the call is from a toolbar button, the sender will be an NSToolbarItem and we will need to fetch its itemIdentifier. If we want to call this method by hand, we can send it an NSString which will be used instead.
    if ([item respondsToSelector:@selector(itemIdentifier)])
        identifier = [item itemIdentifier];
    else
        identifier = item;
    
    if ([identifier isEqualToString:@"IdentityItem"]) {
        view = identityView;
    }
    else if ([identifier isEqualToString:@"ShareItem"]) {
        view = sharesView;
    }
    else if ([identifier isEqualToString:@"NetworkItem"]) {
        view = networkView;
    }
    else if ([identifier isEqualToString:@"AdvancedItem"]) {
        view = advancedView;
    }
    
    if (view) {
        [self switchToView:view];
        [prefsToolbar setSelectedItemIdentifier:identifier];
        [[NSUserDefaults standardUserDefaults] setObject:identifier forKey:@"lastPrefPane"];
    }
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

    [item setTarget:self];
    [item setLabel:[prefItems objectForKey:itemIdentifier]];
    [item setPaletteLabel:[item label]];
    [item setImage:[NSImage imageNamed:itemIdentifier]];
    [item setAction:@selector(switchToItem:)];
    
    return [item autorelease];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    if (toolbar == prefsToolbar) {
        return [prefItems allKeys];
    }
    return nil;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    if (toolbar == prefsToolbar) {
        return [prefItems allKeys];
    }
    return nil;
}

-(NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    if (toolbar == prefsToolbar) {
        return [prefItems allKeys];
    }
    return nil;
}

#pragma mark -
#pragma mark Notifications

- (void)shareStatsNotification:(NSNotification *)aNotification
{
    NSString *path = [[aNotification userInfo] objectForKey:@"path"];
    
    if ([path isEqualToString:@""]) {
        uint64_t size = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
        [self setTotalShareSize:size];
        return;
    }
    
    NSEnumerator *e = [sharedPaths objectEnumerator];
    NSMutableDictionary *dict;
    while ((dict = [e nextObject]) != nil) {
        if ([[dict objectForKey:@"path"] isEqualToString:path]) {
            uint64_t size = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
            uint64_t totsize = [[[aNotification userInfo] objectForKey:@"totsize"] unsignedLongLongValue];
            uint64_t dupsize = [[[aNotification userInfo] objectForKey:@"dupsize"] unsignedLongLongValue];
            uint64_t uniqsize = totsize - dupsize;
            unsigned nfiles = [[[aNotification userInfo] objectForKey:@"nfiles"] intValue];
            unsigned ntotfiles = [[[aNotification userInfo] objectForKey:@"ntotfiles"] intValue];
            unsigned nduplicates = [[[aNotification userInfo] objectForKey:@"nduplicates"] intValue];
            unsigned nunique = ntotfiles - nduplicates;
            unsigned percentComplete = (unsigned)(100 * ((double)size / (uniqsize ? uniqsize : 1)));
            
            [self willChangeValueForKey:@"size"];
            [self willChangeValueForKey:@"nfiles"];
            [self willChangeValueForKey:@"percentComplete"];
            [self willChangeValueForKey:@"nleft"];
            [self willChangeValueForKey:@"nduplicates"];
            
            [dict setObject:[NSNumber numberWithUnsignedLongLong:size] forKey:@"size"];
            [dict setObject:[NSNumber numberWithInt:nfiles] forKey:@"nfiles"];
            [dict setObject:[NSNumber numberWithInt:percentComplete] forKey:@"percentComplete"];
            [dict setObject:[NSNumber numberWithInt:nunique - nfiles] forKey:@"nleft"];
            [dict setObject:[NSNumber numberWithInt:nduplicates] forKey:@"nduplicates"];
            
            [self didChangeValueForKey:@"nduplicates"];
            [self didChangeValueForKey:@"nleft"];
            [self didChangeValueForKey:@"percentComplete"];
            [self didChangeValueForKey:@"nfiles"];
            [self didChangeValueForKey:@"size"];
        }
    }
}

- (void)shareDuplicateFoundNotification:(NSNotification *)aNotification
{
    NSString *path = [[aNotification userInfo] valueForKey:@"path"];
    [duplicatePaths addObject:path];
}

- (void)revealDuplicateInFinder:(NSMenuItem *)selectedItem
{
    NSString *duplicatePath = [selectedItem representedObject];
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:duplicatePath]) {
        if (![[NSWorkspace sharedWorkspace] selectFile:duplicatePath inFileViewerRootedAtPath:@""])
            NSLog(@"Couldn't reveal duplicate at %@", duplicatePath);
    }
    else {
        NSLog(@"Couldn't find duplicate at %@", duplicatePath);
    }
}

#pragma mark -
#pragma mark Duplicate menu delegate methods

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    if (menu == duplicatePathsMenu) {
        // Empty the menu, except for "Duplicates" item at top
        [menu removeAllItemsButFirst];
        
        // Populate the menu from our set of duplicate paths
        NSEnumerator *e = [[duplicatePaths allObjects] objectEnumerator];
        NSString *currentPath = nil;
        
        while ((currentPath = [e nextObject])) {
            NSMenuItem* newItem = [[NSMenuItem alloc] initWithTitle:[currentPath lastPathComponent]
                                                             action:@selector(revealDuplicateInFinder:)
                                                      keyEquivalent:@""];
            [newItem setRepresentedObject:currentPath];
            [menu addItem:newItem];
            [newItem release];
        }
    }
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)addSharedPath:(id)sender
{
    NSOpenPanel *op = [NSOpenPanel openPanel];
    
    [op setCanChooseDirectories:YES];
    [op setCanChooseFiles:NO];
    [op setAllowsMultipleSelection:YES];
    
    if ([op runModalForTypes:nil] == NSOKButton) {
        NSEnumerator *e = [[op filenames] objectEnumerator];
        NSString *path;
        
        while ((path = [e nextObject])) {
            [[SPApplicationController sharedApplicationController] addSharedPath:path];
            
            NSArray *tmp = [[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths];
            [[NSUserDefaults standardUserDefaults] setObject:[tmp arrayByAddingObject:path]
                                                      forKey:SPPrefsSharedPaths];
            
            [self addSharedPathsPath:path];
            
        }
    }
}

- (IBAction)removeSharedPath:(id)sender
{
    NSArray *selectedPaths = [sharedPathsController selectedObjects];
    NSEnumerator *enumerator = [selectedPaths objectEnumerator];
    NSDictionary *record;
    
    while ((record = [enumerator nextObject])) {
        [[SPApplicationController sharedApplicationController] removeSharedPath:[record objectForKey:@"path"]];
        
        NSMutableArray *x = [[[NSMutableArray alloc] init] autorelease];
        [x setArray:[[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths]];
        [x removeObject:[record objectForKey:@"path"]];
        [[NSUserDefaults standardUserDefaults] setObject:x forKey:SPPrefsSharedPaths];
        
        NSEnumerator *e = [sharedPaths objectEnumerator];
        NSMutableDictionary *dict;
        while ((dict = [e nextObject]) != nil) {
            if ([[dict objectForKey:@"path"] isEqualToString:[record objectForKey:@"path"]]) {
                [self willChangeValueForKey:@"sharedPaths"];
                [sharedPaths removeObject:dict];
                [self didChangeValueForKey:@"sharedPaths"];
                break;
            }
        }
    }
}

- (IBAction)updateSharedPaths:(id)sender
{
    NSArray *selectedPaths = [sharedPathsController selectedObjects];
    NSEnumerator *enumerator = [selectedPaths objectEnumerator];
    NSDictionary *record;
    while ((record = [enumerator nextObject])) {
        [[SPApplicationController sharedApplicationController] addSharedPath:[record objectForKey:@"path"]];
    }
}

- (IBAction)selectDownloadFolder:(id)sender
{
    NSString *path = nil;
    
    switch([sender tag]) {
        case 0: // Current folder
            path = [sender title]; break;
        case 1:
            path = @"~/Desktop"; break;
        case 2:
            path = @"~/Documents"; break;
        case 3:
            path = @"~/Movies"; break;
        case 4:
            path = @"~/Music"; break;
        case 5:
            path = @"~/Pictures"; break;
        case 6: // Other…
            path = [[self selectFolder] stringByAbbreviatingWithTildeInPath];
            [downloadFolderButton selectItemAtIndex:0];
            break;
    }
    if (path) {
        NSLog(@"Changed download folder to: %@", path);
        [[NSUserDefaults standardUserDefaults] setObject:path forKey:SPPrefsDownloadFolder];
        [[SPApplicationController sharedApplicationController] setDownloadFolder:path];
        [self updateNameAndIconForDownloadFolder];
    }
}

- (IBAction)selectIncompleteFolder:(id)sender
{
    NSString *path = nil;
    
    switch([sender tag]) {
        case 0: // Current folder
            path = [sender title]; break;
        case 1: // Other…
        {
            path = [[self selectFolder] stringByAbbreviatingWithTildeInPath];
            [incompleteFolderButton selectItemAtIndex:0];
            break;
        }
    }
    if (path) {
        NSLog(@"Changed incomplete folder to: %@", path);
        [[NSUserDefaults standardUserDefaults] setObject:path forKey:SPPrefsIncompleteFolder];
        [[SPApplicationController sharedApplicationController] setIncompleteFolder:path];
        [self updateNameAndIconForIncompleteFolder];
    }
}

- (IBAction)setPort:(id)sender
{
    BOOL autoPortForwarding = [[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutomaticPortForwarding];
    [[SPApplicationController sharedApplicationController] setPort:[sender intValue]];
    if (autoPortForwarding)
        [[SPNetworkPortController sharedInstance] changePort:[sender intValue]];
}

- (IBAction)setIPAddress:(id)sender
{
    if ([IPAddressField isEnabled])
        [[SPApplicationController sharedApplicationController] setIPAddress:[IPAddressField stringValue]];
    else
        [[SPApplicationController sharedApplicationController] setIPAddress:@""];
}

- (IBAction)setAutomaticPortForwarding:(id)sender
{
    BOOL autoPortForwarding = [[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutomaticPortForwarding];
    if (!autoPortForwarding)
        [[SPNetworkPortController sharedInstance] shutdown];
    else
        [[SPNetworkPortController sharedInstance] startup];
}

- (IBAction)setAllowHubIPOverride:(id)sender
{
    BOOL allowOverride = [[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAllowHubIPOverride];
    [[SPApplicationController sharedApplicationController] setAllowHubIPOverride:allowOverride];
}

- (IBAction)updateUserInfo:(id)sender
{
    NSString *email = [emailField stringValue];
    NSString *description = [descriptionField stringValue];
    NSString *speed = [speedField stringValue];
    [[SPApplicationController sharedApplicationController] updateUserEmail:email
                                                               description:description
                                                                     speed:speed];
}

- (IBAction)testConnection:(id)sender
{
    [self setPort:portField];
    int port = [portField intValue];
    
    [testResults setTextColor:[NSColor blackColor]];
    [testResults setStringValue:[NSString stringWithFormat:@"Testing port %i", port]];
    [testConnectionProgress startAnimation:self];
    
    if (testInProgress == NO) {
        [NSThread detachNewThreadSelector:@selector(testConnectionThread:) toTarget:self withObject:nil];
    }
    
}

- (IBAction)setSlots:(id)sender
{
    [[SPApplicationController sharedApplicationController] setSlots:[slotsField intValue]
                                                             perHub:[slotsPerHubButton state]];
}

- (IBAction)setConnectionMode:(id)sender
{
    int passive = [sender indexOfSelectedItem];
    
    if (passive)
        [[SPApplicationController sharedApplicationController] setPassiveMode];
    else
        [self setPort:portField];
}

- (IBAction)setLogLevel:(id)sender
{
    [[SPApplicationController sharedApplicationController] setLogLevel:[sender title]];
}

- (IBAction)setRescanShareInterval:(id)sender
{
    [[SPApplicationController sharedApplicationController] setRescanShareInterval:[rescanShareField floatValue] * 3600];
}

- (IBAction)setFollowRedirects:(id)sender
{
    [[SPApplicationController sharedApplicationController] setFollowHubRedirects:[sender state] == NSOnState ? 1 : 0];
}

- (IBAction)togglePauseHashing:(id)sender
{
    if (hashingPaused) {
        [[SPApplicationController sharedApplicationController] resumeHashing];
        [sender setTitle:@"Pause hashing"];
        hashingPaused = NO;
    }
    else {
        [[SPApplicationController sharedApplicationController] pauseHashing];
        [sender setTitle:@"Resume hashing"];
        hashingPaused = YES;
    }
}

- (IBAction)setAutoSearchNewSources:(id)sender
{
    [[SPApplicationController sharedApplicationController] setAutoSearchNewSources:[sender state] == NSOnState ? 1 : 0];
}

- (IBAction)setHashingPriority:(id)sender
{
    [[SPApplicationController sharedApplicationController] setHashingPriority:[[sender selectedItem] tag]];
}

- (IBAction)setHublistURL:(id)sender
{
    if ([hublistsComboBox indexOfItemWithObjectValue:[sender stringValue]] == NSNotFound) {
        [hublistsController insertObject:[sender stringValue] atArrangedObjectIndex:0];
    }
}

#pragma mark -

- (void)addSharedPathsPath:(NSString *)aPath
{
    NSMutableDictionary *newPath = [NSMutableDictionary dictionaryWithObjectsAndKeys:aPath, @"path",
                                                [NSNumber numberWithUnsignedLongLong:0L], @"size",
                                                             [NSNumber numberWithInt:0], @"nfiles",
                                                             [NSNumber numberWithInt:0], @"percentComplete",
                                                             [NSNumber numberWithInt:0], @"nleft",
                                                             [NSNumber numberWithInt:0], @"nduplicates", nil];

    [self willChangeValueForKey:@"sharedPaths"];
    [sharedPaths addObject:newPath];
    [self didChangeValueForKey:@"sharedPaths"];
}

- (void)setTotalShareSize:(uint64_t)aNumber
{
    totalShareSize = aNumber;
}

- (void)updateAllSharedPaths
{
    // Empty the list of duplicates
    [duplicatePaths removeAllObjects];
    
    NSEnumerator *enumerator = [sharedPaths objectEnumerator];
    NSDictionary *record;
    while ((record = [enumerator nextObject])) {
        [[SPApplicationController sharedApplicationController] addSharedPath:[record objectForKey:@"path"]];
    }
}

- (NSString *)selectFolder
{
    NSOpenPanel *op = [NSOpenPanel openPanel];

    [op setCanChooseDirectories:YES];
    [op setCanChooseFiles:NO];
    [op setAllowsMultipleSelection:NO];

    if ([op runModalForTypes:nil] == NSOKButton) {
        return [[op filenames] objectAtIndex:0];
    }
    
    return nil;
}

- (void)updateTestConnectionResultFromErrors:(NSNumber *)wrappedStatus
{
    int status = [wrappedStatus intValue];

    [testConnectionProgress stopAnimation:self];
    if (status == TC_RET_OK) {
        [testResults setTextColor:[NSColor colorWithCalibratedRed:0.0 green:0.78 blue:0.0 alpha:1.0]];
        [testResults setStringValue:@"Connection is OK"];
    }
    else {
        [testResults setTextColor:[NSColor colorWithCalibratedRed:0.78 green:0.0 blue:0.0 alpha:1.0]];
        NSString *errmsg = nil;
        if (status & TC_RET_PRIVPORT) {
            errmsg = @"Refused to test privileged port";
        }
        else if ((status & TC_RET_TCP_FAIL) || (status & TC_RET_UDP_FAIL)) {
            errmsg = @"TCP and/or UDP port unreachable";
        }
        else {
            errmsg = @"Internal error";
        }
        [testResults setStringValue:errmsg];
    }
}

- (void)testConnectionThread:(id)args
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    testInProgress = YES;
    int port = [portField intValue];

    int status = test_connection(port);

    NSNumber *wrappedStatus = [NSNumber numberWithInt:status];

    [self performSelectorOnMainThread:@selector(updateTestConnectionResultFromErrors:)
                           withObject:wrappedStatus
                        waitUntilDone:YES];

    testInProgress = NO;
    [pool release];
}

- (NSImage *)smallIconForPath:(NSString *)path
{
    NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[path stringByExpandingTildeInPath]];
    [icon setSize:NSMakeSize(16, 16)];
    
    return icon;
}

- (void)updateNameAndIconForDownloadFolder
{
    NSString *downloadFolder = [[NSUserDefaults standardUserDefaults] objectForKey:SPPrefsDownloadFolder];
    [[downloadFolderButton itemAtIndex:0] setTitle:downloadFolder];
    [[downloadFolderButton itemAtIndex:0] setImage:[self smallIconForPath:downloadFolder]];
}

- (void)updateNameAndIconForIncompleteFolder
{
    NSString *incompleteFolder = [[NSUserDefaults standardUserDefaults] objectForKey:SPPrefsIncompleteFolder];
    [[incompleteFolderButton itemAtIndex:0] setTitle:incompleteFolder];
    [[incompleteFolderButton itemAtIndex:0] setImage:[self smallIconForPath:incompleteFolder]];
}

- (void)setDefaultMagnetHandler:(id)sender
{
    NSString *magnetHandlerIdentifier = [sender representedObject];
    LSSetDefaultHandlerForURLScheme(CFSTR("magnet"), (CFStringRef)magnetHandlerIdentifier);
    NSLog(@"Setting %@ as default magnet link handler", magnetHandlerIdentifier);
}

@end
