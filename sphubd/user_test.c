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

#include "user.h"
#include "unit_test.h"
#include "log.h"

/* ouch! */
int extra_slots_get_for_user(const char *nick)
{
    return 0;
}

int main(void)
{
    sp_log_set_level("debug");

    user_t *user1 = user_new_from_myinfo("$MyINFO $ALL user1 <++ V:0.668,M:A,H:1/0/0,S:3>$ $LAN(T1) $$98734513452$|", NULL);
    fail_unless(user1);

    fail_unless(strcmp(user1->nick, "user1") == 0);
    fail_unless(strcmp(user1->tag, "<++ V:0.668,M:A,H:1/0/0,S:3>") == 0);
    fail_unless(strcmp(user1->speed, "LAN(T1)") == 0);
    fail_unless(user1->description == 0);
    fail_unless(user1->email == 0);
    fail_unless(user1->shared_size == 98734513452LL);
    fail_unless(user1->is_operator== false);
    fail_unless(user1->passive == false);
    fail_unless(user1->hub == NULL);
    fail_unless(user1->ip == NULL);

    user_t *user2 = user_new_from_myinfo("$MyINFO $ALL user2 description$ $LAN(T1)$email@address$98734513452$|", (void *)0xDEADBEEF);
    fail_unless(user2);
    fail_unless(strcmp(user2->description, "description") == 0);
    fail_unless(strcmp(user2->email, "email@address") == 0);
    fail_unless(user2->hub == (void *)0xDEADBEEF);
    fail_unless(strcmp(user1->speed, "LAN(T1)") == 0);

    /* nick in utf-8 */
    user_t *user3 = user_new_from_myinfo("$MyINFO $ALL Ã¥Ã¤Ã¶â‚¬-Ã¼tf8 <++ V:0.668,M:A,H:1/0/0,S:3>$ $LAN(T1)$$1234567890$", NULL);
    fail_unless(user3);
    fail_unless(strcmp(user3->nick, "Ã¥Ã¤Ã¶â‚¬-Ã¼tf8") == 0);

    /* nick in windows-1252 encoding */
    user_t *user4 = user_new_from_myinfo("$MyINFO $ALL åäö <++ V:0.668,M:A,H:1/0/0,S:3>$ $LAN(T1)$$1234567890$", NULL);
    fail_unless(user4);
    fail_unless(strcmp(user4->nick, "åäö") == 0);

    user_t *user5 = user_new_from_myinfo("$MyINFO $ALL nick GAZONK$ $NetLimiter [3 kB/s]$$30252370217$", NULL);
    fail_unless(user5);

    user_t *user6 = user_new_from_myinfo("$MyINFO $ALL [2Mbit]Otto musik<++ V:0.668,M:P,H:8/0/0,S:12>$ $DSL$mail@address$11500339821$|", NULL);
    fail_unless(user6);
    fail_unless(strcmp(user6->description, "musik") == 0);

    user_t *user7 = user_new_from_myinfo("$MyINFO $ALL -?Dream.boT?- This hub is Powered by RoboCop? v3.2a$ $LAN(T1) $Security@Bot$|", NULL);
    fail_unless(user7);
    fail_unless(strcmp(user7->email, "Security@Bot") == 0);

    user_t *user8 = user_new_from_myinfo("$MyINFO $ALL [dgc]someone <++ V:0.668,M:A,H:1/1/0,S:9>$$[DGC]$$113871334848$|", NULL);
    fail_unless(user8);
    fail_unless(strcmp(user8->speed, "[DGC]") == 0);

    user_t *user9 = user_new_from_myinfo("$MyINFO $ALL Mr.Jones $P$$$1716055818$|", NULL);
    fail_unless(user9);

    user_t *user10 = user_new_from_myinfo("$MyINFO $ALL kurtgoran $A$$$100424222195$|", NULL);
    fail_unless(user10);

    user_free(user1);
    user_free(user2);
    user_free(user3);
    user_free(user4);
    user_free(user6);
    user_free(user7);
    user_free(user8);
    user_free(user9);
    user_free(user10);

    return 0;
}

