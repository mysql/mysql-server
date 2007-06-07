##################################################
# Author: Jeb
# Date:   2007/05
# Purpose: To create a tpcb database using Disk Data, 
#          tables and stored procedures to load the database
#          and run transactions against the DB
##################################################
--disable_warnings
DROP DATABASE IF EXISTS tpcb;
--enable_warnings
CREATE DATABASE tpcb;

--echo
eval CREATE TABLE tpcb.account
                  (id INT, bid INT, balance DECIMAL(10,2),
                   filler CHAR(255), PRIMARY KEY(id))
                   TABLESPACE $table_space STORAGE DISK
                   ENGINE=$engine_type;
--echo
eval CREATE TABLE tpcb.branch
                  (bid INT, balance DECIMAL(10,2), filler VARCHAR(255),
                   PRIMARY KEY(bid))TABLESPACE $table_space STORAGE DISK
                   ENGINE=$engine_type;
--echo
eval CREATE TABLE tpcb.teller
                  (tid INT, balance DECIMAL(10,2), filler VARCHAR(255),
                   PRIMARY KEY(tid)) TABLESPACE $table_space STORAGE DISK
                   ENGINE=$engine_type;

--echo
eval CREATE TABLE tpcb.history
                  (id MEDIUMINT NOT NULL AUTO_INCREMENT,aid INT,
                   tid INT, bid INT,  amount DECIMAL(10,2),
                   tdate DATETIME, teller CHAR(20), uuidf LONGBLOB,
                   filler CHAR(80),PRIMARY KEY (id))
                   TABLESPACE $table_space STORAGE DISK
                   ENGINE=$engine_type;

--echo
--echo --- Create stored procedures & functions ---
--echo

--disable_query_log
delimiter |;
CREATE PROCEDURE tpcb.load()
BEGIN
  DECLARE acct INT DEFAULT 100;
  DECLARE brch INT DEFAULT 10;
  DECLARE tell INT DEFAULT 100;
  DECLARE tmp INT DEFAULT 10;
  WHILE brch > 0 DO
    SET tmp = 100;
    WHILE tmp > 0 DO
     INSERT INTO tpcb.account VALUES (acct, brch, 0.0, "FRESH ACCOUNT");
     SET acct = acct - 1;
     SET tmp = tmp -1;
    END WHILE;
    INSERT INTO tpcb.branch VALUES (brch, 0.0, "FRESH BRANCH");
    SET brch = brch - 1;
  END WHILE;
  WHILE tell > 0 DO
   INSERT INTO tpcb.teller VALUES (tell, 0.0, "FRESH TELLER");
   SET tell = tell - 1;
  END WHILE;
END|

CREATE FUNCTION tpcb.account_id () RETURNS INT
BEGIN
  DECLARE num INT;
  DECLARE ran INT;
  SELECT RAND() * 10 INTO ran;
  IF (ran < 5)
   THEN
     SELECT RAND() * 10 INTO num;
   ELSE
     SELECT RAND() * 100 INTO num;
   END IF;
   IF (num < 1)
    THEN
     RETURN 1;
   END IF;
  RETURN  num;
END|

CREATE FUNCTION tpcb.teller_id () RETURNS INT
BEGIN
  DECLARE num INT;
  DECLARE ran INT;
  SELECT RAND() * 10 INTO ran;
  IF (ran < 5)
   THEN
     SELECT RAND() * 10 INTO num;
   ELSE
     SELECT RAND() * 100 INTO num;
   END IF;
   IF (num < 1)
    THEN
      RETURN 1;
   END IF;
   RETURN  num;
END|

CREATE PROCEDURE tpcb.trans(in format varchar(3))
BEGIN
  DECLARE acct INT DEFAULT 0;
  DECLARE brch INT DEFAULT 0;
  DECLARE tell INT DEFAULT 0;
  DECLARE bal  DECIMAL(10,2) DEFAULT 0.0;
  DECLARE amount DECIMAL(10,2) DEFAULT 1.00;
  DECLARE test INT DEFAULT 0;
  DECLARE bbal DECIMAL(10,2) DEFAULT 0.0;
  DECLARE tbal DECIMAL(10,2) DEFAULT 0.0;
  DECLARE local_uuid VARCHAR(255);
  DECLARE local_user VARCHAR(255);
  DECLARE local_time TIMESTAMP;

  SELECT RAND() * 10 INTO test;
  SELECT tpcb.account_id() INTO acct;
  SELECT tpcb.teller_id() INTO tell;

  SELECT account.balance INTO bal FROM tpcb.account WHERE id = acct;
  SELECT account.bid INTO brch FROM tpcb.account WHERE id = acct;
  SELECT teller.balance INTO tbal FROM tpcb.teller WHERE tid = tell;
  SELECT branch.balance INTO bbal FROM tpcb.branch WHERE bid = brch;

  IF (test < 5)
   THEN
     SET bal = bal + amount;
     SET bbal = bbal + amount;
     SET tbal = tbal + amount;
     UPDATE tpcb.account SET balance = bal, filler = 'account updated'
     WHERE id = acct;
     UPDATE tpcb.branch SET balance = bbal, filler = 'branch updated'
     WHERE bid = brch;
     UPDATE tpcb.teller SET balance = tbal, filler = 'teller updated'
     WHERE tid = tell;
   ELSE
     SET bal = bal - amount;
     SET bbal = bbal - amount;
     SET tbal = tbal - amount;
     UPDATE tpcb.account SET balance = bal, filler = 'account updated'
     WHERE id = acct;
     UPDATE tpcb.branch SET balance = bbal, filler = 'branch updated'
     WHERE bid = brch;
     UPDATE tpcb.teller SET balance = tbal, filler = 'teller updated'
     WHERE tid = tell;
  END IF;

  IF (format = 'SBR')
  THEN
    SET local_uuid=UUID();
    SET local_user=USER();
    SET local_time= NOW();
    INSERT INTO tpcb.history VALUES(NULL,acct,tell,brch,amount, local_time,local_user,
                             local_uuid,'completed trans');
  ELSE
    INSERT INTO tpcb.history VALUES(NULL,acct,tell,brch,amount, NOW(), USER(),
                             UUID(),'completed trans');
  END IF;
END|
delimiter ;|
--enable_query_log
--echo
--echo *** Stored Procedures Created ***
--echo

