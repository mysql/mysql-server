--let $MYSQLD_DATADIR= `select @@datadir`
--let $events_file=$MYSQLTEST_VARDIR/tmp/events.sql
--let GREP_PRINT_NOT_VERBOSE=1

#
# Testing DROP of multiple tables logging when a query produces few groups
# into binary log.
#

CREATE TABLE t1 (a int) ENGINE= innodb;
CREATE TABLE tm (a int) ENGINE= MyISAM;
RESET MASTER;
CREATE TEMPORARY TABLE ttm1 (a INT) ENGINE=MyISAM;
CREATE TEMPORARY TABLE tti1 (a INT) ENGINE=Innodb;

# The query is logged in three parts where only
# the first one is sequenced.
# Such logging is caused by just one cache is used for logging.

--echo *** The query is logged in three parts.

DROP TABLE tm,t1,ttm1,tti1;

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 > $events_file
--let GREP_FILE=$events_file

#
# The final grep invocation should be done by the top level part.
# It may produce results sensitive to the test environment (e.g GTID).
#
