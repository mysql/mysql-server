# Script to be sourced by suite/innodb/t/innodb_tsof_use_1.test
--disable_query_log
--let $p= # Generate entries in tablespaces.open.* files and also log
--let $p= $p by running DDL+DML. The printing of statements is disabled.
--echo $p
inc $flip_count;
let $next_ts= `SELECT IF(MOD($flip_count,2) = 0,
                             'ts1', 'innodb_file_per_table')`;
let $num= $max_num;
while($num)
{
   eval
   ALTER TABLE db_test_a.t_aux_$num TABLESPACE = $next_ts;
   BEGIN;
   eval
   UPDATE db_test_a.t_aux SET col2 = CONCAT('$flip_count-', REPEAT('z', 90))
   WHERE col1 = $num;
   dec $num;
}
ROLLBACK;
--enable_query_log
