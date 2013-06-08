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

#import "SPTransferController.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"
#import "SPHubController.h"
#import "NSStringExtensions.h"
#import "SPLog.h"
#import "SPSideBar.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

#include "util.h"

#define DIR_DOWNLOAD (1)
#define DIR_UPLOAD (2)

@implementation SPTransferController

- (id)init
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"Uploads" owner:self];
        transfers = [[NSMutableArray alloc] init];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferStatsNotification:)
                                                     name:SPNotificationTransferStats
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(downloadStartingNotification:)
                                                     name:SPNotificationDownloadStarting
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(uploadStartingNotification:)
                                                     name:SPNotificationUploadStarting
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferAbortedNotification:)
                                                     name:SPNotificationTransferAborted
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferFinishedNotification:)
                                                     name:SPNotificationDownloadFinished
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferFinishedNotification:)
                                                     name:SPNotificationUploadFinished
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(filelistFinishedNotification:)
                                                     name:SPNotificationFilelistFinished
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(connectionClosedNotification:)
                                                     name:SPNotificationConnectionClosed
                                                   object:nil];
    }
    
    return self;
}

- (void)awakeFromNib
{
    [tcUser retain];
    [tcStatus retain];
    [tcTimeLeft retain];
    [tcSpeed retain];
    [tcFilename retain];
    [tcSize retain];
    [tcPath retain];
    [tcHub retain];
    
    NSArray *tcs = [transferTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];
        
        if (tc == tcUser)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcStatus)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcTimeLeft)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcSpeed)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcFilename)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcSize)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcPath)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
        else if (tc == tcHub)
            [[columnsMenu itemWithTag:7] setState:NSOnState];
    }
    
    [[transferTable headerView] setMenu:columnsMenu];
    
    // Don't show downloads in table
    [arrayController setFilterPredicate:
        [NSPredicate predicateWithFormat:@"direction == 2"]];
    [arrayController rearrangeObjects];
}

- (NSView *)view
{
    return transferView;
}

- (NSString *)title
{
    return @"Uploads";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"transfers"];
}

- (NSMenu *)menu
{
    return transferMenu;
}

- (void)dealloc
{
    [transfers release];
    
    // Free table columns
    [tcUser release];
    [tcStatus release];
    [tcTimeLeft release];
    [tcSpeed release];
    [tcFilename release];
    [tcSize release];
    [tcPath release];
    [tcHub release];
    
    [super dealloc];
}

#pragma mark -
#pragma mark Utility functions

- (void)addTransfer:(NSDictionary *)dict direction:(int)aDirection
{
    NSString *filename = [dict objectForKey:@"targetFilename"];
    SPTransferItem *tr = [self findTransferItemWithTargetFilename:filename];
    if (tr == nil) {
        tr = [self findTransferItemWithNick:[dict objectForKey:@"nick"] directions:aDirection];
    }

    BOOL add = NO;
    if (tr == nil) {
        tr = [[SPTransferItem alloc] initWithNick:[dict objectForKey:@"nick"]
                                         filename:filename
                                             size:[[dict objectForKey:@"size"] unsignedLongLongValue]
                                       hubAddress:[dict objectForKey:@"hubAddress"]
                                        direction:aDirection];
        add = YES;
    }
    else {
        [tr setFilename:filename];
    }

    [self willChangeValueForKey:@"transfers"];
    [tr setState:aDirection == DIR_DOWNLOAD ? SPTransferState_Downloading : SPTransferState_Uploading];
    if (add) {
        [transfers addObject:tr];
        [tr release];
    }
    [self didChangeValueForKey:@"transfers"];
}

- (SPTransferItem *)findTransferItemWithTargetFilename:(NSString *)aTargetFilename
{
    NSEnumerator *e = [transfers objectEnumerator];
    SPTransferItem *tr;
    while ((tr = [e nextObject]) != nil) {
        if ([aTargetFilename isEqualToString:[tr targetFilename]]) {
            return tr;
        }
    }
    return nil;
}

- (SPTransferItem *)findTransferItemWithNick:(NSString *)aNick directions:(int)aDirectionMask
{
    NSEnumerator *e = [transfers objectEnumerator];
    SPTransferItem *tr;
    while ((tr = [e nextObject]) != nil) {
        if ([aNick isEqualToString:[tr nick]] && ([tr direction] & aDirectionMask) > 0) {
            return tr;
        }
    }
    return nil;
}

#pragma mark -
#pragma mark Sphubd notifications

- (void)transferAbortedNotification:(NSNotification *)aNotification
{
    SPTransferItem *tr = [self findTransferItemWithTargetFilename:
                           [[aNotification userInfo] objectForKey:@"targetFilename"]];
    if (tr) {
        [self willChangeValueForKey:@"transfers"];
        [tr setStatus:@"Aborted"];
        [tr setState:SPTransferState_Error];
        [self didChangeValueForKey:@"transfers"];
    }
}

