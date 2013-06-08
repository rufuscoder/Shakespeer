/* vim: ft=objc
 *
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#import "SPSearchWindowController.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"
#import "SPTransformers.h"
#import "NSMenu-UserCommandAdditions.h"
#import "NSStringExtensions.h"
#import "SPLog.h"
#include "util.h"

#import "SPUser.h"
#import "SPUserCommand.h"
#import "SPOutlineView.h"
#import "SPSideBar.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

@implementation SPSearchWindowController

- (id)initWithString:(NSString *)aSearchString size:(NSString *)aSearchSize
     sizeRestriction:(int)aSizeRestriction type:(int)aSearchType
          hubAddress:(NSString *)aHubAddress
{
    self = [super initWithWindowNibName:@"SearchWindow"];
    if (self) {
        searchResults = [[NSMutableArray alloc] init];
        arrangedSearchResults = [searchResults retain];
        searchResultsDelta = [[NSMutableArray alloc] init];
        searchResultsIndex = [[NSMutableDictionary alloc] init];

        tag = -1;

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(searchResponseNotification:)
                                                     name:SPNotificationSearchResponse
                                                   object:nil];

        needUpdating = NO;

        updateTimer = [NSTimer scheduledTimerWithTimeInterval:2.0
                                                       target:self
                                                     selector:@selector(updateSearchResults:)
                                                     userInfo:nil
                                                      repeats:YES];

        [self newSearchWithString:aSearchString
                             size:aSearchSize
                  sizeRestriction:aSizeRestriction
                             type:aSearchType
                       hubAddress:aHubAddress];
    }
    
    return self;
}

- (void)newSearchWithString:(NSString *)aSearchString size:(NSString *)aSearchSize
            sizeRestriction:(int)aSizeRestriction type:(int)aSearchType
                 hubAddress:(NSString *)aHubAddress
{
    [searchResults removeAllObjects];
    [searchResultsDelta removeAllObjects];
    [searchResultsIndex removeAllObjects];
    needUpdating = (searchString == aSearchString);

    if (tag != -1) {
        [[SPApplicationController sharedApplicationController] forgetSearch:tag];
        tag = -1;
    }

    if (searchString != aSearchString) {
        [self willChangeValueForKey:@"title"];
        [searchString release];
        searchString = [aSearchString copy];
        [self didChangeValueForKey:@"title"];
    }

    if (searchSize != aSearchSize) {
        [searchSize release];
        searchSize = [aSearchSize copy];
    }

    sizeRestriction = aSizeRestriction;
    searchType = aSearchType;

    if (hubAddress != aHubAddress) {
        [hubAddress release];
        hubAddress = [aHubAddress retain];
    }

    [self doSearch:self];
}

- (void)updateSearchResults:(NSTimer *)aTimer
{
    if (needUpdating) {
        [searchResults addObjectsFromArray:searchResultsDelta];
        if (searchResults != arrangedSearchResults) {
            [self filterSearchResults:lastFilterString flags:lastFlags];
        }
        [self sortArray:arrangedSearchResults usingDescriptors:[searchResultTable sortDescriptors]];
        [searchResultTable reloadData];
        [searchResultsDelta removeAllObjects];
        needUpdating = NO;
        [[SPMainWindowController sharedMainWindowController] highlightItem:self];
        [statusField setStringValue:[NSString stringWithFormat:@"%lu matches", [searchResultsIndex count]]];
    }
}

- (void)awakeFromNib 
{
    [searchResultTable setTarget:self];
    [searchResultTable setDoubleAction:@selector(doubleClickAction:)];
    
    [[searchResultTable outlineTableColumn] setDataCell:[[[NSBrowserCell alloc] init] autorelease]];
    [[searchResultTable outlineTableColumn] setMinWidth:70.0];
    [statusField setStringValue:[NSString stringWithFormat:@"%lu matches", [searchResults count]]];
    
    [tcNick retain];
    [tcSize retain];
    [tcTTH retain];
    [tcSlots retain];
    [tcPath retain];
    [tcHub retain];
    [tcSpeed retain];
    
    NSArray *tcs = [searchResultTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];
        [[tc dataCell] setFont:[NSFont systemFontOfSize:[[NSUserDefaults standardUserDefaults] floatForKey:@"fontSize"]]];
            
        if (tc == tcNick)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcSize)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcTTH)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcSlots)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcPath)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcHub)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcSpeed)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
    }
        
    [[searchResultTable headerView] setMenu:columnsMenu];
    
    int flags = 0;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"requireOpenSlots"])
        flags |= 1;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"requireTTH"])
        flags |= 2;
    
    lastFlags = flags;
    [self filterSearchResults:lastFilterString flags:lastFlags];
}

- (void)dealloc
{
    NSLog(@"SPSearchWindowController:dealloc [%@]", [self title]);
    
    [searchString release];
    [searchSize release];
    [searchResults release];
    [searchResultsDelta release];
    [hubAddress release];
    
    [tcNick release];
    [tcSize release];
    [tcTTH release];
    [tcSlots release];
    [tcPath release];
    [tcHub release];
    [tcSpeed release];
    
    [super dealloc];
}

#pragma mark -
#pragma mark Sidebar support

- (void)unbindControllers
{
    [[SPApplicationController sharedApplicationController] forgetSearch:[self searchID]];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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
    return [NSImage imageNamed:@"TableFind"];
}

- (NSString *)title
{
    return searchString;
}

- (NSString *)sectionTitle
{
    return @"Searches";
}

- (NSMenu *)menu
{
    return contextMenu;
}

- (BOOL)isHighlighted
{
    return highlighted;
}

- (void)setHighlighted:(BOOL)aFlag
{
    highlighted = aFlag;
}

- (int)searchID
{
    return tag;
}

#pragma mark -
#pragma mark Sphubd notifications

- (void)searchResponseNotification:(NSNotification *)aNotification
{
    int theTag = [[[aNotification userInfo] objectForKey:@"tag"] intValue];
    if (theTag == tag) {
        NSString *filename = [[aNotification userInfo] objectForKey:@"filename"];
        uint64_t exactSize = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
        int openslots = [[[aNotification userInfo] objectForKey:@"openslots"] intValue];
        int totalslots = [[[aNotification userInfo] objectForKey:@"totalslots"] intValue];
        NSString *slots = [NSString stringWithFormat:@"%i/%i", openslots, totalslots];
        NSString *tth = [[aNotification userInfo] objectForKey:@"tth"];

        NSMutableDictionary *sr = [[[NSMutableDictionary alloc] init] autorelease];
        [sr setObject:[[filename componentsSeparatedByString:@"\\"] lastObject] forKey:@"filename"];
        [sr setObject:[filename stringByDeletingLastWindowsPathComponent] forKey:@"path"];
        [sr setObject:[[aNotification userInfo] objectForKey:@"nick"] forKey:@"nick"];
        [sr setObject:[NSNumber numberWithUnsignedLongLong:exactSize] forKey:@"exactSize"];
        [sr setObject:[[HumanSizeTransformer defaultHumanSizeTransformer] transformedValue:[sr objectForKey:@"exactSize"]] forKey:@"size"];
        [sr setObject:slots forKey:@"slots"];
        [sr setObject:[NSNumber numberWithFloat:(float)openslots/totalslots] forKey:@"slotsPercent"];
        [sr setObject:tth forKey:@"tth"];
        [sr setObject:[[aNotification userInfo] objectForKey:@"hubAddress"] forKey:@"hub"];
        [sr setObject:[NSNumber numberWithInt:[[[aNotification userInfo] objectForKey:@"type"] intValue]] forKey:@"type"];

        [sr setObject:[[[[aNotification userInfo] objectForKey:@"speed"] truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"speed"];
        [sr setObject:[[[sr objectForKey:@"filename"] truncatedString:NSLineBreakByTruncatingMiddle] autorelease] forKey:@"attributedFilename"];
        [sr setObject:[[[sr objectForKey:@"path"] truncatedString:NSLineBreakByTruncatingHead] autorelease] forKey:@"attributedPath"];
        [sr setObject:[[[sr objectForKey:@"nick"] truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"attributedNick"];
        [sr setObject:[[[sr objectForKey:@"tth"] truncatedString:NSLineBreakByTruncatingMiddle] autorelease] forKey:@"attributedTTH"];
        [sr setObject:[[[sr objectForKey:@"hub"] truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"attributedHub"];

        NSMutableDictionary *parent = nil;
        if ([tth length] > 0)
            parent = [searchResultsIndex objectForKey:tth];

        if (parent) {
            NSMutableArray *children = [parent objectForKey:@"children"];
            if (children == nil) {
                children = [[[NSMutableArray alloc] init] autorelease];
                NSMutableDictionary *parentCopy = [[[NSMutableDictionary alloc] init] autorelease];
                [parentCopy addEntriesFromDictionary:parent];
                [children addObject:parentCopy];
                [parent setObject:@"" forKey:@"slots"];
                [parent setObject:@"" forKey:@"hub"];
                [parent setObject:@"" forKey:@"attributedHub"];
                [parent setObject:@"" forKey:@"speed"];
                [parent setObject:@"" forKey:@"path"];
                [parent setObject:@"" forKey:@"attributedPath"];
                [parent setObject:[NSNumber numberWithFloat:0.0] forKey:@"slotsPercent"];
                [parent setObject:children forKey:@"children"];
            }
            [children addObject:sr];
            [parent setObject:[NSString stringWithFormat:@"%u users", [children count]] forKey:@"nick"];
            [sr setObject:[[[sr objectForKey:@"nick"] truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"attributedNick"];
        }
        else {
            [searchResultsIndex setObject:sr forKey:tth];
            [searchResultsDelta addObject:sr];
        }
        needUpdating = YES;
    }
}

- (void)executeSearchUserCommand:(id)sender
{
    SPUserCommand *uc = [sender representedObject];
    NSLog(@"executeSearchUserCommand: sender = %@, representedObject = %@",
            sender, uc);

    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    NSMutableArray *nicks = [NSMutableArray arrayWithCapacity:[selectedIndices count]];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];
        if ([sr objectForKey:@"children"] == nil) {
            if ([[uc address] isEqualToString:[sr objectForKey:@"hub"]]) {
                NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithCapacity:2];
                [parameters setObject:[sr objectForKey:@"nick"] forKey:@"nick"];
                [parameters setObject:[NSString stringWithFormat:@"%@\\%@", [sr objectForKey:@"path"], [sr objectForKey:@"filename"]]
                               forKey:@"file"];
                [nicks addObject:parameters];
            }
        }
        i = [selectedIndices indexGreaterThanIndex:i];
    }

    id h = [[SPMainWindowController sharedMainWindowController] hubWithAddress:[uc address]];
    [uc executeForNicks:nicks myNick:[h nick]];
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcNick; break;
        case 1: tc = tcSize; break;
        case 2: tc = tcTTH; break;
        case 3: tc = tcSlots; break;
        case 4: tc = tcPath; break;
        case 5: tc = tcHub; break;
        case 6: tc = tcSpeed; break;
    }
    if (tc == nil)
        return;
    
    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [searchResultTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [searchResultTable removeTableColumn:tc];
    }
}

- (IBAction)doSearch:(id)sender
{
    if ([searchString length] == 0)
        return;

    uint64_t size = 0ULL;
    if ([searchSize length] > 0) {
        NSScanner *scan = [NSScanner scannerWithString:searchSize];
        [scan scanLongLong:(int64_t *)&size];
        if ([scan isAtEnd] == NO) {
            NSString *suffix = [[searchSize substringFromIndex:[scan scanLocation]] uppercaseString];
            if ([suffix hasPrefix:@"K"])
                size *= 1024;
            else if ([suffix hasPrefix:@"M"])
                size *= 1024 * 1024;
            else if ([suffix hasPrefix:@"G"])
                size *= 1024 * 1024 * 1024;
        }
    }

    if (valid_tth([searchString UTF8String])) {
        if (hubAddress) {
            tag = [[SPApplicationController sharedApplicationController] searchHub:hubAddress
                                                                            forTTH:searchString];
        }
        else {
            tag = [[SPApplicationController sharedApplicationController] searchAllHubsForTTH:searchString];
        }
    }
    else {
        if (hubAddress) {
            tag = [[SPApplicationController sharedApplicationController] searchHub:hubAddress
                                                                         forString:searchString
                                                                              size:size
                                                                   sizeRestriction:sizeRestriction
                                                                          fileType:searchType];
        }
        else {
            tag = [[SPApplicationController sharedApplicationController] searchAllHubsForString:searchString
                                                                                           size:size
                                                                                sizeRestriction:sizeRestriction
                                                                                       fileType:searchType];
        }
    }
}

- (void)doubleClickAction:(id)sender
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    if (i != NSNotFound) {
        NSDictionary *item = [searchResultTable itemAtRow:i];
        if ([item objectForKey:@"children"]) {
            if ([searchResultTable isItemExpanded:item])
                [searchResultTable collapseItem:item];
            else
                [searchResultTable expandItem:item];
        }
        else {
            [self downloadFile:sender];
        }
    }
}

- (IBAction)downloadFile:(id)sender
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];

        NSString *targetPath = [sr objectForKey:@"filename"];
        NSString *sourcePath = [NSString stringWithFormat:@"%@\\%@", [sr objectForKey:@"path"],
                                         [sr objectForKey:@"filename"]];

        if ([[sr objectForKey:@"type"] intValue] == SHARE_TYPE_DIRECTORY) {
            [[SPApplicationController sharedApplicationController] downloadDirectory:sourcePath
                                                                            fromNick:[sr objectForKey:@"nick"]
                                                                               onHub:[sr objectForKey:@"hub"]
                                                                    toLocalDirectory:targetPath];
        }
        else {
            NSString *tth = [sr objectForKey:@"tth"];
            NSDictionary *parent = [searchResultsIndex objectForKey:tth];
            NSArray *children = [parent objectForKey:@"children"];
            if (children) {
                NSEnumerator *e = [children objectEnumerator];
                while ((sr = [e nextObject]) != nil) {
                    sourcePath = [NSString stringWithFormat:@"%@\\%@", [sr objectForKey:@"path"],
                                           [sr objectForKey:@"filename"]];
                    [[SPApplicationController sharedApplicationController] downloadFile:sourcePath
                                                                               withSize:[sr objectForKey:@"exactSize"]
                                                                               fromNick:[sr objectForKey:@"nick"]
                                                                                  onHub:[sr objectForKey:@"hub"]
                                                                            toLocalFile:targetPath
                                                                                    TTH:tth];
                }
            }
            else
            {
                [[SPApplicationController sharedApplicationController] downloadFile:sourcePath
                                                                           withSize:[sr objectForKey:@"exactSize"]
                                                                           fromNick:[sr objectForKey:@"nick"]
                                                                              onHub:[sr objectForKey:@"hub"]
                                                                        toLocalFile:targetPath
                                                                                TTH:tth];
            }
        }

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

- (IBAction)downloadParentDirectory:(id)sender
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    if (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];

        if ([sr objectForKey:@"children"] == nil) {
            NSString *sourceDirectory = [sr objectForKey:@"path"];
            NSString *targetPath = [sourceDirectory lastWindowsPathComponent];

            [[SPApplicationController sharedApplicationController] downloadDirectory:sourceDirectory
                                                                            fromNick:[sr objectForKey:@"nick"]
                                                                               onHub:[sr objectForKey:@"hub"]
                                                                    toLocalDirectory:targetPath];
        }
    }
}

- (void)processFilelist:(id)sender autoMatch:(BOOL)autoMatchFlag
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    NSMutableDictionary *requests = [NSMutableDictionary dictionaryWithCapacity:[selectedIndices count]];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];
        if ([sr objectForKey:@"children"] == nil) {
            [requests setObject:sr forKey:[sr objectForKey:@"nick"]];
        }
        i = [selectedIndices indexGreaterThanIndex:i];
    }

    NSEnumerator *e = [requests keyEnumerator];
    NSString *aNick;
    while ((aNick = [e nextObject]) != nil) {
        NSDictionary *sr = [requests objectForKey:aNick];
        [[SPApplicationController sharedApplicationController]
            downloadFilelistFromUser:aNick
                               onHub:[sr objectForKey:@"hub"]
                         forceUpdate:NO
                           autoMatch:autoMatchFlag];
    }
}

- (IBAction)getFilelist:(id)sender
{
    [self processFilelist:sender autoMatch:NO];
}

- (IBAction)autoMatchFilelist:(id)sender
{
    [self processFilelist:sender autoMatch:YES];
}

- (IBAction)copyTTH:(id)sender
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    if (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];
        NSString *tth = [sr objectForKey:@"tth"];
        if (tth) {
            NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
            [pasteboard setString:tth forType:NSStringPboardType];
        }
    }
}

- (IBAction)startPrivateChat:(id)sender
{
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    NSMutableDictionary *requests = [NSMutableDictionary dictionaryWithCapacity:[selectedIndices count]];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];
        if ([sr objectForKey:@"children"] == nil) {
            [requests setObject:sr forKey:[sr objectForKey:@"nick"]];
        }
        i = [selectedIndices indexGreaterThanIndex:i];
    }

    NSEnumerator *e = [requests keyEnumerator];
    NSString *aNick;
    while ((aNick = [e nextObject]) != nil) {
        NSDictionary *sr = [requests objectForKey:aNick];
        sendNotification(SPNotificationStartChat,
                /* TODO: we don't have any real own nick here... dunno */
                @"remote_nick", aNick,
                @"hubAddress", [sr objectForKey:@"hub"],
                nil);
    }
}

