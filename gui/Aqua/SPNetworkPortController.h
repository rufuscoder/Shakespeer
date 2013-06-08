/* vim: ft=objc
 *
 * Copyright 2008 Håkan Waara <hwaara@gmail.com>
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

/* Manages ShakesPeer's UPnP/NAT-PMP support (automatic port forwarding). */
@class TCMPortMapping;
@interface SPNetworkPortController : NSObject 
{
  // we could combine these into one object, but the framework has a bug (1.0-r4) 
  // where it won't remove both the TCP and the UDP port mapping on shutdown if we do.
  TCMPortMapping *lastUDPPortMapping;
  TCMPortMapping *lastTCPPortMapping;
}

+ (SPNetworkPortController *)sharedInstance;

- (void)startup;
- (void)changePort:(int)port;
- (void)shutdown;

@end
