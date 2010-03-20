#!/bin/sh

SCRIPT_NAME="`basename $0`"

usage()
{
  echo "Usage: ${SCRIPT_NAME} [--help|-h]"
  echo ""
  echo "This script creates the MySQL system tables and starts the server."
}

for arg do
  case "$arg" in
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "${SCRIPT_NAME}: unknown option $arg"
      usage
      exit 2
      ;;
  esac
done

if test ! -x  ./scripts/mysql_install_db
then
  echo "I didn't find the script './scripts/mysql_install_db'."
  echo "Please execute this script in the mysql distribution directory!"
  exit 1;
fi

echo "NOTE: This is a MySQL binary distribution. It's ready to run, you don't"
echo "need to configure it!"
echo ""
echo "To help you a bit, I am now going to create the needed MySQL databases"
echo "and start the MySQL server for you.  If you run into any trouble, please"
echo "consult the MySQL manual, that you can find in the Docs directory."
echo ""

./scripts/mysql_install_db --no-defaults
if [ $? = 0 ]
then
  echo "Starting the mysqld server.  You can test that it is up and running"
  echo "with the command:"
  echo "./bin/mysqladmin version"
  ./bin/mysqld_safe --no-defaults &
fi
