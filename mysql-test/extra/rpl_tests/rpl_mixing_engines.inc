# ==== Purpose ====
#
# This is an auxiliary file used by rpl_mixing_engines.test. It
# executes SQL statements according to a format string, as specified in
# rpl_mixing_engines.test. In addition, it accepts the special format
# strings 'configure' and 'clean', used before and after everything else.
#
# ==== Usage ====
#
# --let $commands= command1 command2 ...
# --let $database_name= name
# [--let $verbose= 1]
# --source include/rpl_mixing_engines.inc
#
#   Parameters:
#
#     $commands
#       Space-separated list of commands. See rpl_mixing_engines.test
#       for a list of all commands.
#
#     $database_name
#       Name of database to use. If $commands is 'configure' or
#       'clean', and the database name is not 'test', then the
#       database is created or dropped, respectively.
#
#     $verbose
#       By default, the query show binlog events is executed after all
#       commands have been processed, showing the result of all
#       commands. If this flag is set, then the query show binlog events
#       is also executed once after each command, showing only the result
#       of that command.
--echo rpl_mixing_engines.inc [commands=$commands]

--let $rme_initial_commands= $commands

if ($commands == 'configure')
{
  let $binlog_filter=
  /Anonymous_Gtid/Gtid/
  /GTID_NEXT= '([0-9a-f:\-]+|ANONYMOUS)'/GTID_NEXT= Gtid_set/
  /xid=[0-9]+/XID/
  /table_id: [0-9]+/table_id: #/;
  connection master;
  --disable_query_log
  if ($database_name != 'test')
  {
    --eval CREATE DATABASE $database_name
  }
  --eval use $database_name
  --enable_query_log

  eval CREATE TABLE nt_1 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE nt_2 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE nt_3 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE nt_4 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE nt_5 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE nt_6 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = MyISAM;
  eval CREATE TABLE tt_1 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;
  eval CREATE TABLE tt_2 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;
  eval CREATE TABLE tt_3 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;
  eval CREATE TABLE tt_4 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;
  eval CREATE TABLE tt_5 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;
  eval CREATE TABLE tt_6 (trans_id INT, stmt_id INT, info VARCHAR(64), PRIMARY KEY(trans_id, stmt_id)) ENGINE = $engine_type;

  INSERT INTO nt_1(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO nt_2(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO nt_3(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO nt_4(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO nt_5(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO nt_6(trans_id, stmt_id) VALUES(1,1);

  INSERT INTO tt_1(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO tt_2(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO tt_3(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO tt_4(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO tt_5(trans_id, stmt_id) VALUES(1,1);
  INSERT INTO tt_6(trans_id, stmt_id) VALUES(1,1);

  DELIMITER |;

  CREATE PROCEDURE pc_i_tt_5_suc (IN p_trans_id INTEGER, IN p_stmt_id INTEGER)
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM tt_5 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO tt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);
    INSERT INTO tt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id + 1);
  END|

  CREATE PROCEDURE pc_i_nt_5_suc (IN p_trans_id INTEGER, IN p_stmt_id INTEGER)
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM nt_5 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO nt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);
    INSERT INTO nt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id + 1);
  END|

  CREATE FUNCTION fc_i_tt_5_suc (p_trans_id INTEGER, p_stmt_id INTEGER) RETURNS VARCHAR(64)
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM tt_5 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO tt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);
    INSERT INTO tt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id + 1);
    RETURN "fc_i_tt_5_suc";
  END|

  CREATE FUNCTION fc_i_nt_5_suc (p_trans_id INTEGER, p_stmt_id INTEGER) RETURNS VARCHAR(64)
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM nt_5 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO nt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);
    INSERT INTO nt_5(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id + 1);
    RETURN "fc_i_nt_5_suc";
  END|

  CREATE FUNCTION fc_i_nt_3_tt_3_suc (p_trans_id INTEGER, p_stmt_id INTEGER) RETURNS VARCHAR(64)
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM nt_3 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO nt_3(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);

    SELECT max(stmt_id) INTO in_stmt_id FROM tt_3 WHERE trans_id= p_trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, p_stmt_id), 1) INTO in_stmt_id;
    INSERT INTO tt_3(trans_id, stmt_id) VALUES (p_trans_id, in_stmt_id);
    RETURN "fc_i_nt_3_tt_3_suc";
  END|

  CREATE TRIGGER tr_i_tt_3_to_nt_3 AFTER INSERT ON tt_3 FOR EACH ROW
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM nt_3 WHERE trans_id= NEW.trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, NEW.stmt_id), 1) INTO in_stmt_id;
    INSERT INTO nt_3(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id);
    INSERT INTO nt_3(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id + 1);
  END|

  CREATE TRIGGER tr_i_nt_4_to_tt_4 AFTER INSERT ON nt_4 FOR EACH ROW
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM tt_4 WHERE trans_id= NEW.trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, NEW.stmt_id), 1) INTO in_stmt_id;
    INSERT INTO tt_4(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id);
    INSERT INTO tt_4(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id + 1);
  END|

  CREATE TRIGGER tr_i_tt_5_to_tt_6 AFTER INSERT ON tt_5 FOR EACH ROW
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM tt_6 WHERE trans_id= NEW.trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, NEW.stmt_id, 1), 1) INTO in_stmt_id;
    INSERT INTO tt_6(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id);
    INSERT INTO tt_6(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id + 1);
  END|

  CREATE TRIGGER tr_i_nt_5_to_nt_6 AFTER INSERT ON nt_5 FOR EACH ROW
  BEGIN
    DECLARE in_stmt_id INTEGER;
    SELECT max(stmt_id) INTO in_stmt_id FROM nt_6 WHERE trans_id= NEW.trans_id;
    SELECT COALESCE(greatest(in_stmt_id + 1, NEW.stmt_id), 1) INTO in_stmt_id;
    INSERT INTO nt_6(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id);
    INSERT INTO nt_6(trans_id, stmt_id) VALUES (NEW.trans_id, in_stmt_id + 1);
  END|

  DELIMITER ;|

  let $pos_trans_command= query_get_value("SHOW MASTER STATUS", Position, 1);

  let $trans_id= 7;
  let $tb_id= 1;
  let $stmt_id= 1;
  --let $commands=
  let $sn= 0;
}

