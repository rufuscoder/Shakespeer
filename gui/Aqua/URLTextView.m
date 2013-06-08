
// URLTextView
// NSTextView subclass to facilitate URL cursor and click tracking
// Tab Size: 3
// Copyright (C) 2002 Aaron Sittig

#import "URLTextView.h"

static NSCursor *sHandCursor = nil;
static NSRectArray NSCopyRectArray(NSRectArray inSomeRects, int inArraySize);
static BOOL NSMouseInRects(NSPoint inPoint, NSRectArray inSomeRects, int inArraySize, BOOL inFlipped);

@implementation URLTextView

- (id)initWithFrame:(NSRect)inFrame
{
    if ((self = [super initWithFrame:inFrame])) {
        [[self window] resetCursorRects];
        [self setFrame:[self frame]];
    }
    
    return self;
}

#pragma mark -

- (void)mouseDown:(NSEvent*)inEvent
{
    // Tracking info
    NSPoint mouseLoc;
    unsigned int glyphIndex;
    unsigned int charIndex;
    unsigned int eventMask;
    NSDate *distantFuture;

    // Link info
    NSRange linkRange;
    NSRectArray linkRects;
    NSURL *linkURL = nil;
    unsigned int linkCount;

    // Tracking state
    BOOL inRects;
    BOOL done;

    // Find clicked char index
    mouseLoc = [self convertPoint:[inEvent locationInWindow] fromView:nil];
    glyphIndex = [[self layoutManager] glyphIndexForPoint:mouseLoc inTextContainer:[self textContainer] fractionOfDistanceThroughGlyph:nil];
    charIndex = [[self layoutManager] characterIndexForGlyphAtIndex:glyphIndex];

    // Check if click is in valid link attributed range, and is inside the bounds of that style range, else fall back to default handler    
    if ([[self textStorage] length] > 0)
        linkURL = [[self textStorage] attribute:NSLinkAttributeName atIndex:charIndex effectiveRange:&linkRange];
    if (!linkURL) {
        [super mouseDown:inEvent];
        return;
    }

    // Setup tracking info and state
    distantFuture = [NSDate distantFuture];
    eventMask = NSLeftMouseUpMask | NSRightMouseUpMask | NSLeftMouseDraggedMask | NSRightMouseDraggedMask;
    done = NO;
    inRects = NO;

    // Find region of clicked link and copy since returned rect array belongs to layout manager
    linkRects = [[self layoutManager] rectArrayForCharacterRange:linkRange
                                    withinSelectedCharacterRange:linkRange
                                                 inTextContainer:[self textContainer]
                                                       rectCount:&linkCount];
    linkRects = NSCopyRectArray(linkRects, linkCount);

    // One last check to make sure we're really in the bounds of the link.
    // Useful when the link runs up to the end of the document and a click in
    // the black area below still passes the style range test above.
    if (!NSMouseInRects(mouseLoc, linkRects, linkCount, NO)) {
        free(linkRects);
        [super mouseDown:inEvent];
        return;
    }

    // Draw outselves as clicked and kick off tracking
    [[self textStorage] addAttribute:NSForegroundColorAttributeName value:[NSColor orangeColor] range:linkRange];
    while (!done) {
        // Get the next event and mouse location
        inEvent = [NSApp nextEventMatchingMask:eventMask untilDate:distantFuture inMode:NSEventTrackingRunLoopMode dequeue:YES];
        mouseLoc = [self convertPoint:[inEvent locationInWindow] fromView:nil];

        switch([inEvent type]) {
            // Case Done Tracking Click
            case NSRightMouseUp:
            case NSLeftMouseUp:
                // If we were still inside the link, draw unclicked and open link
                if (NSMouseInRects(mouseLoc, linkRects, linkCount, NO))
                    [[NSWorkspace sharedWorkspace] openURL:linkURL];

                [[self textStorage] addAttribute:NSForegroundColorAttributeName
                                           value:[NSColor colorWithCalibratedRed:0.1 green:0.3 blue:0.8 alpha:1.0]
                                           range:linkRange];
                done = YES;
                break;

            // Case Mouse Moved
            case NSLeftMouseDragged:
            case NSRightMouseDragged:
                // Check if we moved into the link
                if (NSMouseInRects(mouseLoc, linkRects, linkCount, NO) && inRects == NO) {
                    [[self textStorage] addAttribute:NSForegroundColorAttributeName
                                               value:[NSColor orangeColor]
                                               range:linkRange];
                    inRects = YES;
                }
                else {
                    // Check if we moved out of the link
                    if (!NSMouseInRects(mouseLoc, linkRects, linkCount, NO) && inRects == YES) {
                        [[self textStorage] addAttribute:NSForegroundColorAttributeName
                                                   value:[NSColor colorWithCalibratedRed:0.1 green:0.3 blue:0.8 alpha:1.0]
                                                   range:linkRange];
                        inRects = NO;
                    }
                }
                break;

            default:
                break;
        }
    }

    // Free our copy of the link region
    free(linkRects);
}

- (void)resetCursorRects
{
    NSRect visRect;
    NSRange glyphRange;
    NSRange charRange;
    NSRange linkRange;
    NSRectArray linkRects;
    unsigned int linkCount;
    unsigned int scanLoc;
    unsigned int index;

    // Create the hand cursor if it hasn't already been made
    if (!sHandCursor)
        sHandCursor = [[NSCursor alloc] initWithImage:[NSImage imageNamed:@"URLTextViewHand"] hotSpot:NSMakePoint(5.0, 0.0)];

    // Find the range of visible characters
    visRect = [[self enclosingScrollView] documentVisibleRect];
    glyphRange = [[self layoutManager] glyphRangeForBoundingRect:visRect inTextContainer:[self textContainer]];
    charRange = [[self layoutManager] characterRangeForGlyphRange:glyphRange actualGlyphRange:nil];

    // Loop through all the visible characters
    scanLoc = charRange.location;
    while (scanLoc < charRange.location + charRange.length) {
        // Find the next range of characters with a link attribute
        if ([[self textStorage] attribute:NSLinkAttributeName atIndex:scanLoc effectiveRange:&linkRange]) {
            // Get the array of rects represented by an attribute range
            linkRects = [[self layoutManager] rectArrayForCharacterRange:linkRange withinSelectedCharacterRange:linkRange inTextContainer:[self textContainer] rectCount:&linkCount];

            // Loop through these rects adding them as cursor rects
            for (index = 0; index < linkCount; index++)
                [self addCursorRect:NSIntersectionRect(visRect, linkRects[index]) cursor:sHandCursor];
        }

        // Even if we didn't find a link, the range returned tells us where to check next
        scanLoc = linkRange.location + linkRange.length;
    }
}

@end

NSRectArray NSCopyRectArray(NSRectArray inSomeRects, int inArraySize)
{
    NSRectArray array;

    array = malloc(sizeof(NSRect) * inArraySize);
    memcpy(array, inSomeRects, sizeof(NSRect) * inArraySize);

    return array;
}

BOOL NSMouseInRects(NSPoint inPoint, NSRectArray inSomeRects, int inArraySize, BOOL inFlipped)
{
    int index;

    for (index = 0; index < inArraySize; index++) {
        if (NSMouseInRect(inPoint, inSomeRects[index], inFlipped))
            return YES;
    }

    return NO;
}