- (BOOL)filterString:(NSString *)filterString flags:(int)flags matchesItem:(NSDictionary *)item
{
    NSString *filename = [item objectForKey:@"filename"];
    NSString *nick = [item objectForKey:@"nick"];
    NSString *path = [item objectForKey:@"path"];

    if ((flags & 1) == 1) {
        if ([[item objectForKey:@"slotsPercent"] floatValue] == 0) {
            return NO;
        }
    }

    if ((flags & 2) == 2) {
        if ([(NSString *)[item objectForKey:@"tth"] length] == 0) {
            return NO;
        }
    }

    if ([filterString length] == 0 ||
       [filename rangeOfString:filterString options:NSCaseInsensitiveSearch].location != NSNotFound ||
       [nick rangeOfString:filterString options:NSCaseInsensitiveSearch].location != NSNotFound ||
       [path rangeOfString:filterString options:NSCaseInsensitiveSearch].location != NSNotFound) {
        return YES;
    }

    return NO;
}

- (BOOL)filterString:(NSString *)filterString flags:(int)flags matchesChildren:(NSArray *)children
{
    NSEnumerator *e = [children objectEnumerator];
    NSDictionary *item;
    while ((item = [e nextObject]) != nil) {
        if ([self filterString:filterString flags:flags matchesItem:item]) {
            return YES;
        }
    }
    return NO;
}

