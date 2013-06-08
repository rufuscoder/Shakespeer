/*
 * Copyright 2006 Martin Hedenfalk <martin@bzero.se>
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

#ifndef _test_connection_h_
#define _test_connection_h_

#define TC_RET_OK 0         /* both UDP and TCP tested ok */
#define TC_RET_PRIVPORT 1   /* Refused to test privileged port */
#define TC_RET_TCP_FAIL 2   /* TCP test failed */
#define TC_RET_UDP_FAIL 4   /* UDP test failed */
#define TC_RET_INTERNAL 6   /* Internal error */

int test_connection(int port);

#endif

