/* nfkc.c --- Unicode normalization utilities.
 * Copyright (C) 2002, 2003, 2004, 2006  Simon Josefsson
 *
 * This file is part of GNU Libidn.
 *
 * GNU Libidn is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GNU Libidn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GNU Libidn; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "nfkc.h"
#include "dstring.h"

#include <wchar.h>

#ifndef __STDC_ISO_10646__
# define __STDC_ISO_10646__
#endif

/* from gunicode.h */
/* These are the possible character classifications.
 * See http://www.unicode.org/Public/UNIDATA/UnicodeData.html
 */
typedef enum
{
  G_UNICODE_CONTROL,
  G_UNICODE_FORMAT,
  G_UNICODE_UNASSIGNED,
  G_UNICODE_PRIVATE_USE,
  G_UNICODE_SURROGATE,
  G_UNICODE_LOWERCASE_LETTER,
  G_UNICODE_MODIFIER_LETTER,
  G_UNICODE_OTHER_LETTER,
  G_UNICODE_TITLECASE_LETTER,
  G_UNICODE_UPPERCASE_LETTER,
  G_UNICODE_COMBINING_MARK,
  G_UNICODE_ENCLOSING_MARK,
  G_UNICODE_NON_SPACING_MARK,
  G_UNICODE_DECIMAL_NUMBER,
  G_UNICODE_LETTER_NUMBER,
  G_UNICODE_OTHER_NUMBER,
  G_UNICODE_CONNECT_PUNCTUATION,
  G_UNICODE_DASH_PUNCTUATION,
  G_UNICODE_CLOSE_PUNCTUATION,
  G_UNICODE_FINAL_PUNCTUATION,
  G_UNICODE_INITIAL_PUNCTUATION,
  G_UNICODE_OTHER_PUNCTUATION,
  G_UNICODE_OPEN_PUNCTUATION,
  G_UNICODE_CURRENCY_SYMBOL,
  G_UNICODE_MODIFIER_SYMBOL,
  G_UNICODE_MATH_SYMBOL,
  G_UNICODE_OTHER_SYMBOL,
  G_UNICODE_LINE_SEPARATOR,
  G_UNICODE_PARAGRAPH_SEPARATOR,
  G_UNICODE_SPACE_SEPARATOR
} GUnicodeType;

#include "gunichartables.h"

/* This file contains functions from GLIB, including gutf8.c and
 * gunidecomp.c, all licensed under LGPL and copyright hold by:
 *
 *  Copyright (C) 1999, 2000 Tom Tromey
 *  Copyright 2000 Red Hat, Inc.
 */

/* Code from GLIB gutf8.c starts here. */

#define UTF8_COMPUTE(Char, Mask, Len)		\
  if (Char < 128)				\
    {						\
      Len = 1;					\
      Mask = 0x7f;				\
    }						\
  else if ((Char & 0xe0) == 0xc0)		\
    {						\
      Len = 2;					\
      Mask = 0x1f;				\
    }						\
  else if ((Char & 0xf0) == 0xe0)		\
    {						\
      Len = 3;					\
      Mask = 0x0f;				\
    }						\
  else if ((Char & 0xf8) == 0xf0)		\
    {						\
      Len = 4;					\
      Mask = 0x07;				\
    }						\
  else if ((Char & 0xfc) == 0xf8)		\
    {						\
      Len = 5;					\
      Mask = 0x03;				\
    }						\
  else if ((Char & 0xfe) == 0xfc)		\
    {						\
      Len = 6;					\
      Mask = 0x01;				\
    }						\
  else						\
    Len = -1;

#define UTF8_LENGTH(Char)			\
  ((Char) < 0x80 ? 1 :				\
   ((Char) < 0x800 ? 2 :			\
    ((Char) < 0x10000 ? 3 :			\
     ((Char) < 0x200000 ? 4 :			\
      ((Char) < 0x4000000 ? 5 : 6)))))


#define UTF8_GET(Result, Chars, Count, Mask, Len)	\
  (Result) = (Chars)[0] & (Mask);			\
  for ((Count) = 1; (Count) < (Len); ++(Count))		\
    {							\
      if (((Chars)[(Count)] & 0xc0) != 0x80)		\
	{						\
	  (Result) = -1;				\
	  break;					\
	}						\
      (Result) <<= 6;					\
      (Result) |= ((Chars)[(Count)] & 0x3f);		\
    }

#define UNICODE_VALID(Char)			\
  ((Char) < 0x110000 &&				\
   (((Char) & 0xFFFFF800) != 0xD800) &&		\
   ((Char) < 0xFDD0 || (Char) > 0xFDEF) &&	\
   ((Char) & 0xFFFE) != 0xFFFE)

#define CONTINUATION_CHAR                           \
 G_STMT_START {                                     \
  if ((*(guchar *)p & 0xc0) != 0x80) /* 10xxxxxx */ \
    goto error;                                     \
  val <<= 6;                                        \
  val |= (*(guchar *)p) & 0x3f;                     \
 } G_STMT_END



static const gchar utf8_skip_data[256] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5,
  5, 5, 5, 6, 6, 1, 1
};

