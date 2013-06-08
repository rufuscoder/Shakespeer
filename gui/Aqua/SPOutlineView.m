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

#import "SPOutlineView.h"

@implementation SPOutlineView 

- (void)awakeFromNib
{
    keypressBuffer = [[NSMutableString alloc] initWithString:@""];
}

- (void)dealloc
{
    [keypressBuffer release];
    [bufferTimer invalidate];
    [bufferTimer release];
    [typeSearchTableColumn release];
    
    [super dealloc];
}

/* Select the row under the mouse cursor on right-click (ctrl-click). */
- (NSMenu *)menuForEvent:(NSEvent *)event
{
    NSPoint where = [self convertPoint:[event locationInWindow] fromView:nil];
    int row = [self rowAtPoint:where];

    if (row >= 0) {
        if ([self numberOfSelectedRows] <= 1)
            [self selectRow:row byExtendingSelection:NO];

        return [self menu];
    }

    [self deselectAll:nil];
    
    return [self menu];
}

#pragma mark -
#pragma mark Type searching

- (void)setTypeSearchTableColumn:(NSTableColumn *)tableColumn
{
    if (tableColumn != typeSearchTableColumn) {
        [typeSearchTableColumn release];
        typeSearchTableColumn = [tableColumn retain];
    }
}

// handles keypresses
- (void)keyDown:(NSEvent *)theEvent
{
    // we only want non-shortcuts (pure characters), so skip events with command key
    if (!([theEvent modifierFlags] & NSCommandKeyMask) && typeSearchTableColumn) {
        // the user pressed a key, so add it to the buffer
        [keypressBuffer appendString:[theEvent characters]];
        
        // start the search with the root item(s)
        int numberOfRootItems = [[self dataSource] outlineView:self numberOfChildrenOfItem:nil];
        NSMutableArray *rootItems = [NSMutableArray array];
        int i;
        for (i = 0; i < numberOfRootItems; i++)
            [rootItems addObject:[[self dataSource] outlineView:self child:i ofItem:nil]];
        
        // do a recursive search for the given string
        if (![self selectItemInArray:rootItems byString:keypressBuffer]) {
            // no item found, so pass event to the default handler
            [super keyDown:theEvent];
        }
        
        // clear the buffer unless the user presses another key within 0.5 seconds
        [bufferTimer invalidate];
        [bufferTimer autorelease];
        bufferTimer = [[NSTimer scheduledTimerWithTimeInterval:0.5
                                                        target:self
                                                      selector:@selector(clearKeypressBuffer)
                                                      userInfo:nil
                                                       repeats:NO] retain];
    }
}

// clears the keypress buffer
- (void)clearKeypressBuffer
{
    [keypressBuffer setString:@""];
}

// selects the first matching row in the outlineview, based on the supplied string
- (BOOL)selectItemInArray:(NSArray *)rootArray byString:(NSString *)searchString
{
    BOOL eventHandled = NO;
    NSEnumerator *allRows = [rootArray objectEnumerator];
    NSDictionary *currentItem;
    while ((currentItem = [allRows nextObject])) {
        NSString *theString = nil;
        if ([[currentItem valueForKey:[typeSearchTableColumn identifier]] isKindOfClass:[NSAttributedString class]])
            theString = [[currentItem valueForKey:[typeSearchTableColumn identifier]] string];
        else if ([[currentItem valueForKey:[typeSearchTableColumn identifier]] isKindOfClass:[NSString class]])
            theString = [currentItem valueForKey:[typeSearchTableColumn identifier]];
        else
            continue;
        
        // use lowercaseString to make matches case insensitive
        if ([[theString lowercaseString] hasPrefix:[searchString lowercaseString]]) {
            /* scroll to and select the appropriate row, then end the search
               to imitate the behaviour in Finder */
            [self scrollRowToVisible:[self rowForItem:currentItem]];
            [self selectRow:[self rowForItem:currentItem] byExtendingSelection:NO];
            eventHandled = YES;
            break;
        }
        if ([self isItemExpanded:currentItem]) {
            if ([self selectItemInArray:[currentItem objectForKey:@"children"] byString:searchString]) {
                eventHandled = YES;
                break;
            }
        }
            
    }
    
    return eventHandled;
}

@end
