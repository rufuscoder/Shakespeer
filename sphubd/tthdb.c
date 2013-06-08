/*
 * Copyright (c) 2007 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "tthdb.h"
#include "base64.h"
#include "log.h"
#include "globals.h"
#include "compat.h"
#include "xstr.h"

int tth_entry_cmp(struct tth_entry *a, struct tth_entry *b)
{
	return strcmp(a->tth, b->tth);
}

int tth_inode_cmp(struct tth_inode *a, struct tth_inode *b)
{
	/* This used to be:
	 *  return b->inode - a->inode;
	 * That doesn't work! The return type is int, so the return value
	 * gets truncated if both a and b is large enough and only differs
	 * in the higher 32 bits.
	 */

	if(a->inode < b->inode)
		return -1;
	if(a->inode > b->inode)
		return 1;
	return 0;
}

RB_GENERATE(tth_entries_head, tth_entry, link, tth_entry_cmp);
RB_GENERATE(tth_inodes_head, tth_inode, link, tth_inode_cmp);

static void tth_parse_add_tth(struct tth_store *store,
	char *buf, size_t len, off_t offset)
{
	buf += 3; /* skip past "+T:" */
	len -= 3;

	/* syntax of buf is 'tth:leafdata_base64' */

	if(len <= 40 || buf[39] != ':')
	{
		WARNING("failed to load tth on line %u", store->line_number);
	}
	else
	{
		/* Don't read the leafdata into memory. Instead we
		 * store the offset for this entry in the file, so
		 * we easily can retrieve it when needed.
		 */

		buf[39] = 0;
		tth_store_add_entry(store, buf, NULL, offset);
	}
}

static void tth_parse_add_inode(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "+I:" */

	/* syntax of buf is 'inode:mtime:tth' */
	/* numeric values are stored in hex */

	uint64_t inode;
	unsigned long mtime;
	char tth[40];

	int rc = sscanf(buf, "%"PRIX64":%lX:%s", &inode, &mtime, tth);
	if(rc != 3 || inode == 0 || mtime == 0 || tth[39] != 0)
		WARNING("failed to load inode on line %u", store->line_number);
	else
		tth_store_add_inode(store, inode, mtime, tth);
}

static void tth_parse_remove_tth(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "-T:" */

	tth_store_remove(store, buf);
}

static void tth_parse_remove_inode(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "-I:" */

	char *endptr = NULL;
	uint64_t inode = strtoull(buf, &endptr, 16);
	if(endptr == NULL || *endptr != 0 || inode == 0)
	{
		WARNING("failed to load TTH remove on line %u",
			store->line_number);
	}
	else
		tth_store_remove_inode(store, inode);
}

static void tth_parse(struct tth_store *store)
{
	int ntth = 0;
	int ninode = 0;

	store->loading = true;
	store->need_normalize = false;

	char *buf, *lbuf = NULL;
	size_t len;
	off_t offset = 0;
	while((buf = fgetln(store->fp, &len)) != NULL)
	{
		store->line_number++;

		if(buf[len - 1] == '\n')
			buf[len - 1] = 0;
		else
		{
			/* EOF without EOL, copy and add the NUL */
			lbuf = malloc(len + 1);
			assert(lbuf);
			memcpy(lbuf, buf, len);
			lbuf[len] = 0;
			buf = lbuf;
		}

		if(len < 3 || buf[2] != ':')
			continue;

		if(strncmp(buf, "+T:", 3) == 0)
		{
			tth_parse_add_tth(store, buf, len, offset);
			ntth++;
		}
		else if(strncmp(buf, "+I:", 3) == 0)
		{
			tth_parse_add_inode(store, buf, len);
			ninode++;
		}
		else if(strncmp(buf, "-T:", 3) == 0)
		{
			tth_parse_remove_tth(store, buf, len);
			ntth--;
			store->need_normalize = true;
		}
		else if(strncmp(buf, "-I:", 3) == 0)
		{
			tth_parse_remove_inode(store, buf, len);
			ninode--;
			store->need_normalize = true;
		}
		else
		{
			INFO("unknown type %02X%02X, skipping line %u",
				(unsigned char)buf[0], (unsigned char)buf[1],
				store->line_number);
			store->need_normalize = true;
		}

		offset = ftell(store->fp);
	}
	free(lbuf);

	INFO("done loading TTH store (%i TTHs, %i inodes)", ntth, ninode);

	store->loading = false;
}