if ($commands == 'clean')
{
  connection master;

  --disable_query_log
  --eval use $database_name
  --enable_query_log

  DROP TABLE tt_1;
  DROP TABLE tt_2;
  DROP TABLE tt_3;
  DROP TABLE tt_4;
  DROP TABLE tt_5;
  DROP TABLE tt_6;

  DROP TABLE nt_1;
  DROP TABLE nt_2;
  DROP TABLE nt_3;
  DROP TABLE nt_4;
  DROP TABLE nt_5;
  DROP TABLE nt_6;

  DROP PROCEDURE pc_i_tt_5_suc;
  DROP PROCEDURE pc_i_nt_5_suc;
  DROP FUNCTION fc_i_tt_5_suc;
  DROP FUNCTION fc_i_nt_5_suc;
  DROP FUNCTION fc_i_nt_3_tt_3_suc;

  --disable_query_log
  if ($database_name != 'test')
  {
    --eval DROP DATABASE $database_name
  }
  --enable_query_log

  sync_slave_with_master;

  --let $commands=
}

while ($commands != '')
{
  --disable_query_log
  --eval use $database_name
  --enable_query_log

  --disable_query_log
  --let $command= `SELECT SUBSTRING_INDEX('$commands', ' ', 1)`
  if ($verbose)
  {
    --echo -b-b-b-b-b-b-b-b-b-b-b- >> $command << -b-b-b-b-b-b-b-b-b-b-b-
    let $pos_command= query_get_value("SHOW MASTER STATUS", Position, 1);
  }
  --enable_query_log
  if ($command == 'B')
  {
    eval BEGIN;
  }
  if ($command == 'T')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'T-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'T-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval SELECT fc_i_tt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'T-proc')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval CALL pc_i_tt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'eT')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Te')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Te-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Te-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_tt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'N')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'N-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'N-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval SELECT fc_i_nt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'N-proc')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval CALL pc_i_nt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'eN')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Ne')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Ne-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'Ne-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_nt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'tN')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM tt_1;
    inc $stmt_id;
  }
  if ($command == 'tNe')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM tt_1 UNION SELECT $old_trans_id, $old_stmt_id, COUNT(*) FROM tt_1;
    inc $stmt_id;
  }
  if ($command == 'nT')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM nt_1;
    inc $stmt_id;
  }
  if ($command == 'nTe')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM nt_1 UNION SELECT $old_trans_id, $old_stmt_id, COUNT(*) FROM nt_1;
    inc $stmt_id;
  }
  if ($command == 'NT')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval UPDATE nt_3, tt_3 SET nt_3.info= "new text $trans_id --> $stmt_id", tt_3.info= "new text $trans_id --> $stmt_id" where nt_3.trans_id = tt_3.trans_id and tt_3.trans_id = 1;
    inc $stmt_id;
  }
  if ($command == 'NT-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_4(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'NT-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, fc_i_tt_5_suc($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'NeT-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_4`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_4 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_4(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'NeT-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_tt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'TN')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval UPDATE tt_4, nt_4 SET tt_4.info= "new text $trans_id --> $stmt_id", nt_4.info= "new text $trans_id --> $stmt_id" where nt_4.trans_id = tt_4.trans_id and tt_4.trans_id = 1;
    inc $stmt_id;
  }
  if ($command == 'TN-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_3(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if ($command == 'TN-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, fc_i_nt_5_suc($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'TeN-trig')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_3`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_3 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_3(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if ($command == 'TeN-func')
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_nt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if ($command == 'set-T')
  {
    --eval SET @var= fc_i_tt_5_suc($trans_id, $stmt_id)
    inc $stmt_id;
  }
  if ($command == 'set-N')
  {
    --eval SET @var= fc_i_nt_5_suc($trans_id, $stmt_id)
    inc $stmt_id;
  }
  if ($command == 'set-NT')
  {
    --eval SET @var= fc_i_nt_3_tt_3_suc($trans_id, $stmt_id)
    inc $stmt_id;
  }
  if ($command == 'CS-T->T')
  {
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=$engine_type SELECT * FROM tt_1;
  }
  if ($command == 'CS-N->N')
  {
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=MyIsam SELECT * FROM nt_1;
  }
  if ($command == 'CS-T->N')
  {
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=$engine_type SELECT * FROM nt_1;
  }
  if ($command == 'CS-N->T')
  {
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=MyIsam SELECT * FROM tt_1;
  }
  if ($command == 'CSe-T->T')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=$engine_type SELECT stmt_id FROM tt_1;
  }
  if ($command == 'CSe-N->N')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=MyIsam SELECT stmt_id FROM nt_1;
  }
  if ($command == 'CSe-T->N')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=$engine_type SELECT stmt_id FROM nt_1;
  }
  if ($command == 'CSe-N->T')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=MyIsam SELECT stmt_id FROM tt_1;
  }
  if ($command == 'CT')
  {
    --eval CREATE TEMPORARY TABLE tt_xx_$tb_id (a int) engine=$engine_type;
  }
  if ($command == 'IS-T<-N')
  {
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM nt_1;
  }
  if ($command == 'ISe-T<-N')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM nt_1;
  }
  if ($command == 'IS-N<-T')
  {
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM tt_1;
  }
  if ($command == 'ISe-N<-T')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM tt_1;
  }
  if ($command == 'IS-T<-T')
  {
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM tt_1;
  }
  if ($command == 'ISe-T<-T')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM tt_1;
  }
  if ($command == 'IS-N<-N')
  {
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM nt_1;
  }
  if ($command == 'ISe-N<-N')
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM nt_1;
  }
  if ($command == 'trunc-CS-T')
  {
    eval TRUNCATE TABLE tt_xx_$tb_id;
  }
  if ($command == 'trunc-CS-N')
  {
    eval TRUNCATE TABLE nt_xx_$tb_id;
  }
  if ($command == 'trunc-CT')
  {
    eval TRUNCATE TABLE tt_xx_$tb_id;
  }
  if ($command == 'drop-CS')
  {
    --disable_warnings
    eval DROP TABLE IF EXISTS tt_xx_$tb_id, nt_xx_$tb_id;
    inc $tb_id;
    --enable_warnings
  }
  if ($command == 'drop-CT')
  {
    --disable_warnings
    eval DROP TEMPORARY TABLE IF EXISTS tt_xx_$tb_id;
    inc $tb_id;
    --enable_warnings
  }
  if ($command == 'C')
  {
    --error 0, ER_GET_ERRMSG
    eval COMMIT;
  }
  if ($command == 'R')
  {
    --replace_column 2 #
    --error 0, ER_GET_ERRMSG
    eval ROLLBACK;
  }
  if ($command == 'Sn')
  {
    eval SAVEPOINT S_$sn;
    --inc $sn 
  }
  if ($command == 'Rn')
  {
    if ($sn)
    {
      --dec $sn
    }
    --replace_column 2 #
    eval ROLLBACK TO S_$sn;
  }
  if ($command == 'Cn')
  {
    if ($sn)
    {
      --dec $sn
    }
    eval RELEASE SAVEPOINT S_$sn;
  }
  --disable_query_log
  --let $commands= `SELECT LTRIM(SUBSTRING('$commands', LENGTH('$command') + 1))`
  inc $stmt_id;

  let $binlog_start= $pos_command;
  if ($verbose)
  {
    # Use other connection because query show binlog events
    # executes SELECT UUID(), which switches to row-logging when
    # binlog_format=mixed, if there are open temporary tables. This is
    # due to BUG#13692513.
    --connection server_1
    replace_column 2 # 4 # 5 #;
    --replace_regex $binlog_filter
    eval show binlog events from $binlog_start;
    --connection master
    --echo -e-e-e-e-e-e-e-e-e-e-e- >> $command << -e-e-e-e-e-e-e-e-e-e-e-
  }
  if ($commands == '')
  {
    let $binlog_start= $pos_trans_command;
    --echo -b-b-b-b-b-b-b-b-b-b-b- >> $rme_initial_commands << -b-b-b-b-b-b-b-b-b-b-b-
    --connection server_1
    replace_column 2 # 4 # 5 #;
    --replace_regex $binlog_filter
    eval show binlog events from $binlog_start;
    --connection master
    --echo -e-e-e-e-e-e-e-e-e-e-e- >> $rme_initial_commands << -e-e-e-e-e-e-e-e-e-e-e-
    --echo
    let $pos_trans_command= query_get_value("SHOW MASTER STATUS", Position, 1);
    let $stmt_id= 1;
    inc $trans_id;
  }
}

--let $commands= $rme_initial_commands