const gchar *const g_utf8_skip = utf8_skip_data;

/**
 * g_utf8_find_prev_char:
 * @str: pointer to the beginning of a UTF-8 encoded string
 * @p: pointer to some position within @str
 * 
 * Given a position @p with a UTF-8 encoded string @str, find the start
 * of the previous UTF-8 character starting before @p. Returns %NULL if no
 * UTF-8 characters are present in @p before @str.
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL.
 **/
gchar *
g_utf8_find_prev_char (const char *str,
		       const char *p)
{
  for (--p; p >= str; --p)
    {
      if ((*p & 0xc0) != 0x80)
	return (gchar *)p;
    }
  return NULL;
}

/**
 * g_utf8_find_next_char:
 * @p: a pointer to a position within a UTF-8 encoded string
 * @end: a pointer to the end of the string, or %NULL to indicate
 *        that the string is nul-terminated, in which case
 *        the returned value will be 
 *
 * Finds the start of the next UTF-8 character in the string after @p.
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 * 
 * Return value: a pointer to the found character or %NULL
 **/
gchar *
g_utf8_find_next_char (const gchar *p,
		       const gchar *end)
{
  if (*p)
    {
      if (end)
	for (++p; p < end && (*p & 0xc0) == 0x80; ++p)
	  ;
      else
	for (++p; (*p & 0xc0) == 0x80; ++p)
	  ;
    }
  return (p == end) ? NULL : (gchar *)p;
}


/**
 * g_unichar_validate:
 * @ch: a Unicode character
 * 
 * Checks whether @ch is a valid Unicode character. Some possible
 * integer values of @ch will not be valid. 0 is considered a valid
 * character, though it's normally a string terminator.
 * 
 * Return value: %TRUE if @ch is a valid Unicode character
 **/
gboolean
g_unichar_validate (gunichar ch)
{
  return UNICODE_VALID (ch);
}

/*
 * g_utf8_strlen:
 * @p: pointer to the start of a UTF-8 encoded string.
 * @max: the maximum number of bytes to examine. If @max
 *       is less than 0, then the string is assumed to be
 *       nul-terminated. If @max is 0, @p will not be examined and
 *       may be %NULL.
 *
 * Returns the length of the string in characters.
 *
 * Return value: the length of the string in characters
 **/
glong
g_utf8_strlen (const gchar * p, gssize max)
{
  glong len = 0;
  const gchar *start = p;
  g_return_val_if_fail (p != NULL || max == 0, 0);

  if (max < 0)
    {
      while (*p)
	{
	  p = g_utf8_next_char (p);
	  ++len;
	}
    }
  else
    {
      if (max == 0 || !*p)
	return 0;

      p = g_utf8_next_char (p);

      while (p - start < max && *p)
	{
	  ++len;
	  p = g_utf8_next_char (p);
	}

      /* only do the last len increment if we got a complete
       * char (don't count partial chars)
       */
      if (p - start == max)
	++len;
    }

  return len;
}

/**
 * g_utf8_offset_to_pointer:
 * @str: a UTF-8 encoded string
 * @offset: a character offset within @str
 * 
 * Converts from an integer character offset to a pointer to a position
 * within the string.
 * 
 * Return value: the resulting pointer
 **/
gchar *
g_utf8_offset_to_pointer  (const gchar *str,
                           glong        offset)    
{
  const gchar *s = str;
  while (offset--)
    s = g_utf8_next_char (s);
  
  return (gchar *)s;
}

/**
 * g_utf8_pointer_to_offset:
 * @str: a UTF-8 encoded string
 * @pos: a pointer to a position within @str
 * 
 * Converts from a pointer to position within a string to a integer
 * character offset.
 * 
 * Return value: the resulting character offset
 **/
glong
g_utf8_pointer_to_offset (const gchar *str,
                          const gchar *pos)
{
  const gchar *s = str;
  glong offset = 0;
  
  while (s < pos)
    {
      s = g_utf8_next_char (s);
      offset++;
    }

  return offset;
}

/**
 * g_utf8_strncpy:
 * @dest: buffer to fill with characters from @src
 * @src: UTF-8 encoded string
 * @n: character count
 * 
 * Like the standard C strncpy() function, but 
 * copies a given number of characters instead of a given number of 
 * bytes. The @src string must be valid UTF-8 encoded text. 
 * (Use g_utf8_validate() on all text before trying to use UTF-8 
 * utility functions with it.)
 * 
 * Return value: @dest
 **/
gchar *
g_utf8_strncpy (gchar       *dest,
                const gchar *src,
                gsize        n)
{
  const gchar *s = src;
  while (n && *s)
    {
      s = g_utf8_next_char(s);
      n--;
    }
  strncpy(dest, src, s - src);
  dest[s - src] = 0;
  return dest;
}


/*
 * g_utf8_get_char:
 * @p: a pointer to Unicode character encoded as UTF-8
 *
 * Converts a sequence of bytes encoded as UTF-8 to a Unicode character.
 * If @p does not point to a valid UTF-8 encoded character, results are
 * undefined. If you are not sure that the bytes are complete
 * valid Unicode characters, you should use g_utf8_get_char_validated()
 * instead.
 *
 * Return value: the resulting character
 **/
