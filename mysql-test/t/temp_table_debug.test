# Test that checks that we properly delete temporary tables
# on server restart.
#
# This test crashes server on purpose.
# Add includes which are necessary to do this safely.
--source include/have_debug.inc
--source include/not_valgrind.inc
--source include/not_crashrep.inc

# It also requires various storage engines.
--source include/have_archive.inc
--source include/have_blackhole.inc
--source include/have_myisam.inc

# windows does not support symlink
--source include/not_windows.inc

--echo # Create temporary tables in various storage engines.
create temporary table ta (i int not null) engine=archive;
create temporary table tb (i int not null) engine=blackhole;
create temporary table tc (i int not null) engine=csv;
create temporary table th (i int not null) engine=heap;
create temporary table ti (i int not null) engine=innodb;
create temporary table tm (i int not null) engine=myisam;
create temporary table tg (i int not null) engine=merge union=();

--echo # Also manually create files mimicing temporary table files which
--echo # are symlinks to files in data-directory.
let $MYSQLD_DATADIR= `select @@datadir`;
create database mysqltest;
create table mysqltest.t1 (i int not null) engine=myisam;
--exec ln -s $MYSQLD_DATADIR/mysqltest/t1.MYI $MYSQL_TMP_DIR/mysqld.1/#sqlzzzz.MYI
--exec ln -s $MYSQLD_DATADIR/mysqltest/t1.MYD $MYSQL_TMP_DIR/mysqld.1/#sqlzzzz.MYD

--echo # Check files created for them.
--replace_regex /#sql.*\./#sqlXXXX./ /_[0-9]+\.sdi/_XXX.sdi/
--list_files $MYSQL_TMP_DIR/mysqld.1/

--echo # Crash server:
begin;
select * from ti;
--echo # Ask server to crash on next commit.
set session debug="+d,crash_commit_before";
--echo # Write file to make mysql-test-run.pl start up the server again.
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--echo # Crash server.
--error 2013
commit;

--echo # Wait until disconnected.
--source include/wait_until_disconnected.inc

--echo # Check that temporary table files are still around.
--replace_regex /#sql.*\./#sqlXXXX./ /_[0-9]+\.sdi/_XXX.sdi/
--list_files $MYSQL_TMP_DIR/mysqld.1/

--echo # Reconnect.
--source include/wait_until_connected_again.inc

--echo # Check that temporary table files are gone after restart,
--echo # including symlinks.
--replace_regex /#sql.*\./#sqlXXXX./
--list_files $MYSQL_TMP_DIR/mysqld.1/

--echo # But files in data-directory to which these symlinks were pointing
--echo # should still be around.
--replace_regex /_[0-9]+\.sdi/_XXX.sdi/
--list_files $MYSQLD_DATADIR/mysqltest/
drop database mysqltest;

