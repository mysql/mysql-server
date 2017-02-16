#!/bin/bash

# a poor man's profiler
# http://webcache.googleusercontent.com/search?q=cache:http://mituzas.lt/2009/02/15/poor-mans-contention-profiling/

nsamples=1
sleeptime=1

while [ $# -gt 0 ] ; do
    arg=$1; 
    if [[ $arg =~ --(.*)=(.*) ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	break
	fi
    shift
done

pid=$1

for x in $(seq 1 $nsamples)
  do
    gdb -ex "set pagination 0" -ex "thread apply all bt" -batch -p $pid
    sleep $sleeptime
  done | \
awk '
  BEGIN { s = ""; } 
  /^Thread/ { if (s != "") print s; s = ""; } 
  /^\#/ { if ($3 == "in") { v = $4; } else { v = $2 } if (s != "" ) { s = s "," v} else { s = v } } 
  END { print s }' | \
sort | uniq -c | sort -r -n -k 1,1
