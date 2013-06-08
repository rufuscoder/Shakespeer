/* vim: ft=objc
 *
 * Copyright 2008 Markus Amalthea Magnuson <markus.magnuson@gmail.com>
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
 *
 * Part of this code is from Adium (AIMenuAdditions.m)
 * Copyright (C) 2001-2005, Adam Iser (adamiser@mac.com | http://www.adiumx.com)
 */

#import "NSMenu-MassRemovalAdditions.h"

@implementation NSMenu (ItemCreationAdditions)

- (void)removeAllItems
{
	int count = [self numberOfItems];
	while (count--) {
		[self removeItemAtIndex:0];
	}
}

- (void)removeAllItemsButFirst
{
	int count = [self numberOfItems];
	if (count > 1) {
		while (--count) {
			[self removeItemAtIndex:1];
		}
	}
}

@end
