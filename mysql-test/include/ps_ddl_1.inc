# include/ps_ddl_1.inc 
#
# Auxiliary script to be used in ps_ddl.test
#

prepare stmt_sf from 'select f_12093();';
prepare stmt_sp from 'call p_12093(f_12093())';
execute stmt_sf;
execute stmt_sp;

connection con1;
eval $my_drop;
#
connection default;
--echo # XXX: used to be a bug
execute stmt_sf;
--echo # XXX: used to be a bug
execute stmt_sp;
#
--echo # XXX: used to be a bug
execute stmt_sf;
--echo # XXX: used to be a bug
execute stmt_sp;

connection default;
