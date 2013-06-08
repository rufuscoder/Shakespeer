#!/bin/sh

# this script must be executed with an absolute path
dir=`dirname $0`
sphubd=$dir/sphubd

args=""
while test -n "$1"; do
	args=$args" '"$1"'"
	shift
done

cat >/tmp/gdb-sphubd.tmp <<EOF
echo launching sphubd:\n
echo executable: "$sphubd"\n
echo arguments: -f $args -d debug\n
echo ================================================================================\n
echo This is a development snapshot of ShakesPeer. Thanks for helping with testing.
echo If the program crashes, type 'backtrace full' at the (gdb) prompt.\n
echo Then copy and paste everything in this window and send it in an email to:\n
echo \                    shakespeer-devel@bzero.se\n
echo You can then close this window by typing 'quit' at the prompt.
echo ================================================================================\n
file "$sphubd"
run -f $args -d debug
echo
EOF

cat > /tmp/gdb-start-sphubd.tmp <<EOF
gdb -quiet -n -x /tmp/gdb-sphubd.tmp || {
echo ===============================================================================
echo You must install XCode and the Developer Tools
echo before running a development shapshot of ShakesPeer.
echo Press enter to quit.
echo ===============================================================================
read
}
rm -f /tmp/gdb-start-sphubd.tmp
rm -f /tmp/gdb-sphubd.tmp
EOF

osascript -e 'tell application "Terminal" to do script "sh /tmp/gdb-start-sphubd.tmp; exit"'

