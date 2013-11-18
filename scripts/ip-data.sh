#!/bin/bash

verbose=false
if [[ "$1" == "-v" ]]; then
    verbose=true
    shift
fi
if [[ $# -lt 2 ]]; then
    echo "Usage: $0 [-v] <ip> <program> [<arg> ...]" >&2
    exit 1
fi
ip=$1
shift

if [[ $ip == "localhost" || $ip == "127.0.0.1" ]]; then
    ifaces="-i lo"
else
    ifaces="-i eth0 -i wlan0"
fi

run() {
    if $verbose; then
	# printf " %q" "$@" >&2
	echo "$@" >&2
    fi
    eval "$@"
}

# kill spawned processes and delete created files on exit
cleanup() {
    rm -f "$out"
    [ -n "$tshark_pid" ] && kill $tshark_pid
}
trap 'cleanup' EXIT

# run tshark and capture IP data to/from host into $out
out=$(mktemp --suffix .ipdata.$ip)
tshark_cmd="tshark $ifaces -q -z conv,ip host $ip > '$out' 2>/dev/null &"
run $tshark_cmd
tshark_pid=$(pgrep -n tshark)

# run program and dump time stats to stdout in a single line,
# redirecting program's stderr to stdout along with time stats
( run /usr/bin/time -p -f "'%e %U %S'" -a "$@" >/dev/null ) 2>&1
kill -SIGINT $tshark_pid # this doesn't kill it if no packets are captured
# wait until the output is written to the file
timeout=0.1
while [ ! -s "$out" ]; do
    sleep $timeout
    timeout=$(echo $timeout '* 2' | bc -l)
    if (( $(echo "$timeout > 1.5" | bc -l) == 1 )); then
	echo "No traffic captured due to timeout"
	exit 3
    fi
done
tshark_pid=""

# dump total ip bytes rx/tx to stdout in a single line
if $verbose; then
    cat "$out" >&2
fi
if (( $(grep '<->' "$out" | wc -l) == 1 )); then
    grep -m 1 '<->' "$out" | awk '{print $5 " " $7}'
else
    echo "No traffic captured" >&2
    exit 4
fi
