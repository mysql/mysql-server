#!/bin/sh
if [ "$MYSQL_TEST_DIR" ]
then
  rm -f $MYSQL_TEST_DIR/var/tmp/*.frm $MYSQL_TEST_DIR/var/tmp/*.MY?
fi
