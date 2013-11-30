#!/bin/bash
#trap 'pgrep -n '"'^nc$' >/dev/null"' && kill $!' EXIT
if [[ "$1" == "-l" ]]; then
    if hostname | grep -q purdue; then
	set -- "-l" "${@:3}"
    else
	( sleep 1; echo $2 ) &
    fi
fi
# out=/tmp/mync.out
# >$out
# trap 'cat $out' EXIT
nc $@ -v 2>&1 #| perl -i -ne 'if (/\[any\] (\d+)/) { print "$1\n"; } else { print STDERR }' #2>$out
#wait $!
