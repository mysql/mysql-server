#!/bin/sh

###########################################################################

basename=`basename "$0"`
dirname=`dirname "$0"`

###########################################################################

. "$dirname/utils.sh"

###########################################################################

if [ $# -lt 2 ]; then
  echo "Usage: $basename <test id> log message ..."
  exit 1
fi

test_id="$1"
log_file="$MYSQLTEST_VARDIR/log/$test_id.script.log"

shift

log_debug "$*"