- (NSMutableArray *)filterOnString:(NSString *)filterString flags:(int)flags
{
    NSMutableArray *arrangedObjects = [NSMutableArray arrayWithCapacity:[searchResults count]];
    NSEnumerator *e = [searchResults objectEnumerator];
    NSDictionary *item;
    while ((item = [e nextObject]) != nil) {
        NSArray *children = [item objectForKey:@"children"];
        if ([self filterString:filterString flags:flags matchesItem:item] ||
           [self filterString:filterString flags:flags matchesChildren:children]) {
            [arrangedObjects addObject:item];
        }
    }
    return arrangedObjects;
}

- (void)filterSearchResults:(NSString *)filterString flags:(int)flags
{
    [arrangedSearchResults release];
    if ([filterString length] > 0 || flags > 0) {
        arrangedSearchResults = [self filterOnString:filterString flags:flags];
    }
    else {
        arrangedSearchResults = searchResults;
    }
    [arrangedSearchResults retain];
}

- (IBAction)filter:(id)sender
{
    NSString *filterString = [searchField stringValue];
    int flags = 0;
    if ([requireOpenSlotButton state] == NSOnState) {
        flags |= 1;
    }
    if ([requireTTHButton state] == NSOnState) {
        flags |= 2;
    }
    
    if (filterString != lastFilterString || flags != lastFlags) {
        if (filterString != lastFilterString) {
            [lastFilterString release];
            lastFilterString = [filterString retain];
        }
        lastFlags = flags;
        [self filterSearchResults:lastFilterString flags:flags];
        [searchResultTable reloadData];
    }
}