- (void)transferFinishedNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPTransferItem *tr = [self findTransferItemWithTargetFilename:targetFilename];
    if (tr) {
        [self willChangeValueForKey:@"transfers"];
        [tr setStatus:@"Finished, idle"];
        [tr setState:SPTransferState_Idle];
        [tr setOffset:[tr size]];
        [self didChangeValueForKey:@"transfers"];
    }
}

- (void)filelistFinishedNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPTransferItem *tr = [self findTransferItemWithTargetFilename:targetFilename];
    if (!tr)
        // this is normal. we got a notification when the user loaded a cached filelist, 
        // so no file list was in the transfers.
        return;
    
    [self willChangeValueForKey:@"transfers"];
    [tr setStatus:@"Finished, idle"];
    [tr setState:SPTransferState_Idle];
    [tr setOffset:[tr size]];
    [self didChangeValueForKey:@"transfers"];
}

- (void)uploadStartingNotification:(NSNotification *)aNotification
{
    [self addTransfer:[aNotification userInfo] direction:DIR_UPLOAD];
}

- (void)downloadStartingNotification:(NSNotification *)aNotification
{
    [self addTransfer:[aNotification userInfo] direction:DIR_DOWNLOAD];
}

- (void)transferStatsNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPTransferItem *tr = [self findTransferItemWithTargetFilename:targetFilename];
    if (tr) {
        uint64_t offset = [[[aNotification userInfo] objectForKey:@"offset"]
            unsignedLongLongValue];
        uint64_t size = [[[aNotification userInfo] objectForKey:@"size"]
            unsignedLongLongValue];

        [self willChangeValueForKey:@"transfers"];
        [tr setStatus:[NSNumber numberWithDouble: (double)offset / (size ? size : 1) * 100]];
        [tr setSpeed:[[[aNotification userInfo] objectForKey:@"bps"] intValue]];
        [tr setOffset:offset];
        [tr setSize:size];
        [self didChangeValueForKey:@"transfers"];
    }
}

- (void)connectionClosedNotification:(NSNotification *)aNotification
{
    NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];
    int direction = [[[aNotification userInfo] objectForKey:@"direction"] intValue];

    while (YES) {
        SPTransferItem *tr = [self findTransferItemWithNick:nick directions:direction];
        if (tr) {
            [self willChangeValueForKey:@"transfers"];
            [transfers removeObject:tr];
            [self didChangeValueForKey:@"transfers"];
        }
        else
            break;
    }
}

#pragma mark -
#pragma mark Interface actions

- (void)cancelTransfersInArray:(NSArray *)selectedTransfers
{
    if (selectedTransfers && [selectedTransfers count]) {
        NSString *targetFilename = [[selectedTransfers objectAtIndex:0] valueForKey:@"targetFilename"];

        if (targetFilename) {
            [[SPApplicationController sharedApplicationController] cancelTransfer:targetFilename];
        }
    }
}

- (IBAction)cancelTransfer:(id)sender
{
    [self cancelTransfersInArray:[arrayController selectedObjects]];
}

- (void)removeSourcesInArray:(NSArray *)selectedTransfers
{
    if (selectedTransfers && [selectedTransfers count]) {
        NSString *targetFilename = [[selectedTransfers objectAtIndex:0] valueForKey:@"targetFilename"];
        NSString *remoteNick = [[selectedTransfers objectAtIndex:0] valueForKey:@"nick"];

        if (targetFilename && remoteNick) {
            [[SPApplicationController sharedApplicationController] removeSource:targetFilename
                                                                           nick:remoteNick];
        }
    }
}

- (IBAction)removeSource:(id)sender
{
    [self removeSourcesInArray:[arrayController selectedObjects]];
}

- (void)removeQueuesInArray:(NSArray *)selectedTransfers
{
    if (selectedTransfers && [selectedTransfers count]) {
        NSString *targetFilename = [[selectedTransfers objectAtIndex:0] valueForKey:@"targetFilename"];

        if (targetFilename) {
            [[SPApplicationController sharedApplicationController] removeQueue:targetFilename];
        }
    }
}

- (IBAction)removeQueue:(id)sender
{
    [self removeQueuesInArray:[arrayController selectedObjects]];
}

- (void)removeAllSourcesWithNicksInArray:(NSArray *)selectedTransfers
{
    if (selectedTransfers && [selectedTransfers count]) {
        NSString *remoteNick = [[selectedTransfers objectAtIndex:0] valueForKey:@"nick"];
        if (remoteNick) {
            [[SPApplicationController sharedApplicationController] removeAllSourcesWithNick:remoteNick];
        }
    }
}

