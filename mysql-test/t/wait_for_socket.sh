#!/bin/sh

###########################################################################

if [ $# -ne 6 ]; then
  echo "Usage: wait_for_socket.sh <executable path> <socket path> <username> <password> <db> <timeout>"
  exit 0
fi

client_exe="$1"
socket_path="$2"
username="$3"
password="$4"
db="$5"
total_timeout="$6"

###########################################################################

if [ -z "$client_exe" ]; then
  echo "Error: invalid path to client executable ($client_exe)."
  exit 0;
fi

if [ ! -x "$client_exe" ]; then
  echo "Error: client by path '$client_exe' is not available."
  exit 0;
fi

if [ -z "$socket_path" ]; then
  echo "Error: invalid socket patch."
  exit 0
fi

###########################################################################

client_args="--silent --socket=$socket_path "

[ -n "$username" ] && client_args="$client_args --user=$username "
[ -n "$password" ] && client_args="$client_args --password=$password "
[ -n "$db" ] && client_args="$client_args $db"

###########################################################################

cur_attempt=1

while true; do

  if ( echo 'quit' | "$client_exe" $client_args >/dev/null 2>&1 ); then
    echo "Success: server is ready to accept connection on socket."
    exit 0
  fi

  [ $cur_attempt -ge $total_timeout ] && break

  sleep 1

  cur_attempt=`expr $cur_attempt + 1`

done

echo "Error: server does not accept connections after $total_timeout seconds."
exit 0