static struct tth_store *tth_load(const char *filename)
{
	FILE *fp = fopen(filename, "a+");
	return_val_if_fail(fp, NULL);

	struct tth_store *store = calloc(1, sizeof(struct tth_store));

	store->filename = strdup(filename);
	store->fp = fp;
	RB_INIT(&store->entries);
	RB_INIT(&store->inodes);

	rewind(store->fp);
	INFO("loading TTH store from [%s]", filename);
	tth_parse(store);

	return store;
}

void tth_store_init(void)
{
	return_if_fail(global_working_directory);
	return_if_fail(global_tth_store == NULL);

	char *tth_store_filename;
	int num_returned_bytes = asprintf(&tth_store_filename, "%s/tth2.db", global_working_directory);
	if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
	global_tth_store = tth_load(tth_store_filename);
	free(tth_store_filename);
}

static void tth_close_database(struct tth_store *store)
{
	INFO("closing TTH database");
	return_if_fail(store);

	fclose(store->fp);
	free(store->filename);
	free(store);
}

void tth_store_close(void)
{
	tth_close_database(global_tth_store);
}

void tth_store_add_inode(struct tth_store *store,
	 uint64_t inode, time_t mtime, const char *tth)
{
	return_if_fail(store);
	return_if_fail(tth);

	struct tth_inode *ti;

	ti = tth_store_lookup_inode(store, inode);
	if(ti == NULL)
	{
		ti = calloc(1, sizeof(struct tth_inode));
		ti->inode = inode;
		RB_INSERT(tth_inodes_head, &store->inodes, ti);
	}

	if(ti->mtime != mtime || strcmp(ti->tth, tth) != 0)
	{
		ti->mtime = mtime;
		strlcpy(ti->tth, tth, sizeof(ti->tth));

		if(!store->loading)
		{
			fprintf(store->fp, "+I:%"PRIX64":%lX:%s\n",
				inode, (unsigned long)mtime, tth);
		}
	}
}

void tth_store_add_entry(struct tth_store *store,
	const char *tth, const char *leafdata_base64,
	off_t leafdata_offset)
{
	return_if_fail(store);

	struct tth_entry *te;

	te = tth_store_lookup(store, tth);

	if(te == NULL)
	{
		te = calloc(1, sizeof(struct tth_entry));
		strlcpy(te->tth, tth, sizeof(te->tth));
		te->leafdata_offset = leafdata_offset;

		RB_INSERT(tth_entries_head, &store->entries, te);
	}

	if(!store->loading)
	{
		return_if_fail(leafdata_base64);

		int len = fprintf(store->fp, "+T:%s:%s\n",
			tth, leafdata_base64);

		/* Call ftell() _after_ we have written the +T line, because
		 * the file is opened in append mode and we might have
		 * seek'd earlier to read leafdata. This way we are certain
		 * we get the correct offset.
		 */
		if(len > 0)
			te->leafdata_offset = ftell(store->fp) - len;
	}
}

/* load the leafdata for the given TTH from the backend store */
int tth_store_load_leafdata(struct tth_store *store, struct tth_entry *entry)
{
	char *lbuf = NULL;
	return_val_if_fail(store, -1);
	return_val_if_fail(entry, -1);

	if(entry->leafdata)
		return 0; /* already loaded */

	INFO("loading leafdata for tth [%s] at offset %llu",
		entry->tth, entry->leafdata_offset);

	/* seek to the entry->leafdata_offset position in the backend store */
	int rc = fseek(store->fp, entry->leafdata_offset, SEEK_SET);
	if(rc == -1)
	{
		WARNING("seek to %llu failed", entry->leafdata_offset);
		goto failed;
	}

	size_t len;
	char *buf = fgetln(store->fp, &len);
	if(buf == NULL)
	{
		WARNING("failed to read line @offset %llu",
			entry->leafdata_offset);
		goto failed;
	}

	if(buf[len - 1] == '\n')
		buf[len - 1] = 0;
	else
	{
		/* EOF without EOL, copy and add the NUL */
		lbuf = malloc(len + 1);
		assert(lbuf);
		memcpy(lbuf, buf, len);
		lbuf[len] = 0;
		buf = lbuf;
	}

	if(len < 3 || strncmp(buf, "+T:", 3) != 0)
	{
		WARNING("invalid start tag: [%s]", buf);
		goto failed;
	}

	buf += 3;
	len -= 3;

	if(len <= 40 || buf[39] != ':' || strncmp(buf, entry->tth, 39) != 0)
	{
		WARNING("offset points to wrong tth: [%s]", buf);
		goto failed;
	}

	buf += 40;
	len -= 40;

	unsigned base64_len = strcspn(buf, ":");
	if(buf[base64_len] == ':')
	{
		WARNING("excessive fields in tth database, truncating");
		buf[base64_len] = 0;
	}

	entry->leafdata = malloc(base64_len);
	assert(entry->leafdata);

	rc = base64_pton(buf, (unsigned char *)entry->leafdata, base64_len);
	if(rc <= 0)
	{
		WARNING("invalid base64 encoded leafdata");
		free(entry->leafdata);
		entry->leafdata = NULL;
		goto failed;
	}

	entry->leafdata_len = rc;

	free(lbuf);
	return 0;

failed:
	WARNING("failed to load leafdata for tth [%s]: %s",
		entry->tth, strerror(errno));

	free(lbuf);

	/* FIXME: should we re-load the tth store ? */
	return -1;
}