- (IBAction)removeAllSourcesWithNick:(id)sender
{
    [self removeAllSourcesWithNicksInArray:[arrayController selectedObjects]];
}

- (void)browseUserInArray:(NSArray *)selectedTransfers
{
    NSDictionary *dict = nil;
    if ([selectedTransfers count])
        dict = [selectedTransfers objectAtIndex:0];

    if (dict) {
        NSString *nick = [dict valueForKey:@"nick"];
        NSString *hubAddress = [dict valueForKey:@"hubAddress"];
        [[SPApplicationController sharedApplicationController] downloadFilelistFromUser:nick
                                                                                  onHub:hubAddress
                                                                            forceUpdate:NO
                                                                              autoMatch:NO];
    }
}

- (IBAction)browseUser:(id)sender
{
    [self browseUserInArray:[arrayController selectedObjects]];
}

- (void)privateMessageInArray:(NSArray *)selectedTransfers
{
    NSDictionary *dict = nil;
    if ([selectedTransfers count])
        dict = [selectedTransfers objectAtIndex:0];

    if (dict) {
        NSString *hubAddress = [dict valueForKey:@"hubAddress"];
        SPHubController *hub = [[SPMainWindowController sharedMainWindowController] hubWithAddress:hubAddress];
        NSString *myNick = nil;
        if (hub) {
            myNick = [hub nick];
        }
        sendNotification(SPNotificationStartChat,
                @"remote_nick", [dict valueForKey:@"nick"],
                @"hubAddress", hubAddress,
                @"my_nick", myNick,
                nil);
    }
}

- (IBAction)privateMessage:(id)sender
{
    [self privateMessageInArray:[arrayController selectedObjects]];
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcUser; break;
        case 1: tc = tcStatus; break;
        case 2: tc = tcTimeLeft; break;
        case 3: tc = tcSpeed; break;
        case 4: tc = tcFilename; break;
        case 5: tc = tcSize; break;
        case 6: tc = tcPath; break;
        case 7: tc = tcHub; break;
    }
    if (tc == nil)
        return;
    
    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [transferTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [transferTable removeTableColumn:tc];
    }
}

@end

#pragma mark -
#pragma mark SPTransferItem implementation

@implementation SPTransferItem

- (id)initWithNick:(NSString *)aNick
          filename:(NSString *)aFilename
              size:(uint64_t)aSize
        hubAddress:(NSString *)aHubAddress
         direction:(int)aDirection
{
    if ((self = [super init])) {
        nick = [aNick retain];
        hubAddress = [aHubAddress retain];
        direction = aDirection;
        [self setFilename:aFilename];
        [self setSize:aSize];
        [self setState:SPTransferState_Idle];
    }
    
    return self;
}

- (id)copy
{
    SPTransferItem *cp = [[SPTransferItem alloc] initWithNick:nick
                                                     filename:targetFilename
                                                         size:exactSize
                                                   hubAddress:hubAddress
                                                    direction:direction];
    [cp setSpeed:speed];
    [cp setState:state];
    return cp;
}

- (void)dealloc
{
    [nick release];
    [filename release];
    [pathname release];
    [targetFilename release];
    [status release];
    [hubAddress release];
    [super dealloc];
}

- (void)setFilename:(NSString *)aFilename
{
    if (aFilename != targetFilename) {
        [filename release];
        filename = [[aFilename lastPathComponent] truncatedString:NSLineBreakByTruncatingHead];

        [pathname release];
        pathname = [[[aFilename stringByDeletingLastPathComponent]
            stringByAbbreviatingWithTildeInPath]
            truncatedString:NSLineBreakByTruncatingHead];

        [targetFilename release];
        targetFilename = [aFilename retain];
    }
}

- (unsigned int)ETA
{
    return (exactSize - offset) / (speed ? speed : 1);
}

- (float)ratio
{
    return 1.0;
}

- (void)setStatus:(NSNumber *)statusNumber
{
    if (status != statusNumber) {
        [status release];
        status = [statusNumber retain];
    }
}

- (void)setSpeed:(unsigned int)aSpeed
{
    speed = aSpeed;
}

- (void)setOffset:(uint64_t)anOffset
{
    offset = anOffset;
}

- (NSString *)targetFilename
{
    return targetFilename;
}

- (NSString *)filename
{
    return [filename string];
}

- (NSString *)nick
{
    return nick;
}

- (void)setSize:(uint64_t)aSize
{
    exactSize = aSize;
}

- (uint64_t)size
{
    return exactSize;
}

- (void)setState:(int)aState
{
    state = aState;
}

- (int)direction
{
    return direction;
}

@end

