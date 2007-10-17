#!/bin/sh

###########################################################################

# NOTE: this script returns 0 (success) even in case of failure (except for
# usage-error). This is because this script is executed under
# mysql-test-run[.pl] and it's better to examine particular problem in log
# file, than just having said that the test case has failed.

###########################################################################

basename=`basename "$0"`
dirname=`dirname "$0"`

###########################################################################

. "$dirname/utils.sh"

###########################################################################

check_started()
{
  if [ ! -r "$pid_path" ]; then
    log_debug "No PID-file ($pid_path) found -- not started."
    return 1
  fi

  new_pid=`cat "$pid_path" 2>/dev/null`
  err_code=$?

  log_debug "err_code: $err_code; new_pid: $new_pid."

  if [ $? -ne 0 -o -z "$new_pid" ]; then
    log_debug "The process was not started."
    return 1
  fi

  log_debug "The process was started."
  return 0
}

###########################################################################

check_stopped()
{
  if [ -r "$pid_path" ]; then
    log_debug "PID-file '$pid_path' exists -- not stopped."
    return 1
  fi

  log_debug "No PID-file ($pid_path) found -- stopped."
  return 0
}

###########################################################################

if [ $# -ne 4 ]; then
  echo "Usage: $basename <pid file path> <total attempts> started|stopped <test id>"
  exit 1
fi

pid_path="$1"
total_attempts="$2"
event="$3"
test_id="$4"
log_file="$MYSQLTEST_VARDIR/log/$test_id.script.log"

log_debug "-- $basename: starting --"
log_debug "pid_path: '$pid_path'"
log_debug "total_attempts: '$total_attempts'"
log_debug "event: '$event'"
log_debug "test_id: '$test_id'"
log_debug "log_file: '$log_file'"

###########################################################################

case "$event" in
  started)
    check_fn='check_started';
    ;;

  stopped)
    check_fn='check_stopped';
    ;;

  *)
    log_error "Invalid third argument ('started' or 'stopped' expected)."
    quit 0
esac

###########################################################################

cur_attempt=1

while true; do

  log_debug "cur_attempt: $cur_attempt."

  if ( eval $check_fn ); then
    log_info "Success: the process has been $event."
    quit 0
  fi

  [ $cur_attempt -ge $total_attempts ] && break

  log_debug "Sleeping for 1 second..."
  sleep 1

  cur_attempt=`expr $cur_attempt + 1`

done

log_error "The process has not been $event in $total_attempts secs."
quit 0
