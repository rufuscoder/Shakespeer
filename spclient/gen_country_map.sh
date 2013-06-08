#!/bin/sh

# Fetches the list of 2-letter country codes from ISO's webpage and
# formats them suitable to insert in a linked list.

cat <<HEADER
/* This is a generated file by gen_country_map.sh. Don't edit, edit the source instead. */
#include "sys_queue.h"
#include <string.h>
#include <stdlib.h>

struct country
{
    LIST_ENTRY(country) link;
    char *code;
    char *country;
};

static int initialized = 0;

static LIST_HEAD(, country) country_list_head = LIST_HEAD_INITIALIZER();

static void add_country(const char *code, const char *country)
{
    struct country *c = calloc(1, sizeof(struct country));
    c->code = strdup(code);
    c->country = strdup(country);

    LIST_INSERT_HEAD(&country_list_head, c, link);
}

static void country_map_create(void)
{
HEADER

URL=http://www.iso.ch/iso/en/prods-services/iso3166ma/02iso-3166-code-lists/list-en1-semic.txt

set -e

F="list-en1-semic.txt"
if ! test -f $F; then
  curl -s "$URL" -o "$F" || wget "$URL" || fetch "$URL" || \
    ftp $URL || lynx -source "$URL" > "$F"
fi

iconv -f latin1 -t utf-8 $F | tr -d '\r' | awk -F \; '
function capitalize(input, result, words, n, i, w)
{
  result = ""
  n = split(input, words, " ")
  for(i = 1; i <= n; i++)
  {
    w = words[i]
    if(w == "OF" || w == "AND" || w == "THE")
      w = tolower(w)
    else
      w = substr(w, 1, 1) tolower(substr(w, 2))
    if(i > 1)
      result = result " "
    result = result w
  }
  return result
}
NF == 2 && $2 ~ /[A-Z][A-Z]/ {
    printf("    add_country(\"%s\", \"%s\");\n", tolower($2), capitalize($1));
}'

cat <<FOOTER
}

const char *country_map_lookup(const char *code)
{
    struct country *c;

    if(!initialized)
    {
        country_map_create();
        initialized = 1;
    }
    
    LIST_FOREACH(c, &country_list_head, link)
    {
        if(strcmp(c->code, code) == 0)
            return c->country;
    }

    return NULL;
}
FOOTER

