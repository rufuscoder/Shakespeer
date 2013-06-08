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

#import "SidebarCell.h"
#import "SPSideBar.h"

@implementation SidebarCell

- (void)dealloc
{
    [super dealloc];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    int fontSize = 11;

    /* set our paragraph styles */
    NSMutableParagraphStyle *paragraphStyle = [[[NSParagraphStyle  defaultParagraphStyle] mutableCopy] autorelease];
    [paragraphStyle setAlignment:NSLeftTextAlignment];
    [paragraphStyle setLineBreakMode:NSLineBreakByTruncatingTail];

    if ([objectValue isKindOfClass:[NSString class]]) {
        [controlView lockFocus];

        NSColor *textColor = [NSColor grayColor];

        fontSize = 10;

        cellFrame.origin.x += 2;
        cellFrame.size.width -= 4;

        NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
            textColor, NSForegroundColorAttributeName,
            paragraphStyle, NSParagraphStyleAttributeName,
            [NSFont systemFontOfSize:fontSize], NSFontAttributeName,
            nil];

        NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:objectValue
                                                                               attributes:attributes];

        NSRect inset = cellFrame;
        inset.origin.y += floor(((float)cellFrame.size.height - fontSize) / 2) - 2;
        [attributedString drawInRect:inset];
        [attributedString release];

        int w = [objectValue sizeWithAttributes:attributes].width + 4;
        cellFrame.origin.x += w;
        cellFrame.size.width -= w;

        int y = inset.origin.y + [objectValue sizeWithAttributes:attributes].height - 2;
        NSPoint fromPoint = NSMakePoint(cellFrame.origin.x, y);
        NSPoint toPoint = NSMakePoint(cellFrame.origin.x + cellFrame.size.width - 2, y);
        if (toPoint.x > fromPoint.x) {
            [[NSColor colorWithCalibratedRed:0.702 green:0.702 blue:0.702 alpha:1.0] set];
            [NSBezierPath setDefaultLineWidth:2.0];
            NSGraphicsContext* context = [NSGraphicsContext currentContext];
            BOOL antiAlias = [context shouldAntialias];
            [context setShouldAntialias:NO];
            [NSBezierPath strokeLineFromPoint:fromPoint toPoint:toPoint];
            [context setShouldAntialias:antiAlias];
        }

        [controlView unlockFocus];
        
        return;
    }

    NSImage *gradient;
    /* Determine whether we should draw a blue or grey gradient. */
    /* We will automatically redraw when our parent view loses/gains focus, 
    or when our parent window loses/gains main/key status. */
    if (([[controlView window] firstResponder] == controlView) && 
            [[controlView window] isMainWindow] &&
            [[controlView window] isKeyWindow]) {
        gradient = [NSImage imageNamed:@"highlight_blue.tiff"];
    }
    else {
        gradient = [NSImage imageNamed:@"highlight_grey.tiff"];
    }

    /* Make sure we draw the gradient the correct way up. */
    [gradient setFlipped:YES];
    int i = 0;
    
    [controlView lockFocus];

    if ([self isHighlighted]) {
        /* We're selected, so draw the gradient background. */
        NSSize gradientSize = [gradient size];
        for (i = cellFrame.origin.x; i < (cellFrame.origin.x + cellFrame.size.width); i += gradientSize.width) {
            [gradient drawInRect:NSMakeRect(i, cellFrame.origin.y, gradientSize.width, cellFrame.size.height)
                        fromRect:NSMakeRect(0, 0, gradientSize.width, gradientSize.height)
                       operation:NSCompositeSourceOver
                        fraction:1.0];
        }
    }
        
    /* Now draw our image. */
    NSImage *img = [objectValue image];
    [img setFlipped:YES];
    NSSize imgSize = [img size];
    NSRect imageRect = cellFrame;
    imageRect.size = NSMakeSize(16,16);
    imageRect.origin.x += 2 + floor((16.0 - imgSize.width) / 2);
    imageRect.origin.y += floor(((float)cellFrame.size.height - imgSize.height) / 2);
    [img drawInRect:imageRect fromRect:NSMakeRect(0,0,16,16) operation:NSCompositeSourceOver fraction:1.0f];
    [img setFlipped:NO];

    /* Decrease the cell width by the width of the image we drew and its left padding */
    cellFrame.size.width -= 2 + imageRect.size.width;

    /* Shift the origin over to the right edge of the image we just drew */
    cellFrame.origin.x += 2 + imageRect.size.width;

    /* Now draw our text */
    NSColor *textColor = [NSColor blackColor];
    if ([self isHighlighted])
        textColor = [NSColor whiteColor];

    BOOL canClose = [objectValue respondsToSelector:@selector(canClose)] && [objectValue canClose];
    if (canClose && cellFrame.size.width <= 34)
        canClose = NO;

    NSRect inset = cellFrame;
    inset.origin.x += 2;
    inset.size.width -= 4 + (canClose ? 20 : 0);
    inset.origin.y += floor(((float)cellFrame.size.height - fontSize) / 2) - 2;

    BOOL highlighted = NO;
    if ([objectValue respondsToSelector:@selector(isHighlighted)])
        highlighted = [objectValue isHighlighted];

    NSFont *cellFont;
    if (highlighted)
        cellFont = [NSFont boldSystemFontOfSize:fontSize];
    else
        cellFont = [NSFont systemFontOfSize:fontSize];

    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        textColor, NSForegroundColorAttributeName,
        paragraphStyle, NSParagraphStyleAttributeName,
        cellFont, NSFontAttributeName,
        nil];

    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:[objectValue title]
                                                                           attributes:attributes];
    
    [attributedString drawInRect:inset];
    [attributedString release];

    if (canClose) {
        /* display the close button */
        img = [NSImage imageNamed:hoveringClose ? @"TableClosePressed.tiff" : @"TableClose.tiff"];
        imgSize = [img size];
        closeButtonRect = cellFrame;
        closeButtonRect.size = NSMakeSize(16,16);
        closeButtonRect.origin.x += cellFrame.size.width - imageRect.size.width - 2;
        closeButtonRect.origin.y += floor(((float)cellFrame.size.height - imgSize.height) / 2);
        [img drawInRect:closeButtonRect
               fromRect:NSMakeRect(0,0,16,16)
              operation:NSCompositeSourceOver
               fraction:1.0f];
    }

    [controlView unlockFocus];
}

- (void)setObjectValue:(id <NSCopying>)anObject
{
    objectValue = anObject;
}

/* Start Tracking.  Redisplay the close button as pressed */
- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView
{
    if (NSPointInRect(startPoint, closeButtonRect)) {
        hoveringClose = YES;
        [controlView setNeedsDisplayInRect:closeButtonRect];
    }

    return YES;
}

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView
{
    BOOL hovering = NSPointInRect(currentPoint, closeButtonRect);

    if (hoveringClose != hovering) {
        hoveringClose = hovering;
        [controlView setNeedsDisplayInRect:closeButtonRect];
    }

    return YES;
}

- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:(NSView *)controlView mouseIsUp:(BOOL)flag
{
    BOOL hovering = NSPointInRect(stopPoint, closeButtonRect);

    if (hovering) {
        /* If the mouse was released over the close button, close our tab */
        [(SPSideBar *)controlView closeSelectedItem:self];
    }

    hoveringClose = NO;
    [controlView setNeedsDisplayInRect:closeButtonRect];
}

@end

