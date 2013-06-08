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

#import <Cocoa/Cocoa.h>
#import <Growl/Growl.h>

extern NSString *SP_GROWL_HUB_CONNECTION_CLOSED;
extern NSString *SP_GROWL_NEW_PRIVATE_CONVERSATION;
extern NSString *SP_GROWL_PRIVATE_MESSAGE;
extern NSString *SP_GROWL_NICK_IN_MAINCHAT;
extern NSString *SP_GROWL_DOWNLOAD_FINISHED;

@interface SPGrowlBridge : NSObject <GrowlApplicationBridgeDelegate>
{
}

+ (SPGrowlBridge *)sharedGrowlBridge;
- (void)notifyWithName:(NSString *)name
           description:(NSString *)aDescription;

@end