- (IBAction)relaunchSearch:(id)sender
{
    [self newSearchWithString:searchString
                         size:searchSize
              sizeRestriction:sizeRestriction
                         type:searchType
                   hubAddress:hubAddress];
}

#pragma mark -
#pragma mark NSOutlineView delegate methods

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (item == nil)
        return [arrangedSearchResults count];
    return [[item objectForKey:@"children"] count];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return [[item objectForKey:@"children"] count] > 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (item == nil)
        return [arrangedSearchResults objectAtIndex:index];
    return [[item objectForKey:@"children"] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return [item objectForKey:[tableColumn identifier]];
}

- (void)outlineView:(NSOutlineView *)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn *)tableColumn
               item:(id)item
{
    if ([[tableColumn identifier] isEqualToString:@"attributedFilename"]) {
        share_type_t type = [[item objectForKey:@"type"] intValue];
        NSImage *img = [[FiletypeImageTransformer defaultFiletypeImageTransformer]
            transformedValue:[NSNumber numberWithInt:type]];
        [cell setImage:img];
        [cell setLeaf:YES];
    }
}

- (void)sortArray:(NSMutableArray *)anArray usingDescriptors:(NSArray *)sortDescriptors
{
    [anArray sortUsingDescriptors:sortDescriptors];

    NSEnumerator *e = [anArray objectEnumerator];
    NSDictionary *item;
    while ((item = [e nextObject]) != nil) {
        NSMutableArray *children = [item objectForKey:@"children"];
        if (children) {
            [self sortArray:children usingDescriptors:sortDescriptors];
        }
    }
}

