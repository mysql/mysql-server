#
# Bug #10901 Analyze Table on new table destroys table
# This is minimal test case to get error
# The problem was that analyze table wrote the shared state to the
# file and this didn't include the inserts while locked. A check was
# needed to ensure that state information was not updated when
# executing analyze table for a locked table.  The analyze table had
# to be within locks and check table had to be after unlocking since
# then it brings the wrong state from disk rather than from the
# currently correct internal state. The insert is needed since it
# changes the file state, number of records.  The fix is to
# synchronise the state of the shared state and the current state
# before calling mi_state_info_write
#

create table t1 (a bigint);
lock tables t1 write;
insert into t1 values(0);
analyze table t1;
unlock tables;
check table t1;

drop table t1;

create table t1 (a bigint);
insert into t1 values(0);
lock tables t1 write;
delete from t1;
analyze table t1;
unlock tables;
check table t1;

drop table t1;

create table t1 (a bigint);
insert into t1 values(0);
analyze table t1;
check table t1;

drop table t1;

# Bug #14902 ANALYZE TABLE fails to recognize up-to-date tables
# minimal test case to get an error.
# The problem is happening when analysing table with FT index that
# contains stopwords only. The first execution of analyze table should
# mark index statistics as up to date so that next execution of this
# statement will end up with Table is up to date status.
create table t1 (a mediumtext, fulltext key key1(a)) charset utf8mb3 COLLATE utf8mb3_general_ci;
insert into t1 values ('hello');

analyze table t1;
analyze table t1;

drop table t1;

#
# bug#15225 (ANALYZE temporary has no effect)
#
create temporary table t1(a int, index(a));
insert into t1 values('1'),('2'),('3'),('4'),('5');
analyze table t1;
show index from t1;
drop table t1;

--echo End of 4.1 tests

#
# Bug #30495: optimize table t1,t2,t3 extended errors
#
create table t1(a int);
--error 1064
analyze table t1 extended;
--error 1064
optimize table t1 extended;
drop table t1;

--echo End of 5.0 tests

--echo #
--echo # Bug#33079073 - sql_cmd_call::execute_inner(thd*): assertion
--echo #                `thd->is_error() || thd->killed' failed.
--echo # Assertion to check if error state is set in the diagnostics area
--echo # on ANALYZE TABLE to UPDATE HISTOGRAM non-success in SP is failed.
--echo #
CREATE PROCEDURE p() ANALYZE TABLE v UPDATE HISTOGRAM ON w;
--echo # Without fix, assert condition fails for following statement in the debug
--echo # build. Statement executes successfully in the non-debug build.
CALL p();
DROP PROCEDURE p;
