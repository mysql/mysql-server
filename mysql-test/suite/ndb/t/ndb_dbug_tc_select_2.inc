
--eval create table t1 (a $type1, b $type2, c $type3, d int, primary key(a,c), unique(c,b)) engine ndb
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "all error 8072" >> $NDB_TOOLS_OUTPUT

--echo # verify tc select on insert using simple int key
--disable_query_log
--disable_result_log
let $i= 1000;
let $j= 1;
while ($i)
{
--eval insert into t1 values ("$i","$j",0,0)
dec $i;
inc $j;
}
--enable_query_log
--enable_result_log

--echo # verify tc select on select using simple pk int key
--disable_query_log
--disable_result_log
let $i= 100;
while ($i)
{
--eval select * from t1 where a="$i" and c="0"
dec $i;
}

--enable_query_log
--enable_result_log

--echo # verify tc select on select using simple unique int key
--disable_query_log
--disable_result_log
let $i= 100;
while ($i)
{
--eval select * from t1 where b="$i" and c="0"
dec $i;
}
--enable_query_log
--enable_result_log

--echo # verify tc select on update using simple pk int key
--disable_query_log
--disable_result_log
let $i= 100;
while ($i)
{
--eval update t1 set d="10" where a="$i" and c="0"
dec $i;
}
--enable_query_log
--enable_result_log

--echo # verify tc select on update using simple unique int key
--disable_query_log
--disable_result_log
let $i= 100;
while ($i)
{
--eval update t1 set d=11 where b="$i" and c="0"
dec $i;
}
--enable_query_log
--enable_result_log

--echo # verify tc select on delete using simple pk int key
--disable_query_log
--disable_result_log
let $i= 40;
while ($i)
{
--eval delete from t1 where a="$i" and c="0"
dec $i;
}
--enable_query_log
--enable_result_log

--echo # verify tc select on delete using simple unique int key
--disable_query_log
--disable_result_log
let $i= 40;
while ($i)
{
--eval delete from t1 where b="$i" and c="0"
dec $i;
}
--enable_query_log
--enable_result_log

--echo # Cleanup
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "all error 0" >> $NDB_TOOLS_OUTPUT
drop table t1;
