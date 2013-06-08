/*
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

#import "SPFilelistController.h"
#import "NSStringExtensions.h"
#import "SPApplicationController.h"
#import "SPTransformers.h"
#import "SPSideBar.h"
#import "SPOutlineView.h"
#import "SPMainWindowController.h"

#import "SPUserDefaultKeys.h"

@implementation SPFilelistController

- (id)initWithFile:(NSString *)aPath nick:(NSString *)aNick hub:(NSString *)aHubAddress
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"DirectoryListing" owner:self];
        nick = [aNick copy];
        hubAddress = [aHubAddress copy];

        [[filelist outlineTableColumn] setDataCell:[[[NSBrowserCell alloc] init] autorelease]];

        /* set initial sort descriptors */
        NSSortDescriptor *sd1 = [[[NSSortDescriptor alloc] initWithKey:@"isDirectory"
                                                             ascending:NO] autorelease];
        NSSortDescriptor *sd2 = [[[NSSortDescriptor alloc] initWithKey:@"Filename"
                                                             ascending:YES
                                                              selector:@selector(caseInsensitiveCompare:)] autorelease];
        [filelist setSortDescriptors:[NSArray arrayWithObjects:sd1, sd2, nil]];

	xerr_t *err = NULL;
        root = fl_parse([aPath UTF8String], &err);
        if (root == NULL) {
	    [[SPMainWindowController sharedMainWindowController]
         setStatusMessage:[NSString stringWithFormat:@"Failed to load %@'s filelist: %s", aNick, xerr_msg(err)]];
	    xerr_free(err);
        }
        else {
            rootItems = [[self setFiles:root] retain];
            arrangedRootItems = [rootItems retain];
            
            fl_free_dir(root);
            [filelist setTarget:self];
            [filelist setDoubleAction:@selector(onDoubleClick:)];
            [self sortArray:rootItems usingDescriptors:[filelist sortDescriptors]];
            [filelist reloadData];
        }
    }
    
    return self;
}

- (void)awakeFromNib
{
    // enable type searching based on the filename column
    [filelist setTypeSearchTableColumn:tcFilename];
    
    [tcSize retain];
    [tcTTH retain];
    
    NSArray *tcs = [filelist tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];
        [[tc dataCell] setFont:[NSFont systemFontOfSize:[[NSUserDefaults standardUserDefaults] floatForKey:@"fontSize"]]];
        
        if (tc == tcSize)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcTTH)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
    }
    
    [[filelist headerView] setMenu:columnsMenu];
}