gunichar
g_utf8_get_char (const gchar * p)
{
  int i, mask = 0, len;
  gunichar result;
  unsigned char c = (unsigned char) *p;

  UTF8_COMPUTE (c, mask, len);
  if (len == -1)
    return (gunichar) - 1;
  UTF8_GET (result, p, i, mask, len);

  return result;
}

/* Like g_utf8_get_char, but take a maximum length
 * and return (gunichar)-2 on incomplete trailing character
 */
static inline gunichar
g_utf8_get_char_extended (const  gchar *p,
			  gssize max_len)  
{
  guint i, len;
  gunichar wc = (guchar) *p;

  if (wc < 0x80)
    {
      return wc;
    }
  else if (wc < 0xc0)
    {
      return (gunichar)-1;
    }
  else if (wc < 0xe0)
    {
      len = 2;
      wc &= 0x1f;
    }
  else if (wc < 0xf0)
    {
      len = 3;
      wc &= 0x0f;
    }
  else if (wc < 0xf8)
    {
      len = 4;
      wc &= 0x07;
    }
  else if (wc < 0xfc)
    {
      len = 5;
      wc &= 0x03;
    }
  else if (wc < 0xfe)
    {
      len = 6;
      wc &= 0x01;
    }
  else
    {
      return (gunichar)-1;
    }
  
  if (max_len >= 0 && len > max_len)
    {
      for (i = 1; i < max_len; i++)
	{
	  if ((((guchar *)p)[i] & 0xc0) != 0x80)
	    return (gunichar)-1;
	}
      return (gunichar)-2;
    }

  for (i = 1; i < len; ++i)
    {
      gunichar ch = ((guchar *)p)[i];
      
      if ((ch & 0xc0) != 0x80)
	{
	  if (ch)
	    return (gunichar)-1;
	  else
	    return (gunichar)-2;
	}

      wc <<= 6;
      wc |= (ch & 0x3f);
    }

  if (UTF8_LENGTH(wc) != len)
    return (gunichar)-1;
  
  return wc;
}

/**
 * g_utf8_get_char_validated:
 * @p: a pointer to Unicode character encoded as UTF-8
 * @max_len: the maximum number of bytes to read, or -1, for no maximum.
 * 
 * Convert a sequence of bytes encoded as UTF-8 to a Unicode character.
 * This function checks for incomplete characters, for invalid characters
 * such as characters that are out of the range of Unicode, and for
 * overlong encodings of valid characters.
 * 
 * Return value: the resulting character. If @p points to a partial
 *    sequence at the end of a string that could begin a valid 
 *    character, returns (gunichar)-2; otherwise, if @p does not point 
 *    to a valid UTF-8 encoded Unicode character, returns (gunichar)-1.
 **/
gunichar
g_utf8_get_char_validated (const  gchar *p,
			   gssize max_len)
{
  gunichar result = g_utf8_get_char_extended (p, max_len);

  if (result & 0x80000000)
    return result;
  else if (!UNICODE_VALID (result))
    return (gunichar)-1;
  else
    return result;
}


/*
 * g_unichar_to_utf8:
 * @c: a ISO10646 character code
 * @outbuf: output buffer, must have at least 6 bytes of space.
 *       If %NULL, the length will be computed and returned
 *       and nothing will be written to @outbuf.
 *
 * Converts a single character to UTF-8.
 *
 * Return value: number of bytes written
 **/
int
g_unichar_to_utf8 (gunichar c, gchar * outbuf)
{
  guint len = 0;
  int first;
  int i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
  else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  if (outbuf)
    {
      for (i = len - 1; i > 0; --i)
	{
	  outbuf[i] = (c & 0x3f) | 0x80;
	  c >>= 6;
	}
      outbuf[0] = c | first;
    }

  return len;
}

/*
 * g_utf8_to_ucs4_fast:
 * @str: a UTF-8 encoded string
 * @len: the maximum length of @str to use. If @len < 0, then
 *       the string is nul-terminated.
 * @items_written: location to store the number of characters in the
 *                 result, or %NULL.
 *
 * Convert a string from UTF-8 to a 32-bit fixed width
 * representation as UCS-4, assuming valid UTF-8 input.
 * This function is roughly twice as fast as g_utf8_to_ucs4()
 * but does no error checking on the input.
 *
 * Return value: a pointer to a newly allocated UCS-4 string.
 *               This value must be freed with g_free().
 **/
