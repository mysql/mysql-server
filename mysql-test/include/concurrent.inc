# include/concurrent.inc
#
# Concurrent tests for transactional storage engines, mainly in UPDATE's
# Bug#3300
# Designed and tested by Sinisa Milivojevic, sinisa@mysql.com
#
# These variables have to be set before sourcing this script:
#   TRANSACTION ISOLATION LEVEL      REPEATABLE READ or READ COMMITTED
#   $engine_type                     storage engine to be tested
#
# Last update:
# 2009-02-13 HH "Release_lock("hello")" is now also successful when delivering NULL,
#               replaced two sleeps by wait_condition. The last two "sleep 1" have not been
#               replaced as all tried wait conditions leaded to nondeterministic results, especially
#               to succeeding concurrent updates. To replace the sleeps there should be some time
#               planned (or internal knowledge of the server may help).
# 2006-08-02 ML test refactored
#               old name was t/innodb_concurrent.test
#               main code went into include/concurrent.inc
# 2008-06-03 KP test refactored; removed name locks, added comments.
#               renamed wrapper t/concurrent_innodb.test ->
#                           t/concurrent_innodb_unsafelog.test
#               new wrapper t/concurrent_innodb_safelog.test
#

connection default;
#
# Show prerequisites for this test.
#
SELECT @@global.transaction_isolation;
#
# Semi-consistent read is done only for isolation levels READ
# COMMITTED and READ UNCOMMITTED.  The variable $keep_locks is
# false if semi-consistent read is done.
#
let $keep_locks= `SELECT @@global.transaction_isolation = 'REPEATABLE-READ'`;

--echo # keep_locks == $keep_locks

#
# Set up privileges and remove user level locks, if exist.
#
CREATE USER mysqltest@localhost;

#
# Preparatory cleanup.
#
--disable_warnings
drop table if exists t1;
--enable_warnings


--echo
--echo **
--echo ** two UPDATE's running and both changing distinct result sets
--echo **
  --echo ** connection thread1
  connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
  --echo ** Get user level lock (ULL) for thread 1
  select get_lock("hello",10);

      --echo ** connection thread2
      connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Start transaction for thread 2
      begin;
      --echo ** Update will cause a table scan and a new ULL  will
      --echo ** be created and blocked on the first row where tipo=11.
      send update t1 set eta=1+get_lock("hello",10)*0 where tipo=11;

  --echo ** connection thread1
  connection thread1;
  let $wait_condition= select count(*)= 1 from information_schema.processlist where state= 'User lock';
  --source include/wait_condition.inc
  --echo ** Start new transaction for thread 1
  begin;
  --echo ** Update on t1 will cause a table scan which will be blocked because
  --echo ** the previously initiated table scan applied exclusive key locks on
  --echo ** all primary keys.
  --echo ** Not so if isolation level is READ COMMITTED or READ UNCOMMITTED.
  --echo ** The locks that do not match the WHERE condition are released.
  if ($keep_locks)
  {
    --error ER_LOCK_WAIT_TIMEOUT
    update t1 set eta=2 where tipo=22;
  }
  if (!$keep_locks)
  {
    update t1 set eta=2 where tipo=22;
  }
  --echo ** Release user level name lock from thread 1. This will cause the ULL
  --echo ** on thread 2 to end its wait.
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically) the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
  DO release_lock("hello");
  --echo ** Table is now updated with a new eta on tipo=22 for thread 1.
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** Release the lock and collect result from update on thread 2
      reap;
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically) the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
      DO release_lock("hello");
      --echo ** Table should have eta updates where tipo=11 but updates made by
      --echo ** thread 1 shouldn't be visible yet.
      select * from t1;
      --echo ** Sending commit on thread 2.
      commit;

  --echo ** connection thread1
  connection thread1;
  --echo ** Make sure table reads didn't change yet on thread 1.
  select * from t1;
  --echo ** And send final commit on thread 1.
  commit;
  --echo ** Table should now be updated by both updates in the order of
  --echo ** thread 1,2.
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** Make sure the output is similar for t1.
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** two UPDATE's running and one changing result set
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
  --echo ** Get ULL "hello" on thread 1
  select get_lock("hello",10);

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Start transaction on thread 2
      begin;
      --echo ** Update will cause a table scan.
      --echo ** This will cause a hang on the first row where tipo=1 until the
      --echo ** blocking ULL is released.
      send update t1 set eta=1+get_lock("hello",10)*0 where tipo=1;

