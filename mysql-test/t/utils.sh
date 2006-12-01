###########################################################################
#
# This file provides utility functions and is included by other scripts.
#
# The following global variables must be set before calling functions from this
# file:
#   - basename -- base name of the calling script (main application);
#   - log_file -- where to store log records;
#
###########################################################################

log()
{
  [ -z "$log_file" ] && return;

  log_level="$1"
  log_msg="$2"
  ts=`date`

  echo "[$ts] [$basename] [$log_level] $log_msg" >> "$log_file";
}

###########################################################################

log_debug()
{
  log 'DEBUG' "$1"
}

###########################################################################

log_info()
{
  log 'INFO' "$1"
  echo "$1"
}

###########################################################################

log_error()
{
  log 'ERROR' "$1"
  echo "Error: $1"
}

###########################################################################

quit()
{
  exit_status="$1"

  log_debug "-- $basename: finished (exit_status: $exit_status) --"

  exit $exit_status
}
