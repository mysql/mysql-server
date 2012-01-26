# ==== Purpose ====
# 
# Several test primitives from mysql-test/extra/rpl_tests 
# are shared for test cases for MyISAM, InnoDB, NDB and
# other engines.
# For NDB engine all events will be added by NDB injector
# so tests only can continue after injector is ready, 
# this test waits for proper injector thread state.
#
# ==== Usage ====
#
# let $engine_type= NDB;
# --source include/wait_for_ndb_to_binlog.inc
#
# ==== Parameters =====
#
# $engine_type
#   Type of engine. If type is NDB then it waits for injector
#   thread proper state.

if (`SELECT UPPER(LEFT('$engine_type',3)) = 'NDB'`) {
  let $show_statement= SHOW PROCESSLIST;
  let $field= State;
  let $condition= = 'Waiting for event from ndbcluster';
  source include/wait_show_condition.inc;
}