gunichar *
g_utf8_to_ucs4_fast (const gchar * str, glong len, glong * items_written)
{
  gint j, charlen;
  gunichar *result;
  gint n_chars, i;
  const gchar *p;

  g_return_val_if_fail (str != NULL, NULL);

  p = str;
  n_chars = 0;
  if (len < 0)
    {
      while (*p)
	{
	  p = g_utf8_next_char (p);
	  ++n_chars;
	}
    }
  else
    {
      while (p < str + len && *p)
	{
	  p = g_utf8_next_char (p);
	  ++n_chars;
	}
    }

  result = g_new (gunichar, n_chars + 1);
  if (!result)
    return NULL;

  p = str;
  for (i = 0; i < n_chars; i++)
    {
      gunichar wc = ((unsigned char *) p)[0];

      if (wc < 0x80)
	{
	  result[i] = wc;
	  p++;
	}
      else
	{
	  if (wc < 0xe0)
	    {
	      charlen = 2;
	      wc &= 0x1f;
	    }
	  else if (wc < 0xf0)
	    {
	      charlen = 3;
	      wc &= 0x0f;
	    }
	  else if (wc < 0xf8)
	    {
	      charlen = 4;
	      wc &= 0x07;
	    }
	  else if (wc < 0xfc)
	    {
	      charlen = 5;
	      wc &= 0x03;
	    }
	  else
	    {
	      charlen = 6;
	      wc &= 0x01;
	    }

	  for (j = 1; j < charlen; j++)
	    {
	      wc <<= 6;
	      wc |= ((unsigned char *) p)[j] & 0x3f;
	    }

	  result[i] = wc;
	  p += charlen;
	}
    }
  result[i] = 0;

  if (items_written)
    *items_written = i;

  return result;
}

/*
 * g_ucs4_to_utf8:
 * @str: a UCS-4 encoded string
 * @len: the maximum length of @str to use. If @len < 0, then
 *       the string is terminated with a 0 character.
 * @items_read: location to store number of characters read read, or %NULL.
 * @items_written: location to store number of bytes written or %NULL.
 *                 The value here stored does not include the trailing 0
 *                 byte.
 * @error: location to store the error occuring, or %NULL to ignore
 *         errors. Any of the errors in #GConvertError other than
 *         %G_CONVERT_ERROR_NO_CONVERSION may occur.
 *
 * Convert a string from a 32-bit fixed width representation as UCS-4.
 * to UTF-8. The result will be terminated with a 0 byte.
 *
 * Return value: a pointer to a newly allocated UTF-8 string.
 *               This value must be freed with g_free(). If an
 *               error occurs, %NULL will be returned and
 *               @error set.
 **/
gchar *
g_ucs4_to_utf8 (const gunichar * str,
		glong len,
		glong * items_read, glong * items_written, GError ** error)
{
  gint result_length;
  gchar *result = NULL;
  gchar *p;
  gint i;

  result_length = 0;
  for (i = 0; len < 0 || i < len; i++)
    {
      if (!str[i])
	break;

      if (str[i] >= 0x80000000)
	{
	  if (items_read)
	    *items_read = i;

	  g_set_error (error, G_CONVERT_ERROR,
		       G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
		       _("Character out of range for UTF-8"));
	  goto err_out;
	}

      result_length += UTF8_LENGTH (str[i]);
    }

  result = g_malloc (result_length + 1);
  if (!result)
    return NULL;
  p = result;

  i = 0;
  while (p < result + result_length)
    p += g_unichar_to_utf8 (str[i++], p);

  *p = '\0';

  if (items_written)
    *items_written = p - result;

err_out:
  if (items_read)
    *items_read = i;

  return result;
}

static const gchar *
fast_validate (const char *str)

{
  gunichar val = 0;
  gunichar min = 0;
  const gchar *p;

  for (p = str; *p; p++)
    {
      if (*(guchar *)p < 128)
	/* done */;
      else 
	{
	  const gchar *last;
	  
	  last = p;
	  if ((*(guchar *)p & 0xe0) == 0xc0) /* 110xxxxx */
	    {
	      if (G_UNLIKELY ((*(guchar *)p & 0x1e) == 0))
		goto error;
	      p++;
	      if (G_UNLIKELY ((*(guchar *)p & 0xc0) != 0x80)) /* 10xxxxxx */
		goto error;
	    }
	  else
	    {
	      if ((*(guchar *)p & 0xf0) == 0xe0) /* 1110xxxx */
		{
		  min = (1 << 11);
		  val = *(guchar *)p & 0x0f;
		  goto TWO_REMAINING;
		}
	      else if ((*(guchar *)p & 0xf8) == 0xf0) /* 11110xxx */
		{
		  min = (1 << 16);
		  val = *(guchar *)p & 0x07;
		}
	      else
		goto error;
	      
	      p++;
	      CONTINUATION_CHAR;
	    TWO_REMAINING:
	      p++;
	      CONTINUATION_CHAR;
	      p++;
	      CONTINUATION_CHAR;
	      
	      if (G_UNLIKELY (val < min))
		goto error;

	      if (G_UNLIKELY (!UNICODE_VALID(val)))
		goto error;
	    } 
	  
	  continue;
	  
	error:
	  return last;
	}
    }

  return p;
}

static const gchar *
fast_validate_len (const char *str,
		   gssize      max_len)

