#!/bin/sh
if test ! -x  ./scripts/mysql_install_db
then
  echo "I didn't find the script './scripts/mysql_install_db'."
  echo "Please execute this script in the mysql distribution directory!"
  exit 1;
fi

./scripts/mysql_install_db
if [ $? = 0 ]
then
  echo "Starting the mysqld server.  You can test that it is up and running"
  echo "with the command:"
  echo "./bin/mysqladmin version"
  ./bin/safe_mysqld &
fi
