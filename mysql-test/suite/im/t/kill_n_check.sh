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

check_restart()
{
  if [ ! -r "$pid_path" ]; then
    log_debug "No '$pid_path' found."
    user_msg='the process was killed'
    return 1
  fi

  new_pid=`cat "$pid_path" 2>/dev/null`
  err_code=$?

  log_debug "err_code: $err_code; original_pid: $original_pid; new_pid: $new_pid."

  if [ $err_code -eq 0 -a "$original_pid" = "$new_pid" ]; then
    log_debug "The process was not restarted."
    user_msg='the process was not restarted'
    return 1
  fi

  log_debug "The process was restarted."
  user_msg='the process was restarted'
  return 0
}

###########################################################################

if [ $# -ne 4 ]; then
  echo "Usage: $basename <pid file path> killed|restarted <timeout> <test id>"
  exit 1
fi

pid_path="$1"
expected_result="$2"
total_timeout="$3"
test_id="$4"
log_file="$MYSQLTEST_VARDIR/log/$test_id.script.log"

log_debug "-- $basename: starting --"
log_debug "pid_path: '$pid_path'"
log_debug "expected_result: '$expected_result'"
log_debug "total_timeout: '$total_timeout'"
log_debug "test_id: '$test_id'"
log_debug "log_file: '$log_file'"

###########################################################################

if [ "$expected_result" != 'killed' -a \
     "$expected_result" != 'restarted' ]; then
  log_error "Invalid second argument ($expected_result): 'killed' or 'restarted' expected."
  quit 0
fi

if [ -z "$pid_path" ]; then
  log_error "Invalid PID path ($pid_path)."
  quit 0
fi

if [ ! -r "$pid_path" ]; then
  log_error "PID file ($pid_path) does not exist."
  quit 0
fi

if [ -z "$total_timeout" ]; then
  log_error "Timeout is not specified."
  quit 0
fi

###########################################################################

original_pid=`cat "$pid_path"`
log_debug "original_pid: $original_pid."

log_info "Killing the process..."

kill -9 $original_pid

###########################################################################

log_info "Waiting..."

if [ "$expected_result" = "restarted" ]; then

  # Wait for the process to restart.

  cur_attempt=1

  while true; do

    log_debug "cur_attempt: $cur_attempt."

    if check_restart; then
      log_info "Success: $user_msg."
      quit 0
    fi

    [ $cur_attempt -ge $total_timeout ] && break

    log_debug "Sleeping for 1 second..."
    sleep 1

    cur_attempt=`expr $cur_attempt + 1`

  done

  log_error "$user_msg."
  quit 0

else # $expected_result == killed

  # Here we have to sleep for some long time to ensure that the process will
  # not be restarted.

  log_debug "Sleeping for $total_timeout seconds..."
  sleep $total_timeout

  new_pid=`cat "$pid_path" 2>/dev/null`
  log_debug "new_pid: $new_pid."

  if [ "$new_pid" -a "$new_pid" -ne "$original_pid" ]; then
    log_error "The process was restarted."
  else
    log_info "Success: the process was killed."
  fi

  quit 0

fi
