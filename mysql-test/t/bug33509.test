#
# BUG#33509: Server crashes with number of recursive subqueries=61
#  

# Thread stack overrun in debug mode on sparc
--source include/not_sparc_debug.inc

create table t1 (a int not null);

# The query may or may not fail with an error; the point is that the server should keep on running.
--disable_result_log
--disable_abort_on_error
prepare s1 from '
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( 
  select a from t1 where a in ( select a from t1) 
  )))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))';
execute s1;
--enable_abort_on_error
--enable_result_log

drop table t1;
