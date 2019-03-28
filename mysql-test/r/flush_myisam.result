#
# Test for bug #55273 "FLUSH TABLE tm WITH READ LOCK for Merge table
#                      causes assert failure".
#
drop table if exists t1, t2, tm;
create table t1 (i int) engine = myisam;
create table t2 (i int) engine = myisam;
create table tm (i int) engine=merge union=(t1, t2);
insert into t1 values (1), (2);
insert into t2 values (3), (4);
# The below statement should succeed and lock merge
# table for read. Only merge table gets flushed and
# not underlying tables.
flush tables tm with read lock;
select * from tm;
i
1
2
3
4
# Check that underlying tables are locked.
select * from t1;
i
1
2
select * from t2;
i
3
4
unlock tables;
# This statement should succeed as well and flush
# all tables in the list.
flush tables tm, t1, t2 with read lock;
select * from tm;
i
1
2
3
4
# Naturally, underlying tables should be locked in this case too.
select * from t1;
i
1
2
select * from t2;
i
3
4
unlock tables;
drop tables tm, t1, t2;
