#########################################################################
# RESTART 1 : WITH KEYRING PLUGIN
#########################################################################
SET SESSION debug= '+d,BtrBulk_insert_inject_error';

#########
# SETUP #
#########
CREATE PROCEDURE populate_t1()
BEGIN
DECLARE i int DEFAULT 1;
START TRANSACTION;
WHILE (i <= 10000) DO
INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
SET i = i + 1;
END WHILE;
COMMIT;
END|
CREATE TABLE t1(class INT, id INT, title VARCHAR(100)) encryption='N';
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' encryption='N';
ALTER TABLE t1 TABLESPACE=encrypt_ts, ALGORITHM=INPLACE;
ERROR 70100: Query execution was interrupted
ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';
#########################################################################
# RESTART 2 : WITH KEYRING PLUGIN
#########################################################################
ALTER TABLESPACE encrypt_ts ENCRYPTION='N';
###########
# Cleanup #
###########
DROP TABLE t1;
DROP TABLESPACE encrypt_ts;
DROP PROCEDURE populate_t1;
# Restarting server without keyring plugin to restore server state
# restart: 
