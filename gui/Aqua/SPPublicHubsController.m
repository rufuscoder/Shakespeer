/* vim: ft=objc
 *
 * Mac DC++. An Aqua user interface for DC++.
 * Copyright (C) 2004 Jonathan Jansson, jonathan.dator@home.se
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import "SPPublicHubsController.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"
#import "NSStringExtensions.h"
#import "SPSideBar.h"
#import "FilteringArrayController.h"
#import "SPUserDefaultKeys.h"
#import "SPBookmarkController.h"
#import "SPLog.h"

#include "util.h"
#include "hublist.h"

// data type for drags of public hubs
NSString *SPPublicHubDataType = @"SPPublicHubDataType";

@implementation SPPublicHubsController

- (id)init
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"PublicHubs" owner:self];
        [hubTable setTarget:self];
        [hubTable setDoubleAction:@selector(tableDoubleActionConnect:)];
        
        [arrayController setSearchKeys:
             [NSArray arrayWithObjects:@"description", @"name",
            @"address", @"location", nil]];
        hubs = [[NSMutableArray alloc] init];

        char *hublist_filename = hl_get_current();

        if (hublist_filename) {
            xerr_t *err = 0;
            hublist_t *hublist = hl_parse_file(hublist_filename, &err);
            if (err) {
                [[SPMainWindowController sharedMainWindowController]
                 setStatusMessage:[NSString stringWithFormat:@"Failed to load hublist: %s", xerr_msg(err)]];
                xerr_free(err);
            }
            [self setHubsFromList:hublist];
            hl_free(hublist);
            free(hublist_filename);
        }
        else {
            [self refresh:self];
        }
    }
    
    return self;
}

- (void)awakeFromNib
{
    NSArray *availableColumns = [hubTable tableColumns];
    NSEnumerator *e = [availableColumns objectEnumerator];
    NSTableColumn *currentColumn;
    
    allTableColumns = [[NSMutableArray array] retain];
    columnsMenu = [[NSMenu alloc] init];
    
    while ((currentColumn = [e nextObject])) {
        [[currentColumn dataCell] setWraps:YES];
        
        // hold on to column so it isn't released if hidden
        [allTableColumns addObject:currentColumn];
        
        // setup a menuitem for it
        NSString *columnName = [[currentColumn headerCell] stringValue];
        NSMenuItem *newColumn = [[[NSMenuItem alloc] initWithTitle:columnName action:@selector(toggleColumn:) keyEquivalent:@""] autorelease];
        [newColumn setTarget:self];
        [newColumn setRepresentedObject:currentColumn];
        [newColumn setState:NSOnState];
        
        // add it to the context menu
        [columnsMenu addItem:newColumn];
    }
    
    [[hubTable headerView] setMenu:columnsMenu];
    [hubTable setMenu:contextMenu];
    
    // register our private drag type
    [hubTable registerForDraggedTypes:[NSArray arrayWithObject:SPPublicHubDataType]];
}

// called when a drag begins inside the table
- (BOOL)tableView:(NSTableView *)tv writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard*)pboard 
{
    // Archive and copy the dragged rows to the pasteboard.
    NSArray *draggedItems = [[arrayController arrangedObjects] objectsAtIndexes:rowIndexes];
    
    [pboard declareTypes:[NSArray arrayWithObject:SPPublicHubDataType] owner:nil];
    BOOL success = [pboard setData:[NSKeyedArchiver archivedDataWithRootObject:draggedItems] forType:SPPublicHubDataType];

    return success;
}

- (void)unbindControllers
{
}

- (void)dealloc
{
    // Free top level nib-file objects
    [hubListView release];
    [arrayController release];
    [allTableColumns release];
    [columnsMenu release];

    // Free other objects
    [hubs release];
    [super dealloc];
}

- (id)view
{
    return hubListView;
}

- (NSString *)title
{
    return @"Public Hubs";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"public_hubs"];
}

- (NSMenu *)menu
{
    return nil;
}

- (IBAction)tableDoubleActionConnect:(id)sender
{
    NSString *address = [[[arrayController selection] valueForKey:@"address"] string];
    if (address) {
        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:nil
                                                                      description:nil
                                                                         password:nil
                                                                         encoding:nil];
    }
}

- (IBAction)connect:(id)sender
{
    /* connect to all selected items in the table */
    NSString *address;
    NSArray *items = [arrayController selectedObjects];
    NSEnumerator *e = [items objectEnumerator];
    NSDictionary *dict;
    while ((dict = [e nextObject]) != nil) {
        address = [[dict valueForKey:@"address"] string];
        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:nil
                                                                      description:nil
                                                                         password:nil
                                                                         encoding:nil];
    }
}

- (void)setHubsFromList:(hublist_t *)hublist
{
    NSMutableArray *hubArray = [[NSMutableArray alloc] init];
    if (hublist) {
        hublist_hub_t *h;
        LIST_FOREACH(h, hublist, link) {
            NSMutableDictionary *hub = [NSMutableDictionary dictionary];

            NSString *str = h->name ? [NSString stringWithUTF8String:h->name] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"name"];

            str = h->address ? [NSString stringWithUTF8String:h->address] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"address"];

            str = h->country ? [NSString stringWithUTF8String:h->country] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"location"];

            str = h->description ? [NSString stringWithUTF8String:h->description] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"description"];

            [hub setObject:[NSNumber numberWithInt:h->max_users] forKey:@"users"];
            [hub setObject:[NSNumber numberWithUnsignedLongLong:h->min_share] forKey:@"minShare"];
            [hub setObject:[NSNumber numberWithInt:h->min_slots] forKey:@"minSlots"];
            [hub setObject:[NSNumber numberWithInt:h->max_hubs] forKey:@"maxHubs"];

            [hubArray addObject:hub];
        }
    }

    [self willChangeValueForKey:@"hubs"];
    [hubs setArray:hubArray];
    [self didChangeValueForKey:@"hubs"];
    [hubArray release];
}