{
  gunichar val = 0;
  gunichar min = 0;
  const gchar *p;

  for (p = str; (max_len < 0 || (p - str) < max_len) && *p; p++)
    {
      if (*(guchar *)p < 128)
	/* done */;
      else 
	{
	  const gchar *last;
	  
	  last = p;
	  if ((*(guchar *)p & 0xe0) == 0xc0) /* 110xxxxx */
	    {
	      if (G_UNLIKELY (max_len >= 0 && max_len - (p - str) < 2))
		goto error;
	      
	      if (G_UNLIKELY ((*(guchar *)p & 0x1e) == 0))
		goto error;
	      p++;
	      if (G_UNLIKELY ((*(guchar *)p & 0xc0) != 0x80)) /* 10xxxxxx */
		goto error;
	    }
	  else
	    {
	      if ((*(guchar *)p & 0xf0) == 0xe0) /* 1110xxxx */
		{
		  if (G_UNLIKELY (max_len >= 0 && max_len - (p - str) < 3))
		    goto error;
		  
		  min = (1 << 11);
		  val = *(guchar *)p & 0x0f;
		  goto TWO_REMAINING;
		}
 	      else if ((*(guchar *)p & 0xf8) == 0xf0) /* 11110xxx */
		{
		  if (G_UNLIKELY (max_len >= 0 && max_len - (p - str) < 4))
		    goto error;
		  
		  min = (1 << 16);
		  val = *(guchar *)p & 0x07;
		}
	      else
		goto error;
	      
	      p++;
	      CONTINUATION_CHAR;
	    TWO_REMAINING:
	      p++;
	      CONTINUATION_CHAR;
	      p++;
	      CONTINUATION_CHAR;
	      
	      if (G_UNLIKELY (val < min))
		goto error;
	      if (G_UNLIKELY (!UNICODE_VALID(val)))
		goto error;
	    } 
	  
	  continue;
	  
	error:
	  return last;
	}
    }

  return p;
}

/**
 * g_utf8_validate:
 * @str: a pointer to character data
 * @max_len: max bytes to validate, or -1 to go until NUL
 * @end: return location for end of valid data
 * 
 * Validates UTF-8 encoded text. @str is the text to validate;
 * if @str is nul-terminated, then @max_len can be -1, otherwise
 * @max_len should be the number of bytes to validate.
 * If @end is non-%NULL, then the end of the valid range
 * will be stored there (i.e. the start of the first invalid 
 * character if some bytes were invalid, or the end of the text 
 * being validated otherwise).
 *
 * Note that g_utf8_validate() returns %FALSE if @max_len is 
 * positive and NUL is met before @max_len bytes have been read.
 *
 * Returns %TRUE if all of @str was valid. Many GLib and GTK+
 * routines <emphasis>require</emphasis> valid UTF-8 as input;
 * so data read from a file or the network should be checked
 * with g_utf8_validate() before doing anything else with it.
 * 
 * Return value: %TRUE if the text was valid UTF-8
 **/
gboolean
g_utf8_validate (const char   *str,
		 gssize        max_len,    
		 const gchar **end)
{
  const gchar *p;

  if (max_len < 0)
    p = fast_validate (str);
  else
    p = fast_validate_len (str, max_len);

  if (end)
    *end = p;

  if ((max_len >= 0 && p != str + max_len) ||
      (max_len < 0 && *p != '\0'))
    return FALSE;
  else
    return TRUE;
}
/* Code from GLIB gunidecomp.c starts here. */

#include "gunidecomp.h"
#include "gunicomp.h"

#define CC_PART1(Page, Char) \
  ((combining_class_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part1[Page]][Char]))

#define CC_PART2(Page, Char) \
  ((combining_class_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part2[Page]][Char]))

#define COMBINING_CLASS(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? CC_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? CC_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : 0))

/* constants for hangul syllable [de]composition */
#define SBase 0xAC00
#define LBase 0x1100
#define VBase 0x1161
#define TBase 0x11A7
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

/*
 * g_unicode_canonical_ordering:
 * @string: a UCS-4 encoded string.
 * @len: the maximum length of @string to use.
 *
 * Computes the canonical ordering of a string in-place.
 * This rearranges decomposed characters in the string
 * according to their combining classes.  See the Unicode
 * manual for more information.
 **/
void
g_unicode_canonical_ordering (gunichar * string, gsize len)
{
  gsize i;
  int swap = 1;

  while (swap)
    {
      int last;
      swap = 0;
      last = COMBINING_CLASS (string[0]);
      for (i = 0; i < len - 1; ++i)
	{
	  int next = COMBINING_CLASS (string[i + 1]);
	  if (next != 0 && last > next)
	    {
	      gsize j;
	      /* Percolate item leftward through string.  */
	      for (j = i + 1; j > 0; --j)
		{
		  gunichar t;
		  if (COMBINING_CLASS (string[j - 1]) <= next)
		    break;
		  t = string[j];
		  string[j] = string[j - 1];
		  string[j - 1] = t;
		  swap = 1;
		}
	      /* We're re-entering the loop looking at the old
	         character again.  */
	      next = last;
	    }
	  last = next;
	}
    }
}

/* http://www.unicode.org/unicode/reports/tr15/#Hangul
 * r should be null or have sufficient space. Calling with r == NULL will
 * only calculate the result_len; however, a buffer with space for three
 * characters will always be big enough. */