- (NSMutableArray *)setFiles:(fl_dir_t *)dir
{
    NSMutableArray *items = [NSMutableArray arrayWithCapacity:dir->nfiles];

    char *e = dir->path;
    for (; e && *e; e++) {
        if (*e == '/')
            *e = '\\';
    }

    NSString *path = [[NSString alloc] initWithUTF8String:dir->path];

    fl_file_t *file;
    TAILQ_FOREACH(file, &dir->files, link) {
        NSMutableDictionary *item = [NSMutableDictionary dictionary];

        [item setObject:path forKey:@"Path"];
        NSString *filename = [NSString stringWithUTF8String:file->name];
        [item setObject:filename forKey:@"Filename"];
        [item setObject:[[filename truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"DisplayFilename"];
        [item setObject:[[path truncatedString:NSLineBreakByTruncatingHead] autorelease] forKey:@"DisplayPath"];

        NSString *fullPath = [[NSString alloc] initWithFormat:@"%@\\%@", path, filename];
        [item setObject:[[fullPath truncatedString:NSLineBreakByTruncatingHead] autorelease] forKey:@"DisplayFullPath"];
        [fullPath release];

        [item setObject:[NSNumber numberWithInt:file->type] forKey:@"type"];
        if (file->tth) {
            NSString *tth = [NSString stringWithUTF8String:file->tth];
            [item setObject:tth forKey:@"TTH"];
            [item setObject:[[tth truncatedString:NSLineBreakByTruncatingMiddle] autorelease] forKey:@"DisplayTTH"];
        }
        else {
            [item setObject:@"" forKey:@"TTH"];
            [item setObject:@"" forKey:@"DisplayTTH"];
        }
        [items addObject:item];

        uint64_t fsize;
        if (file->type == SHARE_TYPE_DIRECTORY) {
            NSMutableArray *children = [self setFiles:file->dir];
            [item setObject:children forKey:@"children"];
            [item setObject:[NSNumber numberWithBool:YES] forKey:@"isDirectory"];
            fsize = file->dir->size;
        }
        else {
            [item setObject:[NSNumber numberWithBool:NO] forKey:@"isDirectory"];
            fsize = file->size;
        }

        NSString *str = [[NSString alloc] initWithUTF8String:str_size_human(fsize)];
        [item setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"Size"];
        [str release];
        [item setObject:[NSNumber numberWithUnsignedLongLong:fsize] forKey:@"Exact Size"];
    }

    [path release];

    return items;
}

- (void)dealloc
{
    [nick release];
    [hubAddress release];

    [arrangedRootItems release];
    [rootItems release];
    
    [tcSize release];
    [tcTTH release];

    [super dealloc];
}

#pragma mark -
#pragma mark SPSideBar support

- (void)unbindControllers
{
}

- (NSView *)view
{
    return filelistView;
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
    return nick;
}

- (NSMenu *)menu
{
    return nil;
}

- (NSString *)sectionTitle
{
    return @"Filelists";
}

#pragma mark -
#pragma mark NSOutlineView delegate methods

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (item == nil)
        return [arrangedRootItems count];
    if ([item objectForKey:@"children"])
        return [[item objectForKey:@"children"] count];
    return 0;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return [item objectForKey:@"children"] != nil;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (item == nil)
        return [arrangedRootItems objectAtIndex:index];
    return [[item objectForKey:@"children"] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if (flatStructure && [[tableColumn identifier] isEqualToString:@"DisplayFilename"]) {
        return [item objectForKey:@"DisplayFullPath"];
    }
    else {
        return [item objectForKey:[tableColumn identifier]];
    }
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if ([[tableColumn identifier] isEqualToString:@"DisplayFilename"]) {
        share_type_t type = [[item objectForKey:@"type"] intValue];
        NSImage *img = [[FiletypeImageTransformer defaultFiletypeImageTransformer]
            transformedValue:[NSNumber numberWithInt:type]];
        [cell setImage:img];
        [cell setLeaf:YES];
        [cell setFont:[NSFont systemFontOfSize:[[NSUserDefaults standardUserDefaults] floatForKey:@"fontSize"]]];
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
    if ([[[[outlineView sortDescriptors] objectAtIndex:0] key] isEqualToString:@"DisplayFilename"]) {
        /* when sorting by filename, prepend a sort descriptor that sorts by
         * isDirectory (gives directories above files) */
        NSSortDescriptor *sd1 = [[[NSSortDescriptor alloc] initWithKey:@"isDirectory"
                                                             ascending:NO] autorelease];
        NSArray *newSortDescriptors = [[NSArray arrayWithObjects:sd1, nil]
                                            arrayByAddingObjectsFromArray:[outlineView sortDescriptors]];
        [self sortArray:arrangedRootItems usingDescriptors:newSortDescriptors];
    }
    else {
        [self sortArray:arrangedRootItems usingDescriptors:[outlineView sortDescriptors]];
    }
    [outlineView reloadData];
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcSize; break;
        case 1: tc = tcTTH; break;
    }
    if (tc == nil)
        return;
    
    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [filelist addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [filelist removeTableColumn:tc];
    }
}

- (void)downloadItem:(NSDictionary *)item
{
    NSString *filename = [item objectForKey:@"Filename"];
    NSString *sourcePath = nil;
    if ([(NSString *)[item objectForKey:@"Path"] length]) {
        sourcePath = [NSString stringWithFormat:@"%@\\%@", [item objectForKey:@"Path"], filename];
    }
    else {
        sourcePath = filename;
    }
    NSString *targetPath = filename;

    if ([[item objectForKey:@"type"] intValue] == SHARE_TYPE_DIRECTORY) {
        [[SPApplicationController sharedApplicationController] downloadDirectory:sourcePath
                                                                        fromNick:nick
                                                                           onHub:hubAddress
                                                                toLocalDirectory:targetPath];
    }
    else {
        [[SPApplicationController sharedApplicationController] downloadFile:sourcePath
                                                                   withSize:[item objectForKey:@"Exact Size"]
                                                                   fromNick:nick
                                                                      onHub:hubAddress
                                                                toLocalFile:targetPath
                                                                        TTH:[item objectForKey:@"TTH"]];
    }
}

- (IBAction)downloadSelectedItems:(id)sender
{
    NSIndexSet *selectedIndexes = [filelist selectedRowIndexes];
    unsigned int i = [selectedIndexes firstIndex];
    while (i != NSNotFound) {
        NSDictionary *item = [filelist itemAtRow:i];
        [self downloadItem:item];
        i = [selectedIndexes indexGreaterThanIndex:i];
    }
}

- (IBAction)copyMagnet:(id)sender
{
    NSIndexSet *selectedIndexes = [filelist selectedRowIndexes];
    unsigned int i = [selectedIndexes firstIndex];
    if (i != NSNotFound) {
        NSDictionary *item = [filelist itemAtRow:i];
        NSString *hash = [item objectForKey:@"TTH"];
        NSNumber *size = [item objectForKey:@"Exact Size"];
        NSString *name = (NSString *)CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef)[item objectForKey:@"Filename"], NULL, NULL, kCFStringEncodingUTF8);
        NSString *contents = [NSString stringWithFormat:@"magnet:?xt=urn:tree:tiger:%@&xl=%@&dn=%@", hash, size, name];
        [name release];
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
        [pasteboard setString:contents forType:NSStringPboardType];
        
        [[SPMainWindowController sharedMainWindowController]
         setStatusMessage:[NSString stringWithFormat:@"Copied magnet link to clipboard"]];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if (item == magnetMenu)
    {
        NSIndexSet *selectedIndexes = [filelist selectedRowIndexes];
        unsigned int i = [selectedIndexes firstIndex];
        if (i != NSNotFound) {
            NSDictionary *row = [filelist itemAtRow:i];
            NSString *hash = [row objectForKey:@"TTH"];
            return ([hash length] > 0);
        }
        return NO;
    }
    return YES;
}

- (void)onDoubleClick:(id)sender
{
    NSDictionary *item = [filelist itemAtRow:[filelist selectedRow]];

    if (item) {
        if ([[item objectForKey:@"type"] intValue] == SHARE_TYPE_DIRECTORY) {
            if ([filelist isItemExpanded:item])
                [filelist collapseItem:item];
            else
                [filelist expandItem:item];
        }
        else {
            [self downloadItem:item];
        }
    }
}

- (NSMutableArray *)recursivelyFilterArray:(NSArray *)anArray
                                  onString:(NSString *)filterString
                             flatStructure:(BOOL)flatFlag
{
    NSMutableArray *items = [NSMutableArray arrayWithCapacity:[anArray count]];
    NSEnumerator *e = [anArray objectEnumerator];
    NSDictionary *item;
    while ((item = [e nextObject]) != nil) {
        NSString *filename = [item objectForKey:@"Filename"];
        NSMutableDictionary *arrangedItem = nil;
        NSMutableArray *children = [item objectForKey:@"children"];
        if (children) {
            // it's a folder. check for matches inside it
            NSArray *arrangedChildren = [self recursivelyFilterArray:children
                                                            onString:filterString
                                                       flatStructure:flatFlag];
            if ([arrangedChildren count] > 0) {
                if (flatFlag) {
                    [items addObjectsFromArray:arrangedChildren];
                }
                else {
                    arrangedItem = [NSMutableDictionary dictionary];
                    [arrangedItem setObject:arrangedChildren forKey:@"children"];
                }
            }
        }
        if (!arrangedItem && ([filterString length] == 0 || 
            [filename rangeOfString:filterString options:NSCaseInsensitiveSearch].location != NSNotFound || 
            (flatFlag && [[item objectForKey:@"Path"] rangeOfString:filterString options:NSCaseInsensitiveSearch].location != NSNotFound)))
        {
            arrangedItem = [NSMutableDictionary dictionary];
        }

        if (arrangedItem) {
            /* NSLog(@"adding item [%@]", [item objectForKey:@"Path"]); */
            [arrangedItem setObject:[item objectForKey:@"Filename"] forKey:@"Filename"];
            [arrangedItem setObject:[item objectForKey:@"DisplayFilename"] forKey:@"DisplayFilename"];
            [arrangedItem setObject:[item objectForKey:@"DisplayPath"] forKey:@"DisplayPath"];
            [arrangedItem setObject:[item objectForKey:@"DisplayFullPath"] forKey:@"DisplayFullPath"];
            [arrangedItem setObject:[item objectForKey:@"Path"] forKey:@"Path"];
            [arrangedItem setObject:[item objectForKey:@"type"] forKey:@"type"];
            [arrangedItem setObject:[item objectForKey:@"TTH"] forKey:@"TTH"];
            [arrangedItem setObject:[item objectForKey:@"DisplayTTH"] forKey:@"DisplayTTH"];
            [arrangedItem setObject:[item objectForKey:@"isDirectory"] forKey:@"isDirectory"];
            [arrangedItem setObject:[item objectForKey:@"Size"] forKey:@"Size"];
            [arrangedItem setObject:[item objectForKey:@"Exact Size"] forKey:@"Exact Size"];
            [items addObject:arrangedItem];
        }
    }
    /* NSLog(@"returning %i items", [items count]); */
    return items;
}

- (IBAction)filter:(id)sender
{
    NSString *filterString = [filterField stringValue];
    flatStructure = ([flatButton state] == NSOnState);

    [arrangedRootItems release];
    arrangedRootItems = [self recursivelyFilterArray:rootItems onString:filterString flatStructure:flatStructure];
    [arrangedRootItems retain];
    [filelist reloadData];
}

- (IBAction)refresh:(id)sender
{
    [[SPApplicationController sharedApplicationController] downloadFilelistFromUser:nick
                                                                              onHub:hubAddress
                                                                        forceUpdate:YES
                                                                          autoMatch:NO];
}

@end