- (void)setHubsFromListWrapped:(NSValue *)wrappedPointer
{
    [self setHubsFromList:[wrappedPointer pointerValue]];
}

- (void)setRefreshInProgress:(BOOL)flag
{
    refreshInProgress = flag;
}

- (void)mainThreadStatusMessage:(NSString *)msg
{
    [[SPMainWindowController sharedMainWindowController] setStatusMessage:msg];
}

- (void)statusMessage:(NSString *)msg
{
    [self performSelectorOnMainThread:@selector(mainThreadStatusMessage:)
			   withObject:msg
			waitUntilDone:YES];
}

- (void)refreshThread:(id)args
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [self setRefreshInProgress:YES];

    char *working_directory = get_working_directory();

    NSString *hublistAddress = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsHublistURL];
    NSURL *hublistURL = [NSURL URLWithString:hublistAddress];

    if(![[hublistURL scheme] isEqualToString:@"http"]) {
	[self statusMessage:@"Failed to load hublist: not an http resource"];
	goto done;
    }

    [self statusMessage:[NSString stringWithFormat:@"Loading hublist from %@", hublistAddress]];

    SPLog(@"sending request for [%@]", hublistAddress);
    NSURLRequest *request = [NSURLRequest requestWithURL:hublistURL];
    NSURLResponse *response = nil;
    NSError *error = nil;
    NSData *data = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&error];
    NSString *hublistPath = nil;

    if(error) {
	SPLog(@"got error: %@", [error localizedDescription]);
	[self statusMessage:[NSString stringWithFormat:@"Failed to load hublist: %@", [error localizedDescription]]];
	goto done;
    } else if([response expectedContentLength] == 0) {
	SPLog(@"zero size response");
	[self statusMessage:@"Failed to load hublist: zero sized file"];
	goto done;
    }

    int code = [(NSHTTPURLResponse *)response statusCode];
    SPLog(@"http response code %i", code);
    if(code != 200) {
	[self statusMessage:[NSString stringWithFormat:@"Failed to load hublist: %@",
	    [NSHTTPURLResponse localizedStringForStatusCode:code]]];
	goto done;
    }

    if(data) {
	NSString *prefix = [NSString stringWithFormat:@"%s/PublicHublist", working_directory];

	// remove old files
	[[NSFileManager defaultManager] removeFileAtPath:[NSString stringWithFormat:@"%@.xml", prefix] handler:nil];
	[[NSFileManager defaultManager] removeFileAtPath:[NSString stringWithFormat:@"%@.xml.bz2", prefix] handler:nil];
	[[NSFileManager defaultManager] removeFileAtPath:[NSString stringWithFormat:@"%@.config", prefix] handler:nil];
	[[NSFileManager defaultManager] removeFileAtPath:[NSString stringWithFormat:@"%@.config.bz2", prefix] handler:nil];

	if([[hublistURL path] hasSuffix:@".xml.bz2"])
	    hublistPath = [NSString stringWithFormat:@"%@.xml.bz2", prefix];
	else
	    hublistPath = [NSString stringWithFormat:@"%@.config.bz2", prefix];

	SPLog(@"saving hublist as %@", hublistPath);

	[[NSFileManager defaultManager] createFileAtPath:hublistPath contents:data attributes:nil];
    }

    hublist_t *hublist = NULL;
    xerr_t *err = 0;
    if(hublistPath)
	hublist = hl_parse_file([hublistPath UTF8String], &err);

    if (err) {
	[self statusMessage:[NSString stringWithFormat:@"Failed to load hublist: %s", xerr_msg(err)]];
	xerr_free(err);
	goto done;
    }

    NSValue *wrappedList = [NSValue valueWithPointer:hublist];
    [self performSelectorOnMainThread:@selector(setHubsFromListWrapped:)
			   withObject:wrappedList
			waitUntilDone:YES];
    hl_free(hublist);

    [self statusMessage:@"Hublist updated"];

done:
    free(working_directory);
    [self setRefreshInProgress:NO];

    [pool release];
}

- (IBAction)refresh:(id)sender
{
    if (refreshInProgress == NO)
        [NSThread detachNewThreadSelector:@selector(refreshThread:) toTarget:self withObject:nil];
    else
        [[SPMainWindowController sharedMainWindowController] statusMessage:@"Refreshing of the hublist already in progress"];
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = [sender representedObject];
    if (!tc)
        return;
    
    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [hubTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [hubTable removeTableColumn:tc];
    }
}

- (void)tableDidRecieveEnterKey:(id)sender
{
    [self tableDoubleActionConnect:sender];
}

- (void)addHubsToBookmarks:(id)sender
{
    // add all selected hubs to bookmarks
    NSArray *items = [arrayController selectedObjects];
    NSEnumerator *e = [items objectEnumerator];
    NSDictionary *dict;
    while ((dict = [e nextObject]) != nil) {
        [[SPBookmarkController sharedBookmarkController] addBookmarkWithName:[[dict valueForKey:@"name"] string]
                                                                     address:[[dict valueForKey:@"address"] string]];
    }
}

@end

