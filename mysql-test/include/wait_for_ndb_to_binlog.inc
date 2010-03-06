# ==== Purpose ====
# 
#   Several test primitives from mysql-test/extra/rpl_tests 
# shared for test cases for MyISAM, InnoDB, NDB and other 
# engines. But for NDB all events will be added by NDB 
# injector and now there are no way to detect the state of 
# NDB injector therefore this primitive waits 5 sec 
# if engine type is NDB. 
#   In future that should be fixed by waiting of proper 
# state of NDB injector. 
#
# ==== Usage ====
#
# let $engine_type= NDB;
# --source include/wait_for_ndb_to_binlog.inc
#
# ==== Parameters =====
#
# $engine_type
#   Type of engine. If type is NDB then it waits $wait_time sec 
#   
# $wait_time
#   Test will wait $wait_time seconds

let $_wait_time= 5;

if (!$wait_time) {
  let $_wait_time= $wait_time;
}

if (`SELECT UPPER(LEFT('$engine_type',3)) = 'NDB'`) {
  while (!$_wait_time) {
    let $_wait_time_internal= 10;
    while (!$_wait_time_internal) {
      sleep 0.1;
      dec $_wait_time_internal;
    }
    dec $_wait_time;
  }
}

