#!/bin/sh

###########################################################################

# NOTE: this script returns 0 (success) even in case of failure. This is
# because this script is executed under mysql-test-run[.pl] and it's better to
# examine particular problem in log file, than just having said that the test
# case has failed.

###########################################################################

check_restart()
{
  if [ ! -r "$pid_path" ]; then
    user_msg='the process was killed'
    return 1
  fi

  new_pid=`cat "$pid_path" 2>/dev/null`

  if [ $? -eq 0 -a "$original_pid" = "$new_pid" ]; then
    user_msg='the process was not restarted'
    return 1
  fi

  user_msg='the process was restarted'
  return 0
}

###########################################################################

if [ $# -ne 3 ]; then
  echo "Usage: kill_n_check.sh <pid file path> killed|restarted <timeout>"
  exit 0
fi

pid_path="$1"
expected_result="$2"
total_timeout="$3"

if [ "$expected_result" != 'killed' -a \
     "$expected_result" != 'restarted' ]; then
  echo "Error: invalid second argument ('killed' or 'restarted' expected)."
  exit 0
fi

if [ -z "$pid_path" ]; then
  echo "Error: invalid PID path ($pid_path)."
  exit 0
fi

if [ ! -r "$pid_path" ]; then
  echo "Error: PID file ($pid_path) does not exist."
  exit 0
fi

if [ -z "$total_timeout" ]; then
  echo "Error: timeout is not specified."
  exit 0
fi

###########################################################################

original_pid=`cat "$pid_path"`

echo "Killing the process..."

kill -9 $original_pid

###########################################################################

echo "Sleeping..."

if [ "$expected_result" = "restarted" ]; then

  # Wait for the process to restart.

  cur_attempt=1

  while true; do

    if check_restart; then
      echo "Success: $user_msg."
      exit 0
    fi

    [ $cur_attempt -ge $total_timeout ] && break

    sleep 1

    cur_attempt=`expr $cur_attempt + 1`

  done

  echo "Error: $user_msg."
  exit 0

else # $expected_result == killed

  # Here we have to sleep for some long time to ensure that the process will
  # not be restarted.

  sleep $total_timeout

  new_pid=`cat "$pid_path" 2>/dev/null`

  if [ "$new_pid" -a "$new_pid" -ne "$original_pid" ]; then
    echo "Error: the process was restarted."
  else
    echo "Success: the process was killed."
  fi

  exit 0

fi