--echo ** connection thread1
  connection thread1;
  let $wait_condition= select count(*)= 1 from information_schema.processlist where state= 'User lock';
  --source include/wait_condition.inc
  --echo ** Start transaction on thread 1
  begin;
  --echo ** Update on t1 will cause a table scan which will be blocked because
  --echo ** the previously initiated table scan applied exclusive key locks on
  --echo ** all primary keys.
  --echo ** Not so if isolation level is READ COMMITTED or READ UNCOMMITTED.
  --echo ** The locks that do not match the WHERE condition are released.
  if ($keep_locks)
  {
    --error ER_LOCK_WAIT_TIMEOUT
    update t1 set tipo=1 where tipo=2;
  }
  if (!$keep_locks)
  {
    update t1 set tipo=1 where tipo=2;
  }
  --echo ** Release ULL. This will release the next waiting ULL on thread 2.
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically)the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
  DO release_lock("hello");
  --echo ** The table should still be updated with updates for thread 1 only:
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** Release the lock and collect result from thread 2:
      reap;
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically) the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
      DO release_lock("hello");
      --echo ** Seen from thread 2 the table should have been updated on four
      --echo ** places.
      select * from t1;
      commit;

  --echo ** connection thread1
  connection thread1;
  --echo ** Thread 2 has committed but the result should remain the same for
  --echo ** thread 1 (updated on three places):
  select * from t1;
  commit;
  --echo ** After a commit the table should be merged with the previous
  --echo ** commit.
  --echo ** This select should show both updates:
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** One UPDATE and one INSERT .... Monty's test
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1 (a int not null, b int not null);
  insert into t1 values (1,1),(2,1),(3,1),(4,1);
  --echo ** Create ULL 'hello2'
  select get_lock("hello2",10);

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Begin a new transaction on thread 2
      begin;
      --echo ** Update will create a table scan which creates a ULL where a=2;
      --echo ** this will hang waiting on thread 1.
      send update t1 set b=10+get_lock(concat("hello",a),10)*0 where a=2;

  --echo ** connection thread1
  connection thread1;
  let $wait_condition= select count(*)= 1 from information_schema.processlist where state= 'User lock';
  --source include/wait_condition.inc
  --echo ** Insert new values to t1 from thread 1; this created an implicit
  --echo ** commit since there are no on-going transactions.
  insert into t1 values (1,1);
  --echo ** Release the ULL (thread 2 updates will finish).
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically) the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
  DO release_lock("hello2");
  --echo ** ..but thread 1 will still see t1 as if nothing has happend:
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** Collect results from thread 2 and release the lock.
      reap;
# Due to Bug#32782 User lock hash fails to find lock, which probably also cause Bug#39484 (main.concurrent_innodb_safelog fails sporadically) the success of the following
# is also guaranteed for NULL. Replaced SELECT by DO (no result).
      DO release_lock("hello2");
      --echo ** The table should look like the original+updates for thread 2,
      --echo ** and consist of new rows:
      select * from t1;
      --echo ** Commit changes from thread 2
      commit;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** one UPDATE changing result set and SELECT ... FOR UPDATE
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Begin a new transaction on thread 2
      begin;
      --echo ** Select a range for update.
      select * from t1 where tipo=2 FOR UPDATE;

  --echo ** connection thread1
  connection thread1;
  --echo ** Begin a new transaction on thread 1
  begin;
  --echo ** Update the same range which is marked for update on thread 2; this
  --echo ** will hang because of row locks.
  --error ER_LOCK_WAIT_TIMEOUT
  update t1 set tipo=1 where tipo=2;
  --echo ** After the update the table will be unmodified because the previous
  --echo ** transaction failed and was rolled back.
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** The table should look unmodified from thread 2.
      select * from t1;
      --echo ** Sending a commit should release the row locks and enable
      --echo ** thread 1 to complete the transaction.
      commit;

  --echo ** connection thread1
  connection thread1;
  --echo ** Commit on thread 1.
  commit;

      --echo ** connection thread2
      connection thread2;
      --echo ** The table should not have been changed.
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  --echo ** Even on thread 1:
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** one UPDATE not changing result set and SELECT ... FOR UPDATE
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Starting new transaction on thread 2.
      begin;
      --echo ** Starting SELECT .. FOR UPDATE
      select * from t1 where tipo=2 FOR UPDATE;

  --echo ** connection thread1
  connection thread1;
  --echo
  --echo ** Starting new transaction on thread 1
  begin;
  --echo ** Updating single row using a table scan. This will time out
  --echo ** because of ongoing transaction on thread 1 holding lock on
  --echo ** all primary keys in the scan.
  --echo ** Not so if isolation level is READ COMMITTED or READ UNCOMMITTED.
  --echo ** The locks that do not match the WHERE condition are released.
  if ($keep_locks)
  {
    --error ER_LOCK_WAIT_TIMEOUT
    update t1 set tipo=11 where tipo=22;
  }
  if (!$keep_locks)
  {
    update t1 set tipo=11 where tipo=22;
  }
  --echo ** After the time out the transaction is aborted; no rows should
  --echo ** have changed.
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      --echo ** The same thing should hold true for the transaction on
      --echo ** thread 2
      select * from t1;
      send commit;

  --echo ** connection thread1
  connection thread1;
  commit;

      --echo ** connection thread2
      connection thread2;
      --echo ** Even after committing:
      reap;
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** two SELECT ... FOR UPDATE
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      --echo ** Begin a new transaction on thread 2
      begin;
      select * from t1 where tipo=2 FOR UPDATE;

  --echo ** connection thread1
  connection thread1;
  --echo ** Begin a new transaction on thread 1
  begin;
  --echo ** Selecting a range for update by table scan will be blocked
  --echo ** because of on-going transaction on thread 2.
  --error ER_LOCK_WAIT_TIMEOUT
  select * from t1 where tipo=1 FOR UPDATE;

      --echo ** connection thread2
      connection thread2;
      --echo ** Table will be unchanged and the select command will not be
      --echo ** blocked:
      select * from t1;
      --echo ** Commit transacton on thread 2.
      commit;

  --echo ** connection thread1
  connection thread1;
  --echo ** Commit transaction on thread 1.
  commit;

      --echo ** connection thread2
      connection thread2;
      --echo ** Make sure table isn't blocked on thread 2:
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  --echo ** Make sure table isn't blocked on thread 1:
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** one UPDATE changing result set and DELETE
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      begin;
      send delete from t1 where tipo=2;
