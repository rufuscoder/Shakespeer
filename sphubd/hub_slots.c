/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
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

#include "hub.h"
#include "log.h"
#include "extra_slots.h"

static int configured_slots = 2;
static int total_slots = 0;
static bool per_hub_flag = true;
static int used_slots = 0;

void hub_free_upload_slot(hub_t *hub, const char *nick, slot_state_t slot_state)
{
    switch(slot_state)
    {
        case SLOT_EXTRA:
            INFO("removing extra upload slot for nick %s", nick);
            extra_slots_grant(nick, -1);
            break;
        case SLOT_NORMAL:
	    used_slots--;

	    if(used_slots < 0)
	    {
		WARNING("INTERNAL ERROR: used_slots < 0");
		used_slots = 0;
	    }

	    INFO("freeing one upload slot for nick %s: %d used, %d free",
		    nick, used_slots, total_slots - used_slots);
            break;
        case SLOT_FREE:
        case SLOT_NONE:
        default:
            break;
    }
}

/* Returns 0 if a normal slot is available and was allocated. Returns 1 if a
 * free slot was granted (either for a filelist, small file or an extra granted
 * slot). Returns -1 if no slot was available.
 */
slot_state_t hub_request_upload_slot(hub_t *hub, const char *nick,
        const char *filename, uint64_t size)
{
    if(filename == NULL || is_filelist(filename) || size < 64*1024)
    {
        INFO("allowing free upload slot for file %s", filename);
        return SLOT_FREE;
    }

    if(extra_slots_get_for_user(nick) > 0)
    {
        INFO("allowing extra upload slot for nick %s", nick);
        return SLOT_EXTRA;
    }

    if(used_slots >= total_slots)
    {
	INFO("no free slots left");
	return SLOT_NONE;
    }

    used_slots++;
    INFO("allocating one upload slot for file %s: %d used, %d free",
	    filename, used_slots, total_slots - used_slots);

    return SLOT_NORMAL;
}

void hub_set_slots(int slots, bool per_hub)
{
    return_if_fail(slots > 0);

    if(per_hub)
    	total_slots = slots * (hub_count_normal() + hub_count_registered());
    else
    	total_slots = slots;
    INFO("setting slots = %d%s (%i totally)",
	slots, per_hub ? " per hub" : "", total_slots);
    configured_slots = slots;
    per_hub_flag = per_hub;
    hub_set_need_myinfo_update(true);
}

/* must be called if number of connected hubs changes */
void hub_update_slots(void)
{
    hub_set_slots(configured_slots, per_hub_flag);
}

/* Returns number of free slots currently available. */
int hub_slots_free(void)
{
    int d = total_slots - used_slots;
    /* we might have total - used < 0 if we've lowered the total slots */
    if(d < 0)
	d = 0;
    return d;
}

/* Returns total number of slots available. */
int hub_slots_total(void)
{
    return total_slots;
}

#ifdef TEST

#include "unit_test.h"
#include "globals.h"

void check_slots(hub_t *ahub)
{
    /* we haven't used any slots yet */
    fail_unless(hub_slots_free() == 2);

    /* request a normal slot */
    slot_state_t ss;
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 123456);
    fail_unless(ss == SLOT_NORMAL);

    /* make sure we've used one, free it and verify we're back */
    fail_unless(hub_slots_free() == 1);
    hub_free_upload_slot(ahub, "nicke", ss);
    fail_unless(hub_slots_free() == 2);

    /* use all available slots */
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 123456);
    fail_unless(ss == SLOT_NORMAL);
    ss = hub_request_upload_slot(ahub, "nicke2", "filename2", 234567);
    fail_unless(ss == SLOT_NORMAL);
    fail_unless(hub_slots_free() == 0);
    ss = hub_request_upload_slot(ahub, "nicke3", "filename3", 345678);
    fail_unless(ss == SLOT_NONE);
    ss = hub_request_upload_slot(ahub, "nicke3", "filename3", 345678);
    fail_unless(ss == SLOT_NONE);

    /* back to all free slots */
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NONE);
    fail_unless(hub_slots_free() == 1);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 2);

    /* make sure we handle a possible inconsistency */
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 2);

    /* check that we can get a free slot for small files */
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 63*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free() == 2);

    /* check that we can get a free slot for filelists */
    ss = hub_request_upload_slot(ahub, "nicke", "files.xml.bz2", 67*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free() == 2);
    ss = hub_request_upload_slot(ahub, "nicke", NULL, 67*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free() == 2);

    /* check integration with extra-slots module */
    fail_unless(extra_slots_grant("nicke", 1) == 0);
    fail_unless(hub_slots_free() == 2);
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 23423423);
    fail_unless(ss == SLOT_EXTRA);
    /* we should still have 2 slots available for everyone else */
    fail_unless(hub_slots_free() == 2);
    ss = hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(ss == SLOT_EXTRA);
    fail_unless(hub_slots_free() == 2);
    hub_free_upload_slot(ahub, "nicke", SLOT_EXTRA);
    fail_unless(hub_slots_free() == 2);
    /* now the extra slots should be gone */
    ss = hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(ss == SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 2);

    /* increase total number of slots */
    hub_set_slots(4, per_hub_flag);

    /* use all 4 slots, then lower the available slots */
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(hub_slots_free() == 0);
    fail_unless(used_slots == 4);

    /* ...and lower the number of available slots */
    hub_set_slots(2, per_hub_flag);
    fail_unless(hub_slots_free() == 0);
    fail_unless(used_slots == 4);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 0);
    fail_unless(used_slots == 3);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 0);
    fail_unless(used_slots == 2);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 1);
    fail_unless(used_slots == 1);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free() == 2);
    fail_unless(used_slots == 0);
}

int main(void)
{
    sp_log_set_level("debug");

    global_working_directory = "/tmp/sp-slots-test.d";
    system("/bin/rm -rf /tmp/sp-slots-test.d");
    system("mkdir /tmp/sp-slots-test.d");

    fail_unless(extra_slots_init() == 0);

    hub_list_init();
    hub_t *ahub = hub_new();
    fail_unless(ahub);
    ahub->address = strdup("ahub");
    hub_list_add(ahub);

    /* set 2 slots per hub */
    hub_set_slots(2, true);
    fail_unless(total_slots == 2); /* only one hub => 2 slots total */

    check_slots(ahub);

    /* set 2 slots globally */
    hub_set_slots(2, false);
    fail_unless(total_slots == 2);

    check_slots(ahub);

    /* set 5 slots per hub */
    hub_set_slots(5, true);

    /* add a new hub and make sure we get the new default slots value */
    hub_t *bhub = hub_new();
    fail_unless(bhub);
    bhub->address = strdup("bhub");
    hub_list_add(bhub);

    fail_unless(total_slots == 10); /* 2 hubs => 2*5 = 10 slots total */

    /* decrease per-hub slot settings and check that total slots decreases */
    hub_set_slots(4, true);
    fail_unless(total_slots == 8);

    extra_slots_close();
    system("/bin/rm -rf /tmp/sp-slots-test.d");

    return 0;
}

#endif

