/* vim: ft=objc
 *
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#import <Cocoa/Cocoa.h>
#import "SPSideBar.h"

@interface SPChatWindowController : NSObject <SPSideBarItem>
{
    IBOutlet NSTextView *chatTextView;
    IBOutlet NSView *chatView;
    IBOutlet NSTextField *inputField;

    NSString *hubAddress;
    NSString *nick;
    NSString *myNick;
    NSString *hubname;
    BOOL highlighted;
    BOOL firstMessage;
}

- (id)initWithRemoteNick:(NSString *)remoteNick hub:(NSString *)aHubAddress myNick:(NSString *)aMyNick;

- (IBAction)sendMessage:(id)sender;
- (void)focusChatInput;

- (NSImage *)image;
- (NSString *)title;
- (void)unbindControllers;

@end

