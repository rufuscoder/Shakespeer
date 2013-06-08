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

#import "SPMessagePanel.h"
#import "SPNotificationNames.h"

@implementation SPMessagePanel

+ (id)sharedMessagePanel
{
    static id sharedMessagePanel = nil;

    if (sharedMessagePanel == nil)
        sharedMessagePanel = [[SPMessagePanel alloc] init];

    return sharedMessagePanel;
}

- (id)init
{
    if ((self = [super initWithWindowNibName:@"MessagePanel" owner:self])) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(statusMessageNotification:)
                                                     name:SPNotificationStatusMessage
                                                   object:nil];

        /* force loading of the window, otherwise the textView won't be bound
         * when status message notifications are received */
        [self window];
        
        [self setWindowFrameAutosaveName:@"MessagePanel"];
    }
    
    return self;
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

- (void)dealloc
{
    [super dealloc];
}

- (void)statusMessage:(NSString *)aMessage hub:(NSString *)hubAddress
{
    NSString *dateString = [[NSDate date] descriptionWithCalendarFormat:@"%H:%M"
                                                               timeZone:nil
                                                                 locale:nil];

    NSMutableAttributedString *attrmsg = [[NSMutableAttributedString alloc]
        initWithString:[NSString stringWithFormat:@"[%@] %@\n", dateString, aMessage]];
    [[textView textStorage] appendAttributedString:attrmsg];
    [attrmsg release];
    [textView scrollRangeToVisible:NSMakeRange([[textView textStorage] length], 0)];
}

- (void)statusMessageNotification:(NSNotification *)aNotification
{
    [self statusMessage:[[aNotification userInfo] objectForKey:@"message"]
                    hub:[[aNotification userInfo] objectForKey:@"hubAddress"]];
}

@end

