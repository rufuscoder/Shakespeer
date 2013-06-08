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

#include "base32.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
    const char *str = " *"
 "* Copyright 2005 Martin Hedenfalk <mhe@home.se>"
 "*"
 "* This file is part of ShakesPeer."
 "*"
 "* ShakesPeer is free software; you can redistribute it and/or modify"
 "* it under the terms of the GNU General Public License as published by"
 "* the Free Software Foundation; either version 2 of the License, or"
 "* (at your option) any later version."
 "*"
 "* ShakesPeer is distributed in the hope that it will be useful,"
 "* but WITHOUT ANY WARRANTY; without even the implied warranty of"
 "* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
 "* GNU General Public License for more details."
 "*"
 "* You should have received a copy of the GNU General Public License"
 "* along with ShakesPeer; if not, wite to the Free Software"
 "* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA"
 "*/";
    char *tmp = base32_encode(str, strlen(str));

    char *data = base32_decode(tmp, NULL);

    fail_unless(memcmp(data, str, base32_decode_length(strlen(tmp))) == 0);

    const char *b = "RF354FEUIXRYFO4O372SXJLKJDIQ2XKIO3XU37OPKEOUEQ7OACWYRFLWDZU5E2QEMESWOS56BIL65HRBFQJHPF4HGWFD6F4E5EJWSA5NZYSHJJP6UZXPUTA5DGOIRIBJILKE65QPP6WBB3I54JRPHUG3XH6NRQTCTLHWAL73NAQ2BCBPKBL3FCZPCAWYNZ7ZN3OHFX2FOZ277IPPLBEZCP6SHDTKJ3CP6SUF2Q6PGZOBFDRFKOX6ERXQADLK6RH4BCOAVRBRQ3S4BTZWLQJI4JKTV7REN4AA22XUJ7AITQFMIMMG4XAA";
    unsigned outlen = 0;
    void *data2 = base32_decode(b, &outlen);
    fail_unless(data2);
    fail_unless(outlen % 24 == 0);
    fail_unless(outlen == 192);

    return 0;
}