- (void)outlineView:(NSOutlineView *)outlineView sortDescriptorsDidChange:(NSArray *)oldDescriptors
{
    [self sortArray:arrangedSearchResults usingDescriptors:[outlineView sortDescriptors]];
    [outlineView reloadData];
}

#pragma mark -
#pragma mark Menu validation

- (void)menuNeedsUpdate:(NSMenu *)sender
{
    // update all user commands, before showing the contextmenu.
    
    /* erase all previous user commands */
    int indexOfTTHItem = [contextMenu indexOfItem:menuItemCopyTTH];
    int j;
    for (j = [contextMenu numberOfItems]-1; j > indexOfTTHItem; j--)
        [contextMenu removeItemAtIndex:j];

    /* get a unique set of selected hubs */
    NSIndexSet *selectedIndices = [searchResultTable selectedRowIndexes];
    NSMutableSet *uniqueHubs = [[NSMutableSet alloc] init];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        NSDictionary *sr = [searchResultTable itemAtRow:i];
        [uniqueHubs addObject:[sr objectForKey:@"hub"]];
        i = [selectedIndices indexGreaterThanIndex:i];
    }

    if ([uniqueHubs count]) {
        /* add a separator before user commands submenus */
        [contextMenu addItem:[NSMenuItem separatorItem]];

        NSEnumerator *e = [uniqueHubs objectEnumerator];
        NSString *hub;
        while ((hub = [e nextObject]) != nil) {
            NSArray *ucArray = [[SPMainWindowController sharedMainWindowController] userCommandsForHub:hub];
            if ([ucArray count]) {
                /* add a submenu for this hubs usercommands */
                /* [contextMenu addItemWithTitle:hub action:nil keyEquivalent:@""]; */
                NSMenu *subMenu = [[[NSMenu alloc] initWithTitle:hub] autorelease];
                NSMenuItem *menuItem = [contextMenu addItemWithTitle:hub
                                                              action:nil
                                                       keyEquivalent:@""];
                [menuItem setSubmenu:subMenu];

                NSEnumerator *e2 = [ucArray objectEnumerator];
                SPUserCommand *uc;
                while ((uc = [e2 nextObject]) != nil) {
                    if (([uc context] & 4) == 4)
                    {
                        [subMenu addUserCommand:uc
                                         action:@selector(executeSearchUserCommand:)
                                         target:self
                                  staticEntries:0];
                    }
                }
            }
        }
    }
    
    [uniqueHubs release];
}

@end