struct tth_entry *tth_store_lookup(struct tth_store *store, const char *tth)
{
	return_val_if_fail(store, NULL);

	struct tth_entry find;
	strlcpy(find.tth, tth, sizeof(find.tth));
	return RB_FIND(tth_entries_head, &store->entries, &find);
}

static void tth_entry_free(struct tth_entry *entry)
{
	if(entry)
	{
		free(entry->leafdata);
		free(entry);
	}
}

void tth_store_remove(struct tth_store *store, const char *tth)
{
	return_if_fail(store);

	struct tth_entry *entry = tth_store_lookup(store, tth);
	if(entry)
	{
		RB_REMOVE(tth_entries_head, &store->entries, entry);

		if(!store->loading)
		{
			fprintf(store->fp, "-T:%s\n", tth);
		}

		tth_entry_free(entry);
	}
}

struct tth_entry *tth_store_lookup_by_inode(struct tth_store *store,
	uint64_t inode)
{
	struct tth_inode *ti = tth_store_lookup_inode(store, inode);
	if(ti)
		return tth_store_lookup(store, ti->tth);
	return NULL;
}

static void tth_inode_free(struct tth_inode *ti)
{
	if(ti)
	{
		free(ti);
	}
}

void tth_store_remove_inode(struct tth_store *store, uint64_t inode)
{
	return_if_fail(store);

	struct tth_inode *ti = tth_store_lookup_inode(store, inode);
	if(ti)
	{
		RB_REMOVE(tth_inodes_head, &store->inodes, ti);

		if(!store->loading)
		{
			fprintf(store->fp, "-I:%"PRIX64"\n", inode);
		}

		tth_inode_free(ti);
	}
}

struct tth_inode *tth_store_lookup_inode(struct tth_store *store,
	uint64_t inode)
{
	return_val_if_fail(store, NULL);

	struct tth_inode find;
	find.inode = inode;
	return RB_FIND(tth_inodes_head, &store->inodes, &find);
}

void tth_store_set_active_inode(struct tth_store *store,
	const char *tth, uint64_t inode)
{
	return_if_fail(store);
	return_if_fail(tth);

	struct tth_entry *te = tth_store_lookup(store, tth);
	return_if_fail(te);

	/* switch active inode for this TTH */
	te->active_inode = inode;
}

#ifdef TEST

#include "unit_test.h"

#include "unit_test.h"

