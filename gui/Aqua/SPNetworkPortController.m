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
 
#import "SPNetworkPortController.h"
#import "SPUserDefaultKeys.h"

#import <TCMPortMapper/TCMPortMapper.h>


@implementation SPNetworkPortController

static SPNetworkPortController *sharedPortController = nil;

+ (SPNetworkPortController *)sharedInstance
{
  if (!sharedPortController) {
    sharedPortController = [[SPNetworkPortController alloc] init];
  }
  return sharedPortController;
}

- (void)startup
{                               
  [self changePort:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsPort]];
  
  [[TCMPortMapper sharedInstance] start];
}

- (void)shutdown
{
  TCMPortMapper *pm = [TCMPortMapper sharedInstance];
  
  if (lastTCPPortMapping) {
    [lastUDPPortMapping release];
    lastUDPPortMapping = nil;
    [lastTCPPortMapping release];
    lastTCPPortMapping = nil;
  }
  
  if ([pm isRunning])
    [pm stopBlocking];
}

- (void)changePort:(int)port
{
  // avoid "changing" port to the current port (which should never happen, really); this
  // seems to confuse the TCMPortMapping framework
  if (lastTCPPortMapping && port == [lastTCPPortMapping localPort])
    return;
    
  TCMPortMapper *pm = [TCMPortMapper sharedInstance];
  
  if (lastTCPPortMapping) {
    // remove the previous port mappings
    [pm removePortMapping:lastUDPPortMapping];
    [lastUDPPortMapping autorelease];
    lastUDPPortMapping = nil;
    
    [pm removePortMapping:lastTCPPortMapping];
    [lastTCPPortMapping autorelease];
    lastTCPPortMapping = nil;
  }
  
  TCMPortMapping *newTCPMapping = 
    [TCMPortMapping portMappingWithLocalPort:port 
                         desiredExternalPort:port 
                           transportProtocol:TCMPortMappingTransportProtocolTCP
                                    userInfo:nil];
                                    
  TCMPortMapping *newUDPMapping = 
    [TCMPortMapping portMappingWithLocalPort:port 
                         desiredExternalPort:port 
                           transportProtocol:TCMPortMappingTransportProtocolUDP
                                    userInfo:nil];

  [pm addPortMapping:newUDPMapping];
  [pm addPortMapping:newTCPMapping];
  
  lastUDPPortMapping = [newUDPMapping retain];
  lastTCPPortMapping = [newTCPMapping retain];
}


@end
