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

if [ $# -ne 7 ]; then
  echo "Usage: wait_for_socket.sh <executable path> <socket path> <username> <password> <db> <timeout> <test id>"
  exit 1
fi

client_exe="$1"
socket_path="$2"
username="$3"
password="$4"
db="$5"
total_timeout="$6"
test_id="$7"
log_file="$MYSQLTEST_VARDIR/log/$test_id.script.log"

log_debug "-- $basename: starting --"
log_debug "client_exe: '$client_exe'"
log_debug "socket_path: '$socket_path'"
log_debug "username: '$username'"
log_debug "password: '$password'"
log_debug "db: '$db'"
log_debug "total_timeout: '$total_timeout'"
log_debug "test_id: '$test_id'"
log_debug "log_file: '$log_file'"

###########################################################################

if [ -z "$client_exe" ]; then
  log_error "Invalid path to client executable ($client_exe)."
  quit 0;
fi

if [ ! -x "$client_exe" ]; then
  log_error "Client by path '$client_exe' is not available."
  quit 0;
fi

if [ -z "$socket_path" ]; then
  log_error "Invalid socket patch ($socket_path)."
  quit 0
fi

###########################################################################

client_args="--no-defaults --silent --socket=$socket_path --connect_timeout=1 "

[ -n "$username" ] && client_args="$client_args --user=$username "
[ -n "$password" ] && client_args="$client_args --password=$password "
[ -n "$db" ] && client_args="$client_args $db"

log_debug "client_args: '$client_args'"

###########################################################################

cur_attempt=1

while true; do

  log_debug "cur_attempt: $cur_attempt."

  if ( echo 'quit' | "$client_exe" $client_args >/dev/null 2>&1 ); then
    log_info "Success: server is ready to accept connection on socket."
    quit 0
  fi

  [ $cur_attempt -ge $total_timeout ] && break

  sleep 1

  cur_attempt=`expr $cur_attempt + 1`

done

log_error "Server does not accept connections after $total_timeout seconds."
quit 0