int main(void)
{
	sp_log_set_level("debug");
	global_working_directory = "/tmp/sp-tthdb-test.d";
	system("/bin/rm -rf /tmp/sp-tthdb-test.d");
	system("mkdir /tmp/sp-tthdb-test.d");

	const char *data = "+T:7LSZ6K2ZFQJBSEIRWM72N7VW2IULICCDW5ZUMJI:c8shGrB71Qbz5+2QO6og2deqPe+zWs48121TgHwoR1QpQUPut9qBY4csT9rbH68pefxFXkeISTwa1Vx5dnk09zpjnEO4oIYuDZZaXFMwR6RkBGsXLQ5sdsq0HADwZdyrGnd7SRYtOds5gcxEhoi1gOwUruktO6h5VtzR6Wc7JvYm0KmZZX6CgrcrPY/PN9BQGTJw5ADK5rdWRjMedZ/BHHq/8AzbC5a3sq7VvNikAPfqsyMdu5kXqCSIdKy4KXKexrnCKW2gP8VVclaniCnzHC74aEukOWIX7URTPgKBrlRBvXjtgwVeMj/ZZOG3btleUQCG6uQ1vRfyoRvNm+FsBZp1Zq43CAiyo6HhaSDErk1um7EqdxmM/u7BoOzqtkWby3wmvW87Wl0UJ/JejIXJNT+cNmIuL2q+ptkk4OcdLX0MkK6Frgss+LWUSEA4olOIv/uDBnF+mJbUwRYn9MrUWkPs7pJcjlJ0PJByaRFmoveOM3L3z4kW2tzWyOu1mD18kM9Rlf3Y+Ku5lJTB/e257F0lGI2k0rQmBDOLEskc4H3JMbqoWNjNipn/2xYZRFTmJKC7UTimyHfyICXLyedXhVXJYUNDaApRko0/MjKnCxPwVasA1O+iEbQnmUQHRUisY1w0RzWtI+f6Y+1/pJI7qd40bgGC7Lcq7dGfjEoX+NK7Jh1neWixu9dSsyFps36wb9n4uH0KK9zJ5YIAniLTVo5mu5YVsZ0yNM1BTF6IoAPNrZgSRsIkwHmLukijNXWPfXqBbSiHW8DRtIL/6YYxOi1UyDQ7zZY9I4PJr9YrzVWoprHpPVm39FHnlUXHYulhML1qmit4efC1NmBGbWPIV5xPW9OhUlyZpZeWvuW8W52j61VrGnr4BWK+DU6cgUA7T5l61MgMSbpbz4Q/5GpGH50LyRXwCkVkXtdcCt2b7MICOX7kr6vYk84d4rcO2k1AJ1U7PIj/UqIPNg9jZvQZgb1gSlVKmPOvIfr3uHMeGnWcpDIPNDmffnjyjjHRX94CYUf7f1j7BnMXVtiFAbcqfP8E74mJ84icKZSy2AOv88BmB/9ndPXB1fFcEP6wVsH95EivgSVZ9THv69WLK+1jzkJQ+vhZSVNz3igWLTgelznYmeBDma40qj+i+AveianegrO+5QidKeAVwYNpNXB47LPEhv2XtTLzHsrSyvUsKX+PRguuQdizcQLsdOB0FzWYqMoNoNhWoqR015FEsqrcQfE5ehxP8REhj+cG4PeWMnq/CE+QGhSh+YWfas5Da8VEEKRCvo/qBQvlqObORACe2iY5Voo4m5bjf4L1Kb4VksnTXdGCxU5aomXGubxcfQ1dqIIufANM7jGiZpn07eE13sXJMkP5jv9ruajpEIFGxrIsCHebYknl78SKLlguIjVipnLzBB39Np/pHx5BKdCuMu/JAdeLNP14EITh7aQQDXTJ1Yf4/0bu2GjGvWn1Q1pFKfsQ870VkZECF2JbemjOqDjvKO1QqHofEq7tmICdKWV3h7rr5FEDVJ0Q3B85eXVb0FDd9WHJEgWW0+9kob38JASiieFayf6VnWmAHM2lYv2XiDuCjGfY76hcEYp/5W1hGebxuXd9ywa3o5n69Jmv5KZaEkE/e9+cJGnoAjnKmzDjVy7bjD6q22YBw3H2dVgaVsXP3SH4+YlL6hbrkTh0QAR8PYkVrApZXCOD4Y9HsRnxvwtIWUwcgJLcWjgilBYW/bSLq+tjYTTejkdWKgoJK6Un3Jc8yzllsidvVZfAuHVxUrBvL7wFaXQZImmYP6PqGN60/t21Oo2ejUK1rAP40rTepKiT/IICIeGa5Nf39IuxKnnLn/5zICa84VuTzp3/jikJHt+FJEM0zZF+XDyDca1rn68nGSEswifrePekDTfhPIl6tnLhef9wIUCNjrKhjzPPBxELgCE5//3bXJ1pfJAoAJBfko68hUppj8NKPW+1hOZo0SQCaYp2wfiM5NZiAxBmphZA1JaEKr1Ew1JWcgyB1SrD9wGZPIWv9PTgDpDWsQaQlWNkBbArITmXSIFl7VPrEvelPiDNIYWS6kr0XzXRcS4FdqB3tV8WbEpGMoA72jOpU0fZKJ0uMRbpjysWU3F3ipic1JVXaEX5vcufJz5NlgBfKH6bDoEQq7peSINc9fHdLIIsTVM5cWyJa1PUpns3yvTX0ydQuIkg1f4NVOtXVqM7zXQyARitGRQmr683PeyRH+Tdy92wpn4qS/G4PuiK5Y2lgMUpBnqFqEKZT4eGJB9kvnxHHAz9bMC24AHqU4HVWU4ZNtwNEhJ1IZWwnmRVvLEOaHazY3zVhUiHL9JY22Dhq1uGQyBSBC7aOa/ZNWEjHw9DqU0ycCHj23jlzkq/v6yG8Z5NKGRe4YSyMVdUfVcAHyNhp9CkqM47wYZ6N1qp1Q3CFF6xZCaMwf4eaM77hDdRaDc0J2TmGCQXg+0S7g0spjIvB5cpyizCFmHTFEdW30xfIt9OXyDyNy7Q5VXHPM1H+JuWvLbMYBcXtnlYjo/E+8MZRn1Q2wJY8OCX6vdJNglsJep35aZXcmajE2AdZjX3VCrPofQe3Sq82sEXtHYlgkuC78uTH78Ayo5vsZ/Dnpcmw92GvrBB8Cue37IVxlQpxM5yEGgACcJm/1dOoIehBZTA+yF60Y2P9DRUkSpcYuF8GEFDstCyFbcimBL2zGlN5UJWq5vA/KrQs+5qxUaqV+SSn9p/1h5u9QZPh4JjibHVc69o5FmU9dKKXnwv4Pdgri7SOspmIHfAM6K4YDLOfj337YOnkWEifNVyr/N6BgSMTD+cESpyEJPh0HN8+93RyHGWjhEtn/D3NC1U/6ijeFsorxut+kkT7oU1wTgnf5oClSe0dUymaHi6lYO/ySn2LrE3y8msfefTCHSDdOo4wXX4fYMb5MtZ9nymy7ZmS6O74Yg4WIbALBz+lOj+8PJJbDsTcTPcowZoqhxM+o3skzoUVMNj722TY7FxIeQQ+T52Pz333r1F564i4AzW41+4JAa2zCN89EfjwZdqCassb74rGZanoQjOdWNcuG0wr3d/ljojT1Yz74YwpDonR9IPBLlmeDppYin5It6NiBtidutKzN7FmgP9BaTQqf8s8PbKTttkD+9+irfr\n"
	"+I:61529D00001A7B:404E3394:7LSZ6K2ZFQJBSEIRWM72N7VW2IULICCDW5ZUMJI\n";
	size_t len = strlen(data);

	FILE *fp = fopen("/tmp/sp-tthdb-test.d/tth2.db", "w");
	fail_unless(fp);
	fail_unless(fwrite(data, 1, len, fp) == len);
	fail_unless(fclose(fp) == 0);

	tth_store_init();
	fail_unless(global_tth_store);

	struct tth_entry *te = tth_store_lookup(global_tth_store,
		"7LSZ6K2ZFQJBSEIRWM72N7VW2IULICCDW5ZUMJI");
	fail_unless(te);
	fail_unless(te->active_inode = 0x61529D00001A7BULL);

	struct tth_inode *ti = tth_store_lookup_inode(global_tth_store, 0x61529D00001A7BULL);
	fail_unless(ti);
	fail_unless(strcmp(ti->tth, "7LSZ6K2ZFQJBSEIRWM72N7VW2IULICCDW5ZUMJI") == 0);
	fail_unless(ti->mtime = 0x404E3394);

	fail_unless(te->leafdata_offset == 0);
	fail_unless(te->leafdata == NULL);
	int rc = tth_store_load_leafdata(global_tth_store, te);
	fail_unless(rc == 0);
	fail_unless(te->leafdata != NULL);
	fail_unless(te->leafdata_len > 0);

	tth_store_close();

	system("/bin/rm -rf /tmp/sp-tthdb-test.d");

	return 0;
}

#endif

