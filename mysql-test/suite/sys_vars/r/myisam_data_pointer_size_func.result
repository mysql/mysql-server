call mtr.add_suppression("The table 't1' is full");
'#--------------------FN_DYNVARS_093_01-------------------------#'
SET @start_value= @@global.myisam_data_pointer_size;
SET @@global.myisam_data_pointer_size = 2;
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = 3;
'connect (con2,localhost,root,,,,)'
'connection con2'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
3
'#--------------------FN_DYNVARS_093_02-------------------------#'
'connection con1'
DROP PROCEDURE IF EXISTS sp_addRec;
DROP TABLE IF EXISTS t1;
CREATE PROCEDURE sp_addRec(IN count INT)
BEGIN
WHILE (count>0) DO
INSERT INTO t1 value(1);
SET count = count -1;
END WHILE;
END //
SET @@global.myisam_data_pointer_size = 2;
CREATE TABLE t1(a INT);
CALL sp_addRec(65535);
CALL sp_addRec(1);
ERROR HY000: The table 't1' is full
SELECT count(*) from t1;
count(*)
65535
'--Checking myisam_data_pointer_size with MAX_ROWS table option--'
SET @@global.myisam_data_pointer_size = 2;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT)MAX_ROWS=70000;
CALL sp_addRec(65536);
SELECT count(*) from t1;
count(*)
65536
DROP PROCEDURE  sp_addRec;
DROP TABLE t1;
SET @@global.myisam_data_pointer_size= @start_value;
