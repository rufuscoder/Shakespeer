/* This file is part of ShakesPeer.
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

@interface SPQueueItem : NSObject
{
    NSString *target;
    NSString *filename;
    NSString *displayName;
    NSString *path;
    NSString *tth;
    NSAttributedString *attributedTTH;
    NSNumber *size;
    NSNumber *priority;
    NSAttributedString *priorityString;
    NSNumber *status;
    NSString *statusString;
    
    NSMutableDictionary *sources;
    NSMutableArray *children;
    
    BOOL isFilelist;
    BOOL isDirectory;
    BOOL isFinished;
	
	// when a download is requested to be removed by the user, this is set to make it
	// easy to identify user-removed downloads from finished downloads later when the
	// action is complete (it is aborted) and it needs to be removed from the table.
	BOOL isWaitingToBeRemoved;
}
- (id)initWithTarget:(NSString *)aTarget;
- (id)initWithTarget:(NSString *)aTarget displayName:(NSString *)aDisplayName;

- (NSString *)filename;

// usually the same as filename, unless the name needs to be pretty-printed, like filelists.
- (NSString *)displayName;

- (NSString *)path;
- (NSString *)target;
- (NSString *)tth;
- (void)setTTH:(NSString *)aTTH;
- (NSAttributedString *)attributedTTH;
- (NSNumber *)size;
- (NSNumber *)exactSize;
- (void)setSize:(NSNumber *)aSize;

- (NSNumber *)priority;
- (NSAttributedString *)priorityString;
- (void)setPriority:(NSNumber *)aPriority;

- (NSNumber *)status;
- (void)setStatus:(NSNumber *)statusNumber;
- (NSString *)statusString;
- (void)setStatusString:(NSString *)aStatusString;

- (void)addSource:(NSString *)sourcePath nick:(NSString *)nick;
- (void)removeSourceForNick:(NSString *)nick;
- (NSArray *)nicks;
- (NSMutableArray *)children;
- (NSString *)users;
- (int)filetype;
- (BOOL)isDirectory;
- (void)setIsDirectory;
- (BOOL)isFilelist;
- (void)setIsFilelist:(BOOL)aFlag;
- (BOOL)isFinished;
- (void)setFinished;
- (BOOL)isWaitingToBeRemoved;
- (void)setIsWaitingToBeRemoved:(BOOL)waitingToBeRemoved;

@end
