#!/bin/sh

###########################################################################

pid_path="$1"
total_attempts="$2"
event="$3"

case "$3" in
  started)
    check_fn='check_started';
    ;;

  stopped)
    check_fn='check_stopped';
    ;;

  *)
    echo "Error: invalid third argument ('started' or 'stopped' expected)."
    exit 0
esac

###########################################################################

check_started()
{
  [ ! -r "$pid_path" ] && return 1

  new_pid=`cat "$pid_path" 2>/dev/null`

  [ $? -eq 0 -a "$original_pid" = "$new_pid" ] && return 1

  return 0
}

###########################################################################

check_stopped()
{
  [ -r "$pid_path" ] && return 1

  return 0
}

###########################################################################

cur_attempt=1

while true; do

  if ( eval $check_fn ); then
    echo "Success: the process has been $event."
    exit 0
  fi

  [ $cur_attempt -ge $total_attempts ] && break

  sleep 1

  cur_attempt=`expr $cur_attempt + 1`

done

echo "Error: the process has not been $event in $total_attempts secs."
exit 0

