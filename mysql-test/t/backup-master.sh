#!/bin/sh
if [ "$MYSQL_TEST_DIR" ]
then
  rm -f $MYSQLTEST_VARDIR/tmp/*.frm $MYSQLTEST_VARDIR/tmp/*.MY?
fi
