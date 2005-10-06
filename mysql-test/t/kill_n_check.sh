#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Usage: kill_n_check.sh <pid file path> killed|restarted"
  exit 0
fi

pid_path="$1"
expected_result="$2"

if [ -z "$pid_path" -o ! -r "$pid_path" ]; then
  echo "Error: invalid PID path ($pid_path) or PID file does not exist."
  exit 0
fi

if [ "$expected_result" != "killed" -a \
     "$expected_result" != "restarted" ]; then
  echo "Error: expected result must be either 'killed' or 'restarted'."
  exit 0
fi

# echo "PID path: '$pid_path'"

original_pid=`cat "$pid_path"`

# echo "Original PID: $original_pid"

echo "Killing the process..."

kill -9 $original_pid

echo "Sleeping..."

sleep 3

new_pid=""

[ -r "$pid_path" ] && new_pid=`cat "$pid_path"`

# echo "New PID: $new_pid"

if [ "$expected_result" == "restarted" ]; then

  if [ -z "$new_pid" ]; then
    echo "Error: the process was killed."
    exit 0
  fi

  if [ "$original_pid" -eq "$new_pid" ]; then
    echo "Error: the process was not restarted."
    exit 0
  fi
  
  echo "Success: the process was restarted."
  exit 0
  
else # $expected_result == killed
    
  if [ "$new_pid" -a "$new_pid" -ne "$original_pid" ]; then
    echo "Error: the process was restarted."
    exit 0
  fi

  echo "Success: the process was killed."
  exit 0
fi