static void
decompose_hangul (gunichar s, gunichar * r, gsize * result_len)
{
  gint SIndex = s - SBase;

  /* not a hangul syllable */
  if (SIndex < 0 || SIndex >= SCount)
    {
      if (r)
	r[0] = s;
      *result_len = 1;
    }
  else
    {
      gunichar L = LBase + SIndex / NCount;
      gunichar V = VBase + (SIndex % NCount) / TCount;
      gunichar T = TBase + SIndex % TCount;

      if (r)
	{
	  r[0] = L;
	  r[1] = V;
	}

      if (T != TBase)
	{
	  if (r)
	    r[2] = T;
	  *result_len = 3;
	}
      else
	*result_len = 2;
    }
}

/* returns a pointer to a null-terminated UTF-8 string */
static const gchar *
find_decomposition (gunichar ch, gboolean compat)
{
  int start = 0;
  int end = G_N_ELEMENTS (decomp_table);

  if (ch >= decomp_table[start].ch && ch <= decomp_table[end - 1].ch)
    {
      while (TRUE)
	{
	  int half = (start + end) / 2;
	  if (ch == decomp_table[half].ch)
	    {
	      int offset;

	      if (compat)
		{
		  offset = decomp_table[half].compat_offset;
		  if (offset == G_UNICODE_NOT_PRESENT_OFFSET)
		    offset = decomp_table[half].canon_offset;
		}
	      else
		{
		  offset = decomp_table[half].canon_offset;
		  if (offset == G_UNICODE_NOT_PRESENT_OFFSET)
		    return NULL;
		}

	      return &(decomp_expansion_string[offset]);
	    }
	  else if (half == start)
	    break;
	  else if (ch > decomp_table[half].ch)
	    start = half;
	  else
	    end = half;
	}
    }

  return NULL;
}

/* L,V => LV and LV,T => LVT  */
static gboolean
combine_hangul (gunichar a, gunichar b, gunichar * result)
{
  gint LIndex = a - LBase;
  gint SIndex = a - SBase;

  gint VIndex = b - VBase;
  gint TIndex = b - TBase;

  if (0 <= LIndex && LIndex < LCount && 0 <= VIndex && VIndex < VCount)
    {
      *result = SBase + (LIndex * VCount + VIndex) * TCount;
      return TRUE;
    }
  else if (0 <= SIndex && SIndex < SCount && (SIndex % TCount) == 0
	   && 0 <= TIndex && TIndex <= TCount)
    {
      *result = a + TIndex;
      return TRUE;
    }

  return FALSE;
}

