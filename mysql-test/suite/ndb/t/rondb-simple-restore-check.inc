if(!$t2_only)
{
--output $t1m_compare
show create table t1;
--diff_files $t1m $t1m_compare

--output $t1d_compare
select * from t1 order by id;
--diff_files $t1d $t1d_compare

disable_query_log; drop table t1; enable_query_log;
}

--output $t2m_compare
show create table t2;
--diff_files $t2m $t2m_compare

--output $t2d_compare
select * from t2 order by id;
--diff_files $t2d $t2d_compare

disable_query_log; drop table t2; enable_query_log;

if(!$t2_only)
{
--echo - Restored database matches original
}
if($t2_only)
{
--echo - Table t2 matches original
--let $t2_only= 0
}
