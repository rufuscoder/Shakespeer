#import "SPTableView.h"

@implementation SPTableView : NSTableView

/*
 * Code by Timothy Hatcher, from http://www.cocoadev.com/index.pl?RightClickSelectInTableView
 * Modified by Martin Hedenfalk <martin@bzero.se>
 */
- (NSMenu *)menuForEvent:(NSEvent *)event
{
    NSPoint where = [self convertPoint:[event locationInWindow] fromView:nil];
    int row = [self rowAtPoint:where];
    int col = [self columnAtPoint:where];

    if (row >= 0) {
        NSTableColumn *column = nil;
        if (col >= 0)
            column = [[self tableColumns] objectAtIndex:col];

        if ([self numberOfSelectedRows] <= 1) {
            if ([[self delegate] respondsToSelector:@selector(tableView:shouldSelectRow:)]) {
                if ([[self delegate] tableView:self shouldSelectRow:row])
                    [self selectRow:row byExtendingSelection:NO];
            }
            else {
                [self selectRow:row byExtendingSelection:NO];
            }
        }

        if ([[self dataSource] respondsToSelector:@selector(tableView:menuForTableColumn:row:)])
            return [[self dataSource] tableView:self menuForTableColumn:column row:row];
        else
            return [self menu];
    }

    [self deselectAll:nil];
    return [self menu];
}

/* method to detect an enter key press to connect to the selected hub,
the code is from http://www.cocoadev.com/index.pl?DetectDeleteKeyPressInNSOutlineView */
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
    NSString *chars = [theEvent charactersIgnoringModifiers];
    
    if ([theEvent type] == NSKeyDown && [chars length] == 1) {
        int val = [chars characterAtIndex:0];
        
        // check for the enter key
        if (val == 13) {
            if ([[self delegate] respondsToSelector:@selector(tableDidRecieveEnterKey:)]) {
                [[self delegate] performSelector:@selector(tableDidRecieveEnterKey:) withObject:self];
                return YES;
            }
        }
    }
    
    return [super performKeyEquivalent:theEvent];
}

@end