#define CI(Page, Char) \
  ((compose_table[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (compose_table[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (compose_data[compose_table[Page]][Char]))

#define COMPOSE_INDEX(Char) \
     ((((Char) >> 8) > (COMPOSE_TABLE_LAST)) ? 0 : CI((Char) >> 8, (Char) & 0xff))

static gboolean
combine (gunichar a, gunichar b, gunichar * result)
{
  gushort index_a, index_b;

  if (combine_hangul (a, b, result))
    return TRUE;

  index_a = COMPOSE_INDEX (a);

  if (index_a >= COMPOSE_FIRST_SINGLE_START && index_a < COMPOSE_SECOND_START)
    {
      if (b == compose_first_single[index_a - COMPOSE_FIRST_SINGLE_START][0])
	{
	  *result =
	    compose_first_single[index_a - COMPOSE_FIRST_SINGLE_START][1];
	  return TRUE;
	}
      else
	return FALSE;
    }

  index_b = COMPOSE_INDEX (b);

  if (index_b >= COMPOSE_SECOND_SINGLE_START)
    {
      if (a ==
	  compose_second_single[index_b - COMPOSE_SECOND_SINGLE_START][0])
	{
	  *result =
	    compose_second_single[index_b - COMPOSE_SECOND_SINGLE_START][1];
	  return TRUE;
	}
      else
	return FALSE;
    }

  if (index_a >= COMPOSE_FIRST_START && index_a < COMPOSE_FIRST_SINGLE_START
      && index_b >= COMPOSE_SECOND_START
      && index_b < COMPOSE_SECOND_SINGLE_START)
    {
      gunichar res =
	compose_array[index_a - COMPOSE_FIRST_START][index_b -
						     COMPOSE_SECOND_START];

      if (res)
	{
	  *result = res;
	  return TRUE;
	}
    }

  return FALSE;
}

static gunichar *
_g_utf8_normalize_wc (const gchar * str, gssize max_len, GNormalizeMode mode)
{
  gsize n_wc;
  gunichar *wc_buffer;
  const char *p;
  gsize last_start;
  gboolean do_compat = (mode == G_NORMALIZE_NFKC || mode == G_NORMALIZE_NFKD);
  gboolean do_compose = (mode == G_NORMALIZE_NFC || mode == G_NORMALIZE_NFKC);

  n_wc = 0;
  p = str;
  while ((max_len < 0 || p < str + max_len) && *p)
    {
      const gchar *decomp;
      gunichar wc = g_utf8_get_char (p);

      if (wc >= 0xac00 && wc <= 0xd7af)
	{
	  gsize result_len;
	  decompose_hangul (wc, NULL, &result_len);
	  n_wc += result_len;
	}
      else
	{
	  decomp = find_decomposition (wc, do_compat);

	  if (decomp)
	    n_wc += g_utf8_strlen (decomp, -1);
	  else
	    n_wc++;
	}

      p = g_utf8_next_char (p);
    }

  wc_buffer = g_new (gunichar, n_wc + 1);
  if (!wc_buffer)
    return NULL;

  last_start = 0;
  n_wc = 0;
  p = str;
  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar wc = g_utf8_get_char (p);
      const gchar *decomp;
      int cc;
      gsize old_n_wc = n_wc;

      if (wc >= 0xac00 && wc <= 0xd7af)
	{
	  gsize result_len;
	  decompose_hangul (wc, wc_buffer + n_wc, &result_len);
	  n_wc += result_len;
	}
      else
	{
	  decomp = find_decomposition (wc, do_compat);

	  if (decomp)
	    {
	      const char *pd;
	      for (pd = decomp; *pd != '\0'; pd = g_utf8_next_char (pd))
		wc_buffer[n_wc++] = g_utf8_get_char (pd);
	    }
	  else
	    wc_buffer[n_wc++] = wc;
	}

      if (n_wc > 0)
	{
	  cc = COMBINING_CLASS (wc_buffer[old_n_wc]);

	  if (cc == 0)
	    {
	      g_unicode_canonical_ordering (wc_buffer + last_start,
					    n_wc - last_start);
	      last_start = old_n_wc;
	    }
	}

      p = g_utf8_next_char (p);
    }

  if (n_wc > 0)
    {
      g_unicode_canonical_ordering (wc_buffer + last_start,
				    n_wc - last_start);
      last_start = n_wc;
    }

  wc_buffer[n_wc] = 0;

  /* All decomposed and reordered */

  if (do_compose && n_wc > 0)
    {
      gsize i, j;
      int last_cc = 0;
      last_start = 0;

      for (i = 0; i < n_wc; i++)
	{
	  int cc = COMBINING_CLASS (wc_buffer[i]);

	  if (i > 0 &&
	      (last_cc == 0 || last_cc != cc) &&
	      combine (wc_buffer[last_start], wc_buffer[i],
		       &wc_buffer[last_start]))
	    {
	      for (j = i + 1; j < n_wc; j++)
		wc_buffer[j - 1] = wc_buffer[j];
	      n_wc--;
	      i--;

	      if (i == last_start)
		last_cc = 0;
	      else
		last_cc = COMBINING_CLASS (wc_buffer[i - 1]);

	      continue;
	    }

	  if (cc == 0)
	    last_start = i;

	  last_cc = cc;
	}
    }

  wc_buffer[n_wc] = 0;

  return wc_buffer;
}

/*
 * g_utf8_normalize:
 * @str: a UTF-8 encoded string.
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * @mode: the type of normalization to perform.
 *
 * Converts a string into canonical form, standardizing
 * such issues as whether a character with an accent
 * is represented as a base character and combining
 * accent or as a single precomposed character. You
 * should generally call g_utf8_normalize() before
 * comparing two Unicode strings.
 *
 * The normalization mode %G_NORMALIZE_DEFAULT only
 * standardizes differences that do not affect the
 * text content, such as the above-mentioned accent
 * representation. %G_NORMALIZE_ALL also standardizes
 * the "compatibility" characters in Unicode, such
 * as SUPERSCRIPT THREE to the standard forms
 * (in this case DIGIT THREE). Formatting information
 * may be lost but for most text operations such
 * characters should be considered the same.
 * For example, g_utf8_collate() normalizes
 * with %G_NORMALIZE_ALL as its first step.
 *
 * %G_NORMALIZE_DEFAULT_COMPOSE and %G_NORMALIZE_ALL_COMPOSE
 * are like %G_NORMALIZE_DEFAULT and %G_NORMALIZE_ALL,
 * but returned a result with composed forms rather
 * than a maximally decomposed form. This is often
 * useful if you intend to convert the string to
 * a legacy encoding or pass it to a system with
 * less capable Unicode handling.
 *
 * Return value: a newly allocated string, that is the
 *   normalized form of @str.
 **/
gchar *
g_utf8_normalize (const gchar * str, gssize len, GNormalizeMode mode)
{
  gunichar *result_wc = _g_utf8_normalize_wc (str, len, mode);
  gchar *result;

  result = g_ucs4_to_utf8 (result_wc, -1, NULL, NULL, NULL);
  g_free (result_wc);

  return result;
}


/*
 * Code from guniprop.c
 */

#define ATTR_TABLE(Page) (((Page) <= G_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == G_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : G_UNICODE_UNASSIGNED))


/**
 * g_unichar_toupper:
 * @c: a Unicode character
 * 
 * Converts a character to uppercase.
 * 
 * Return value: the result of converting @c to uppercase.
 *               If @c is not an lowercase or titlecase character,
 *               or has no upper case equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_toupper (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_LOWERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
	  return g_utf8_get_char (p);
	}
      else
	return val ? val : c;
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1];
	}
    }
  return c;
}

/**
 * g_unichar_tolower:
 * @c: a Unicode character.
 * 
 * Converts a character to lower case.
 * 
 * Return value: the result of converting @c to lower case.
 *               If @c is not an upperlower or titlecase character,
 *               or has no lowercase equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_tolower (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_UPPERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
	  return g_utf8_get_char (p);
	}
      else
	return val ? val : c;
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}


/**
 * g_utf8_casefold:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts a string into a form that is independent of case. The
 * result will not correspond to any particular case, but can be
 * compared for equality or ordered with the results of calling
 * g_utf8_casefold() on other strings.
 * 
 * Note that calling g_utf8_casefold() followed by g_utf8_collate() is
 * only an approximation to the correct linguistic case insensitive
 * ordering, though it is a fairly good one. Getting this exactly
 * right would require a more sophisticated collation function that
 * takes case sensitivity into account. GLib does not currently
 * provide such a function.
 * 
 * Return value: a newly allocated string, that is a
 *   case independent form of @str.
 **/
gchar *
g_utf8_casefold (const gchar *str,
		 gssize       len)
{
  dstring_t *result;
  const char *p;

  g_return_val_if_fail (str != NULL, NULL);

  result = dstring_new (NULL);
  p = str;
  while ((len < 0 || p < str + len) && *p)
    {
      gunichar ch = g_utf8_get_char (p);

      int start = 0;
      int end = G_N_ELEMENTS (casefold_table);

      if (ch >= casefold_table[start].ch &&
          ch <= casefold_table[end - 1].ch)
	{
	  while (TRUE)
	    {
	      int half = (start + end) / 2;
	      if (ch == casefold_table[half].ch)
		{
		  dstring_append (result, casefold_table[half].data);
		  goto next;
		}
	      else if (half == start)
		break;
	      else if (ch > casefold_table[half].ch)
		start = half;
	      else
		end = half;
	    }
	}

      char utf8_buf[6];
      int utf8_len = g_unichar_to_utf8 (g_unichar_tolower (ch), utf8_buf);
      dstring_append_len (result, utf8_buf, utf8_len);

    next:
      p = g_utf8_next_char (p);
    }

  return dstring_free (result, 0); 
}




/* gunicollate.c */

/**
 * g_utf8_collate:
 * @str1: a UTF-8 encoded string
 * @str2: a UTF-8 encoded string
 * 
 * Compares two strings for ordering using the linguistically
 * correct rules for the current locale. When sorting a large
 * number of strings, it will be significantly faster to
 * obtain collation keys with g_utf8_collate_key() and 
 * compare the keys with strcmp() when 
 * sorting instead of sorting the original strings.
 * 
 * Return value: &lt; 0 if @str1 compares before @str2, 
 *   0 if they compare equal, &gt; 0 if @str1 compares after @str2.
 **/
gint
g_utf8_collate (const gchar *str1,
		const gchar *str2)
{
  gint result;
  
#ifdef __STDC_ISO_10646__

  gunichar *str1_norm;
  gunichar *str2_norm;

  g_return_val_if_fail (str1 != NULL, 0);
  g_return_val_if_fail (str2 != NULL, 0);

  str1_norm = _g_utf8_normalize_wc (str1, -1, G_NORMALIZE_ALL_COMPOSE);
  str2_norm = _g_utf8_normalize_wc (str2, -1, G_NORMALIZE_ALL_COMPOSE);

  result = wcscoll ((wchar_t *)str1_norm, (wchar_t *)str2_norm);

  g_free (str1_norm);
  g_free (str2_norm);

#else /* !__STDC_ISO_10646__ */

  const gchar *charset;
  gchar *str1_norm;
  gchar *str2_norm;

  g_return_val_if_fail (str1 != NULL, 0);
  g_return_val_if_fail (str2 != NULL, 0);

  str1_norm = g_utf8_normalize (str1, -1, G_NORMALIZE_ALL_COMPOSE);
  str2_norm = g_utf8_normalize (str2, -1, G_NORMALIZE_ALL_COMPOSE);

  if (g_get_charset (&charset))
    {
      result = strcoll (str1_norm, str2_norm);
    }
  else
    {
      gchar *str1_locale = g_convert (str1_norm, -1, charset, "UTF-8", NULL, NULL, NULL);
      gchar *str2_locale = g_convert (str2_norm, -1, charset, "UTF-8", NULL, NULL, NULL);

      if (str1_locale && str2_locale)
	result =  strcoll (str1_locale, str2_locale);
      else if (str1_locale)
	result = -1;
      else if (str2_locale)
	result = 1;
      else
	result = strcmp (str1_norm, str2_norm);

      g_free (str1_locale);
      g_free (str2_locale);
    }

  g_free (str1_norm);
  g_free (str2_norm);

#endif /* __STDC_ISO_10646__ */

  return result;
}

#ifdef TEST

#include <stdio.h>
#include <stdlib.h>

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

int main(void)
{
    const char *utf8_decomposed = "a\xcc\x8a""a\xcc\x88""o\xcc\x88";
    const char *utf8_composed = "\xc3\xa5\xc3\xa4\xc3\xb6";

    char *result = g_utf8_normalize(utf8_decomposed, -1, G_NORMALIZE_NFKC);
    fail_unless(result);
    fail_unless(strcmp(result, utf8_composed) == 0);
    free(result);

    return 0;
}

#endif

