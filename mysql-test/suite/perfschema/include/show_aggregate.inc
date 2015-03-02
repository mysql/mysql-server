############### suite/perfschema/include/show_aggregate.inc ####################
#                                                                              #
# Gather status by thread, by user, by host, by account and global.            #
#                                                                              #
################################################################################

--echo #=================
--echo # Global results
--echo #=================
USE test;
UPDATE test.status_results sr, performance_schema.global_status sg
  SET sr.stop = sg.variable_value
  WHERE sr.variable_name = sg.variable_name
    AND sg.variable_name IN ('handler_delete', 'handler_rollback');
--echo
--echo # Global deltas: END - START.
UPDATE test.status_results sr
  SET sr.delta = sr.stop - sr.start;

#--echo DEBUG
#SELECT * FROM test.status_results;
#SELECT * from performance_schema.global_status where variable_name in ('handler_delete', 'handler_rollback');

--echo #=================
--echo # Status by thread
--echo #=================
--echo # Thread results from CON1.
UPDATE test.status_results sr, performance_schema.status_by_thread sbt
  SET sr.t1 = sbt.variable_value
  WHERE sr.variable_name = sbt.variable_name
    AND sbt.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbt.thread_id = @con1_id;
--echo
--echo # Thread results from CON2.
UPDATE test.status_results sr, performance_schema.status_by_thread sbt
  SET sr.t2 = sbt.variable_value
  WHERE sr.variable_name = sbt.variable_name
    AND sbt.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbt.thread_id = @con2_id;
--echo
--echo # Thread results from CON3.
UPDATE test.status_results sr, performance_schema.status_by_thread sbt
  SET sr.t3 = sbt.variable_value
  WHERE sr.variable_name = sbt.variable_name
    AND sbt.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbt.thread_id = @con3_id;
--echo
--echo # Thread totals for 3 connections.
UPDATE test.status_results sr
  SET sr.thread = sr.t1 + sr.t2 + sr.t3;

--echo #=================
--echo # Status by user
--echo #=================
--echo # User1
UPDATE test.status_results sr, performance_schema.status_by_user sbu
  SET sr.u1 = sbu.variable_value
  WHERE sr.variable_name = sbu.variable_name
    AND sbu.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbu.user IN ('user1');
--echo
--echo # User2
UPDATE test.status_results sr, performance_schema.status_by_user sbu
  SET sr.u2 = sbu.variable_value
  WHERE sr.variable_name = sbu.variable_name
    AND sbu.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbu.user IN ('user2');
--echo
--echo # User3
UPDATE test.status_results sr, performance_schema.status_by_user sbu
  SET sr.u3 = sbu.variable_value
  WHERE sr.variable_name = sbu.variable_name
    AND sbu.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbu.user IN ('user3');
--echo
--echo # Status totals for 3 users.
UPDATE test.status_results sr
  SET sr.user = sr.u1 + sr.u2 + sr.u3;

--echo #===========================
--echo # Status by host (localhost)
--echo #===========================
--echo
--echo # host1 = localhost
UPDATE test.status_results sr, performance_schema.status_by_host sbh
  SET sr.h1 = sbh.variable_value
  WHERE sr.variable_name = sbh.variable_name
    AND sbh.variable_name IN ('handler_delete', 'handler_rollback')
    AND sbh.host IN ('localhost');
--echo
--echo # Status totals for 'localhost' only.
UPDATE test.status_results sr
  SET sr.host = sr.h1 + sr.h2 + sr.h3;

--echo #==================
--echo # Status by account
--echo #==================
--echo # User1@localhost
UPDATE test.status_results sr, performance_schema.status_by_account sba
  SET sr.a1 = sba.variable_value
  WHERE sr.variable_name = sba.variable_name
    AND sba.variable_name IN ('handler_delete', 'handler_rollback')
    AND sba.user IN ('user1');
--echo
--echo # User2@localhost
UPDATE test.status_results sr, performance_schema.status_by_account sba
  SET sr.a2 = sba.variable_value
  WHERE sr.variable_name = sba.variable_name
    AND sba.variable_name IN ('handler_delete', 'handler_rollback')
    AND sba.user IN ('user2');
--echo
--echo # User3@localhost
UPDATE test.status_results sr, performance_schema.status_by_account sba
  SET sr.a3 = sba.variable_value
  WHERE sr.variable_name = sba.variable_name
    AND sba.variable_name IN ('handler_delete', 'handler_rollback')
    AND sba.user IN ('user3');
--echo
--echo
--echo # Status totals for 3 accounts.
UPDATE test.status_results sr
  SET sr.acct = sr.a1 + sr.a2 + sr.a3;

#--echo DEBUG
#SELECT * FROM test.status_results;
