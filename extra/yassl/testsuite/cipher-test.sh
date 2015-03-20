#!/bin/bash

# test all yassl cipher suties 
# 


no_pid=-1
server_pid=$no_pid


do_cleanup() {
    echo "in cleanup"

    if [[ $server_pid != $no_pid ]]
    then
        echo "killing server"
        kill -9 $server_pid
    fi
}

do_trap() {
    echo "got trap"
    do_cleanup
    exit -1
}

trap do_trap INT TERM


# make sure example server and client are built
if test ! -s ../examples/server/server; then
    echo "Please build yaSSL first, example server missing"
    exit -1
fi

if test ! -s ../examples/client/client; then
    echo "Please build yaSSL first, example client missing"
    exit -1
fi


# non DSA suites
for suite in {"DHE-RSA-AES256-SHA","AES256-SHA","DHE-RSA-AES128-SHA","AES128-SHA","AES256-RMD","AES128-RMD","DES-CBC3-RMD","DHE-RSA-AES256-RMD","DHE-RSA-AES128-RMD","DHE-RSA-DES-CBC3-RMD","RC4-SHA","RC4-MD5","DES-CBC3-SHA","DES-CBC-SHA","EDH-RSA-DES-CBC3-SHA","EDH-RSA-DES-CBC-SHA"}
do
  for client_auth in {y,n}
  do
    echo "Trying $suite client auth = $client_auth ..."

    if test -e server_ready; then
        echo -e "removing exisitng server_ready file"
        rm server_ready
    fi
    ../examples/server/server $client_auth &
    server_pid=$!

    while [ ! -s server_ready ]; do
        echo -e "waiting for server_ready file..."
        sleep 0.1
    done

    ../examples/client/client $suite
    client_result=$?

    wait $server_pid
    server_result=$?

    server_pid=$no_pid

    if [[ $client_result != 0 ]]
    then
        echo "Client Error"
        exit $client_result
    fi

    if [[ $server_result != 0 ]]
    then
        echo "Server Error"
        exit $server_result
    fi

  done   # end client auth loop
done  # end non dsa suite list
echo -e "Non DSA Loop SUCCESS"



# DSA suites
for suite in {"DHE-DSS-AES256-SHA","DHE-DSS-AES128-SHA","DHE-DSS-AES256-RMD","DHE-DSS-AES128-RMD","DHE-DSS-DES-CBC3-RMD","EDH-DSS-DES-CBC3-SHA","EDH-DSS-DES-CBC-SHA"}
do
  for client_auth in {y,n}
  do
    echo "Trying $suite client auth = $client_auth ..."

    if test -e server_ready; then
        echo -e "removing exisitng server_ready file"
        rm server_ready
    fi
    # d signifies DSA
    ../examples/server/server $client_auth d &
    server_pid=$!

    while [ ! -s server_ready ]; do
        echo -e "waiting for server_ready file..."
        sleep 0.1
    done

    ../examples/client/client $suite
    client_result=$?

    wait $server_pid
    server_result=$?

    server_pid=$no_pid

    if [[ $client_result != 0 ]]
    then
        echo "Client Error"
        exit $client_result
    fi

    if [[ $server_result != 0 ]]
    then
        echo "Server Error"
        exit $server_result
    fi

  done   # end client auth loop
done  # end dsa suite list
echo -e "DSA Loop SUCCESS"

exit 0
