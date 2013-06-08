#ifndef _nfkc_h_
#define _nfkc_h_

#include <sys/types.h>
#include <stdint.h>

#ifdef __linux__
# include <stdint.h>
#endif

/* Hacks to make syncing with GLIB code easier. */
#define gboolean int
#define gchar char
#define guchar unsigned char
#define glong long
#define gint int
#define guint unsigned int
#define gushort unsigned short
#define gint16 int16_t
#define guint16 uint16_t
#define gunichar uint32_t
#define gsize size_t
#define gssize ssize_t
#define g_malloc malloc
#define g_free free
#define GError void
#define g_set_error(a,b,c,d) ((void) 0)
#define g_new(struct_type, n_structs)					\
  ((struct_type *) malloc ( sizeof (struct_type) * (n_structs)))
#define G_STMT_START	do
#define G_STMT_END	while (0)
#define g_return_val_if_fail(expr,val)	G_STMT_START{ if(!(expr)) return (val); }G_STMT_END
#define G_N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif
#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)

/* Code from GLIB gunicode.h starts here. */

typedef enum
{
  G_NORMALIZE_DEFAULT,
  G_NORMALIZE_NFD = G_NORMALIZE_DEFAULT,
  G_NORMALIZE_DEFAULT_COMPOSE,
  G_NORMALIZE_NFC = G_NORMALIZE_DEFAULT_COMPOSE,
  G_NORMALIZE_ALL,
  G_NORMALIZE_NFKD = G_NORMALIZE_ALL,
  G_NORMALIZE_ALL_COMPOSE,
  G_NORMALIZE_NFKC = G_NORMALIZE_ALL_COMPOSE
}
GNormalizeMode;

extern const gchar *const g_utf8_skip;

#define g_utf8_next_char(p) (char *)((p) + g_utf8_skip[*(guchar *)(p)])

gchar * g_utf8_find_prev_char (const char *str, const char *p);
gchar * g_utf8_find_next_char (const gchar *p, const gchar *end);
gboolean g_unichar_validate (gunichar ch);
glong g_utf8_strlen (const gchar * p, gssize max);
gunichar g_utf8_get_char (const gchar * p);
gunichar g_utf8_get_char_validated (const  gchar *p, gssize max_len);
int g_unichar_to_utf8 (gunichar c, gchar * outbuf);
gunichar * g_utf8_to_ucs4_fast (const gchar * str, glong len, glong * items_written);
gchar * g_ucs4_to_utf8 (const gunichar * str,
		glong len,
		glong * items_read, glong * items_written, GError ** error);
gboolean g_utf8_validate (const char   *str,
		 gssize        max_len,    
		 const gchar **end);
void g_unicode_canonical_ordering (gunichar * string, gsize len);
gchar * g_utf8_normalize (const gchar * str, gssize len, GNormalizeMode mode);
gchar * g_utf8_strncpy (gchar *dest, const gchar *src, gsize n);
glong g_utf8_pointer_to_offset (const gchar *str, const gchar *pos);
gchar * g_utf8_offset_to_pointer (const gchar *str, glong offset);
gchar * g_utf8_casefold (const gchar *str, gssize len);

gint g_utf8_collate (const gchar *str1, const gchar *str2);

#endif

