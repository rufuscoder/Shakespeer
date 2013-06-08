/*
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

#ifndef _encoding_h_
#define _encoding_h_

char *str_legacy_to_utf8(const char *string, const char *encoding);
char *str_legacy_to_utf8_lossy(const char *string, const char *encoding);
char *str_convert_to_unescaped_utf8(const char *string, const char *encoding);
char *str_convert_to_escaped_utf8(const char *string, const char *encoding);
char *str_utf8_to_legacy(const char *string, const char *encoding);
char *str_utf8_to_escaped_legacy(const char *string, const char *encoding);
char *str_unescape_unicode(const char *utf8_string);

int str_need_unescape_unicode(const char *utf8_string);

#endif

