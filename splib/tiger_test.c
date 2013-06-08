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

#include "tiger.h"
#include "unit_test.h"

#define hash(str, w1, w2, w3) \
    tiger((word64*)str, strlen(str), res); \
    fail_unless(res[0] == w1); \
    fail_unless(res[1] == w2); \
    fail_unless(res[2] == w3);

int main(void)
{
    word64 res[3];

    hash("", 0x24F0130C63AC9332LL, 0x16166E76B1BB925FLL, 0xF373DE2D49584E7ALL);
    hash("abc", 0xF258C1E88414AB2ALL, 0x527AB541FFC5B8BFLL, 0x935F7B951C132951LL);
    hash("Tiger", 0x9F00F599072300DDLL, 0x276ABB38C8EB6DECLL, 0x37790C116F9D2BDFLL);

    hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-",
            0x87FB2A9083851CF7LL, 0x470D2CF810E6DF9ELL, 0xB586445034A5A386LL);

    hash("ABCDEFGHIJKLMNOPQRSTUVWXYZ=abcdefghijklmnopqrstuvwxyz+0123456789",
            0x467DB80863EBCE48LL, 0x8DF1CD1261655DE9LL, 0x57896565975F9197LL);
    hash("Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham",
            0x0C410A042968868ALL, 0X1671DA5A3FD29A72LL, 0X5EC1E457D3CDB303LL);
    hash("Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham, proceedings of Fast Software Encryption 3, Cambridge.",
            0xEBF591D5AFA655CELL, 0X7F22894FF87F54ACLL, 0X89C811B6B0DA3193LL);
    hash("Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham, proceedings of Fast Software Encryption 3, Cambridge, 1996.",
            0x3D9AEB03D1BD1A63LL, 0X57B2774DFD6D5B24LL, 0XDD68151D503974FCLL);
    hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-",
            0x00B83EB4E53440C5LL, 0x76AC6AAEE0A74858LL, 0x25FD15E70A59FFE4LL);

    return 0;
}

