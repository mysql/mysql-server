################################################################################
# This is an auxillary file used by rpl_multiple_trigger.test and
# gr_multiple_trigger.test to test multiple triggers with GR and rpl.
#
# Steps:
# 1. Create tables and a trigger t1_b on master.
# 2. Check that action_order attribute of trigger t1_b is same on both servers.
# 3. Create another trigger t1_a on same table for same action event
#    and timing.
# 4. Check that CREATED, ACTION_ORDER attributes are same on both servers.
# 5. Insert some data to the table on master.
# 6. Ensure that data is consistent on both the servers.
# 7. Clean-up
#
# Usage:
# --let $master= server1
# --let $slave = server2
# --source common/rpl/multiple_trigger.inc
#
# $master - master server.
# $slave - slave server.
################################################################################

--echo
--echo #1. Create tables and a trigger t1_b on master.
--let $rpl_connection_name= $master
--source include/connection.inc
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT PRIMARY KEY, b INT DEFAULT 0);

DELIMITER |;
create trigger t1_b
  after insert on t1
    for each row
    BEGIN
       INSERT INTO t2 VALUES (New.a, @value1);
    END|
DELIMITER ;|
--source include/rpl/sync.inc

--echo
--echo #2. Check that action_order attribute of trigger t1_b is same on both servers.
--let $assert_text= action_order for trigger t1_b must be 1.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_b"]=1;
--source include/assert.inc

--let $rpl_connection_name= $slave
--source include/connection.inc
--let $assert_text= action_order for trigger t1_b must be 1.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_b"]=1;
--source include/assert.inc

--let $rpl_connection_name= $master
--source include/connection.inc

--echo
--echo #3. Create another trigger t1_a on t1 for same action event and timing.
DELIMITER |;
create trigger t1_a
  after insert on t1
    for each row PRECEDES t1_b
    BEGIN
       SET @value1:= New.a * 2;
    END|
DELIMITER ;|
--source include/rpl/sync.inc

# Save CREATED attribute on master.
--let $t1a_created1 = `SELECT CREATED FROM information_schema.triggers WHERE trigger_schema='test' AND EVENT_OBJECT_TABLE='t1' AND trigger_name='t1_a'`

--echo
--echo #4. Check that CREATED, ACTION_ORDER attributes are same on both servers.
--let $assert_text= action_order for trigger t1_a must be 1.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_a"]=1;
--source include/assert.inc

--let $assert_text= action_order for trigger t1_b must be 2.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_b"]=2;
--source include/assert.inc

--let $rpl_connection_name= $slave
--source include/connection.inc
--let $assert_text= action_order for trigger t1_a must be 1.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_a"]=1;
--source include/assert.inc

--let $assert_text= action_order for trigger t1_b must be 2.
--let $assert_cond= [SELECT ACTION_ORDER FROM information_schema.triggers WHERE trigger_schema="test" AND EVENT_OBJECT_TABLE="t1" AND trigger_name="t1_b"]=2;
--source include/assert.inc

--let $t1a_created2 = `SELECT CREATED FROM information_schema.triggers WHERE trigger_schema='test' AND EVENT_OBJECT_TABLE='t1' AND trigger_name='t1_a'`

# Check that both servers has same CREATED attribute.
--let $assert_text= Created attribute for a trigger must be same on both the servers
--let $assert_cond= "$t1a_created2"="$t1a_created1"
--source include/assert.inc

--echo
--echo #5. Insert some data to the table on master.
--let $rpl_connection_name= $master
--source include/connection.inc
INSERT INTO t1 (a) values (1),(2);

# Delete trigger t1_b
DROP TRIGGER test.t1_b;

INSERT INTO t1 (a) values (3);

--echo
--echo #6. Ensure that data is consistent on both the servers.
--source include/rpl/sync.inc
--let $diff_tables=$master:t1,$slave:t1
--source include/diff_tables.inc

--let $diff_tables=$master:t2,$slave:t2
--source include/diff_tables.inc

--echo
--echo #7. Clean-up
DROP TRIGGER test.t1_a;
DROP TABLE t1;
DROP TABLE t2;
