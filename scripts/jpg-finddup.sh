#!/bin/bash
usage() { echo "Usage: $0 [-d <locate_dbpath>] [-i <ignore_regex>]"; exit $1; }
while getopts "d:i:I:" o; do
    case "$o" in
	d)
	    d=${OPTARG}
	    [ -f "$d" ] || usage 1 ;;
	i) i+=" -e '${OPTARG}'" ;;
	I) I=${OPTARG} ;;
	*) usage 1 ;;
    esac
done
shift $((OPTIND - 1))
[ -n "$d" ] && d="-d $d"
if [ -n "$I" ] && [ -f "$I" ]; then
    i+=$(cat "$I" | grep -v '^$' | sed "s/.*/-e '&'/" | tr '\n' ' ')
fi
[ -z "$i" ] && i=" -e '^"'$'"'"
echo "i=$i"
echo locate $d -i "'*.jpg'" -e
echo egrep -v $i
locate $d -i '.jpg' -e | egrep -v "$i" \
| xargs -d '\n' stat --printf '%10i %n\n' | sort -s | uniq -w 10 | cut -c 12- \
| xargs -d '\n' jpghash 2>/dev/null | sort | uniq -D -w 40
