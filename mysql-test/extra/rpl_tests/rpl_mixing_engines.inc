################################################################################
# This is an auxiliary file used by rpl_mixing_engines.test, and that it
# executes SQL statements according to a format string, as specified in
# rpl_mixing_engines.test. In addition, it accepts the special format
# strings 'configure' and 'clean', used before and after everything else.
################################################################################

if (`SELECT HEX(@commands) = HEX('configure')`)
{
  connection master;

  SET SQL_LOG_BIN=0;
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
  eval SET SQL_LOG_BIN=1;

  connection slave;

  SET SQL_LOG_BIN=0;
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
  SET SQL_LOG_BIN=1;

  connection master;

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
  let $commands= '';

  SET @commands= '';
}

if (`SELECT HEX(@commands) = HEX('clean')`)
{
  connection master;

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

  sync_slave_with_master;

  SET @commands= '';
}

while (`SELECT HEX(@commands) != HEX('')`)
{
  --disable_query_log
  SET @command= SUBSTRING_INDEX(@commands, ' ', 1);
  let $command= `SELECT @command`;
  --eval SET @check_commands= '$commands'
  if (`SELECT HEX(@check_commands) = HEX('''')`)
  {
    let $commands= `SELECT @commands`;
  }
  --echo -b-b-b-b-b-b-b-b-b-b-b- >> $command << -b-b-b-b-b-b-b-b-b-b-b-
  let $pos_command= query_get_value("SHOW MASTER STATUS", Position, 1);
  --enable_query_log
  if (`SELECT HEX(@command) = HEX('B')`)
  {
    eval BEGIN;
  }
  if (`SELECT HEX(@command) = HEX('T')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('T-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('T-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval SELECT fc_i_tt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('T-proc')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval CALL pc_i_tt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('eT')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Te')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Te-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Te-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_tt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('N')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('N-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('N-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval SELECT fc_i_nt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('N-proc')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval CALL pc_i_nt_5_suc ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('eN')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Ne')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Ne-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_5(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('Ne-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_nt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('tN')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM tt_1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('tNe')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM tt_1 UNION SELECT $old_trans_id, $old_stmt_id, COUNT(*) FROM tt_1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('nT')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM nt_1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('nTe')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_1`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_1 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_1(trans_id, stmt_id, info) SELECT $trans_id, $stmt_id, COUNT(*) FROM nt_1 UNION SELECT $old_trans_id, $old_stmt_id, COUNT(*) FROM nt_1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('NT')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval UPDATE nt_3, tt_3 SET nt_3.info= "new text $trans_id --> $stmt_id", tt_3.info= "new text $trans_id --> $stmt_id" where nt_3.trans_id = tt_3.trans_id and tt_3.trans_id = 1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('NT-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_4(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('NT-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO nt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, fc_i_tt_5_suc($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('NeT-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_4`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_4 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_4(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('NeT-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from nt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from nt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO nt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_tt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('TN')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval UPDATE tt_4, nt_4 SET tt_4.info= "new text $trans_id --> $stmt_id", nt_4.info= "new text $trans_id --> $stmt_id" where nt_4.trans_id = tt_4.trans_id and tt_4.trans_id = 1;
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('TN-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_3(trans_id, stmt_id) VALUES ($trans_id, $stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('TN-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    eval INSERT INTO tt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, fc_i_nt_5_suc($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('TeN-trig')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_3`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_3 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_3(trans_id, stmt_id) VALUES ($trans_id, $stmt_id), ($old_trans_id, $old_stmt_id);
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('TeN-func')`)
  {
    #--echo DEBUG-- (trans_id, stmt_id) --> ($trans_id, $stmt_id)
    let $old_trans_id= `SELECT max(trans_id) from tt_5`;
    let $old_stmt_id= `SELECT max(stmt_id) from tt_5 where trans_id= $old_trans_id`;
    --error ER_DUP_ENTRY, ER_DUP_KEY
    eval INSERT INTO tt_5(trans_id, stmt_id, info) VALUES ($trans_id, $stmt_id, ''), ($old_trans_id, $old_stmt_id, fc_i_nt_5_suc ($trans_id, $stmt_id));
    inc $stmt_id;
  }
  if (`SELECT HEX(@command) = HEX('CS-T->T')`)
  {
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=$engine_type SELECT * FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('CS-N->N')`)
  {
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=MyIsam SELECT * FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('CS-T->N')`)
  {
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=$engine_type SELECT * FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('CS-N->T')`)
  {
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY(trans_id, stmt_id)) engine=MyIsam SELECT * FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('CSe-T->T')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=$engine_type SELECT stmt_id FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('CSe-N->N')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=MyIsam SELECT stmt_id FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('CSe-T->N')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE tt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=$engine_type SELECT stmt_id FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('CSe-N->T')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval CREATE TABLE nt_xx_$tb_id (PRIMARY KEY (stmt_id)) engine=MyIsam SELECT stmt_id FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('CT')`)
  {
    --eval CREATE TEMPORARY TABLE tt_xx_$tb_id (a int) engine=$engine_type;
  }
  if (`SELECT HEX(@command) = HEX('IS-T<-N')`)
  {
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('ISe-T<-N')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('IS-N<-T')`)
  {
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('ISe-N<-T')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('IS-T<-T')`)
  {
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('ISe-T<-T')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO tt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM tt_1;
  }
  if (`SELECT HEX(@command) = HEX('IS-N<-N')`)
  {
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, stmt_id, USER() FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('ISe-N<-N')`)
  {
    --error ER_DUP_ENTRY, ER_DUP_KEY
    --eval INSERT INTO nt_xx_$tb_id(trans_id, stmt_id, info) SELECT trans_id, trans_id, USER() FROM nt_1;
  }
  if (`SELECT HEX(@command) = HEX('trunc-CS-T')`)
  {
    eval TRUNCATE TABLE tt_xx_$tb_id;
  }
  if (`SELECT HEX(@command) = HEX('trunc-CS-N')`)
  {
    eval TRUNCATE TABLE nt_xx_$tb_id;
  }
  if (`SELECT HEX(@command) = HEX('trunc-CT')`)
  {
    eval TRUNCATE TABLE tt_xx_$tb_id;
  }
  if (`SELECT HEX(@command) = HEX('drop-CS')`)
  {
    --disable_warnings
    eval DROP TABLE IF EXISTS tt_xx_$tb_id, nt_xx_$tb_id;
    inc $tb_id;
    --enable_warnings
  }
  if (`SELECT HEX(@command) = HEX('drop-CT')`)
  {
    --disable_warnings
    eval DROP TEMPORARY TABLE IF EXISTS tt_xx_$tb_id;
    inc $tb_id;
    --enable_warnings
  }
  if (`SELECT HEX(@command) = HEX('C')`)
  {
    --error 0, ER_GET_ERRMSG
    eval COMMIT;
  }
  if (`SELECT HEX(@command) = HEX('R')`)
  {
    --error 0, ER_GET_ERRMSG
    eval ROLLBACK;
  }
  if (`SELECT HEX(@command) = HEX('S1')`)
  {
    eval SAVEPOINT s1; 
  }
  if (`SELECT HEX(@command) = HEX('R1')`)
  {
    eval ROLLBACK TO s1; 
  }
  --disable_query_log
  SET @commands= LTRIM(SUBSTRING(@commands, LENGTH(@command) + 1));
  inc $stmt_id;

  let $binlog_start= $pos_command;
  --source include/show_binlog_events.inc
  --echo -e-e-e-e-e-e-e-e-e-e-e- >> $command << -e-e-e-e-e-e-e-e-e-e-e-
  if (`SELECT HEX(@commands) = HEX('')`)
  {
    let $binlog_start= $pos_trans_command;
    --echo -b-b-b-b-b-b-b-b-b-b-b- >> $commands << -b-b-b-b-b-b-b-b-b-b-b-
    --source include/show_binlog_events.inc
    --echo -e-e-e-e-e-e-e-e-e-e-e- >> $commands << -e-e-e-e-e-e-e-e-e-e-e-
    --echo
    let $pos_trans_command= query_get_value("SHOW MASTER STATUS", Position, 1);
    let $stmt_id= 1;
    inc $trans_id;
    let $commands= '';
  }
}
