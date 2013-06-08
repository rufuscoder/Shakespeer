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

#import "SPMainWindowController.h"
#import "SPGrowlBridge.h"

NSString *SP_GROWL_HUB_CONNECTION_CLOSED = @"Hub disconnected";
NSString *SP_GROWL_NEW_PRIVATE_CONVERSATION = @"New private conversation started";
NSString *SP_GROWL_PRIVATE_MESSAGE = @"Private message received";
NSString *SP_GROWL_NICK_IN_MAINCHAT = @"My nick mentioned in main chat";
NSString *SP_GROWL_DOWNLOAD_FINISHED = @"Download finished";

@implementation SPGrowlBridge

+ (SPGrowlBridge *)sharedGrowlBridge
{
    static SPGrowlBridge *sharedGrowlBridge = nil;

    if (sharedGrowlBridge == nil)
        sharedGrowlBridge = [[SPGrowlBridge alloc] init];
    
    return sharedGrowlBridge;
}

- (id)init
{
    if ((self = [super init])) {
        [GrowlApplicationBridge setGrowlDelegate:self];
    }
    
    return self;
}

- (void)notifyWithName:(NSString *)name
            description:(NSString *)aDescription
{
    if ([[NSApplication sharedApplication] isActive])
        return;

    [GrowlApplicationBridge notifyWithTitle:name
                                description:aDescription
                           notificationName:name
                                   iconData:nil
                                   priority:0
                                   isSticky:NO
                               clickContext:name];
}

- (NSDictionary *)registrationDictionaryForGrowl
{
    NSArray *allNotifications = [NSArray arrayWithObjects:
        SP_GROWL_HUB_CONNECTION_CLOSED,
        SP_GROWL_NEW_PRIVATE_CONVERSATION,
        SP_GROWL_PRIVATE_MESSAGE,
        SP_GROWL_NICK_IN_MAINCHAT,
        SP_GROWL_DOWNLOAD_FINISHED,
        nil];

    NSArray *defaultNotifications = [NSArray arrayWithObjects:
        SP_GROWL_HUB_CONNECTION_CLOSED,
        SP_GROWL_NEW_PRIVATE_CONVERSATION,
        SP_GROWL_PRIVATE_MESSAGE,
        nil];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        allNotifications, GROWL_NOTIFICATIONS_ALL,
        defaultNotifications, GROWL_NOTIFICATIONS_DEFAULT,
        nil];

    return dict;
}

- (NSString *)applicationNameForGrowl
{
    return @"ShakesPeer";
}

- (void)growlNotificationWasClicked:(id)clickContext
{
    /* move main window to front */
    [[SPMainWindowController sharedMainWindowController] showWindow:self];
}

@end

