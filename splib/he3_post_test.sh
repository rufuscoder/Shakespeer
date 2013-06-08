#!/bin/sh

diff /tmp/he3.c.decoded he3.c
rc=$?

if test $rc -ne 0; then
    echo "he3 encoded + decoded file differs from original"
fi

rm -f /tmp/he3.c.encoded
rm -f /tmp/he3.c.decoded

exit $rc

