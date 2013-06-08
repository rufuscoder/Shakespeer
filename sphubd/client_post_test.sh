#!/bin/sh

# verify the download request commands sent in client_test.c

# extra newline needed for diff
echo >> /tmp/client_test.log 

diff -u /tmp/client_test.log - <<EOF
\$Get share\foo-file.zip\$1|\$ADCGET file TTH/ABCDEFGHIJKLMNOPQRSTUVWXYZ234 0 17|\$ADCGET file files.xml.bz2 0 -1|
EOF
rc=$?

if test "$rc" != "0"; then
    echo "client download request is NOT correct"
    exit 1
fi
rm /tmp/client_test.log

if ! test -f "/tmp/client_test_file.zip"; then
    echo "downloaded file client_test_file.zip doesn't exist"
    exit 1
fi
rm /tmp/client_test_file.zip

if ! test -f "/tmp/client_test_file2.zip"; then
    echo "downloaded file client_test_file2.zip doesn't exist"
    exit 1
fi
rm /tmp/client_test_file2.zip

if ! test -f "/tmp/files.xml.foo.bz2"; then
    echo "downloaded filelist for foo doesn't exist"
    exit 1
fi
rm /tmp/files.xml.foo.bz2

rm /tmp/files.xml.foo
rm /tmp/files.xml.bz2
rm /tmp/files.xml
rm /tmp/MyList.DcLst
rm /tmp/MyList