# The sleep has not been replaced as all tried wait conditions leaded to sporadically
# succeding update in the following thread. Also the used status variables '%lock%' and
# 'innodb_deleted_rows' and infos in processlist where not sucessful.
      sleep 1;

  --echo ** connection thread1
  connection thread1;
  begin;
  --error ER_LOCK_WAIT_TIMEOUT
  update t1 set tipo=1 where tipo=2;
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      reap;
      select * from t1;
      send commit;

  --echo ** connection thread1
  connection thread1;
  commit;

      --echo ** connection thread2
      connection thread2;
      reap;
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  select * from t1;

--echo ** connection default
connection default;
drop table t1;


--echo
--echo **
--echo ** one UPDATE not changing result set and DELETE
--echo **
  --echo ** connection thread1
  #connect (thread1, localhost, mysqltest,,);
  connection thread1;
  --echo ** Set up table
  eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
  create table t1(eta int(11) not null, tipo int(11), c varchar(255));
  insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
  insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
  insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
  insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
  insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
  insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
  insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");

      --echo ** connection thread2
      #connect (thread2, localhost, mysqltest,,);
      connection thread2;
      begin;
      send delete from t1 where tipo=2;
# The sleep has not been replaced as all tried wait conditions leaded to sporadically
# succeding update in the following thread. Also the used status variables '%lock%' and
# 'innodb_deleted_rows' and infos in processlist where not sucessful.
      sleep 1;

  --echo ** connection thread1
  connection thread1;
  begin;
  --echo ** Update on t1 will cause a table scan which will be blocked because
  --echo ** the previously initiated table scan applied exclusive key locks on
  --echo ** all primary keys.
  --echo ** Not so if isolation level is READ COMMITTED or READ UNCOMMITTED.
  --echo ** The locks that do not match the WHERE condition are released.
  if ($keep_locks)
  {
    --error ER_LOCK_WAIT_TIMEOUT
    update t1 set tipo=1 where tipo=22;
  }
  if (!$keep_locks)
  {
    update t1 set tipo=1 where tipo=22;
  }
  select * from t1;

      --echo ** connection thread2
      connection thread2;
      reap;
      select * from t1;
      send commit;

  --echo ** connection thread1
  connection thread1;
  commit;

      --echo ** connection thread2
      connection thread2;
      reap;
      select * from t1;

  --echo ** connection thread1
  connection thread1;
  select * from t1;

--echo ** Cleanup
connection thread1;
disconnect thread1;
--source include/wait_until_disconnected.inc
--echo ** connection thread2
connection thread2;
disconnect thread2;
--source include/wait_until_disconnected.inc
--echo ** connection default
connection default;
drop table t1;
drop user mysqltest@localhost;

