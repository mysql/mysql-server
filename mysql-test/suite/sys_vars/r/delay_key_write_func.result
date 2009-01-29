'#--------------------FN_DYNVARS_023_01-------------------------#'
SET @start_value= @@global.delay_key_write;
SET @@global.delay_key_write = ON;
SELECT @@global.delay_key_write;
@@global.delay_key_write
ON
'connect (user1,localhost,root,,,,)'
'connection user1'
SELECT @@global.delay_key_write AS res_is_ON;
res_is_ON
ON
SET @@global.delay_key_write = ALL;
'connect (user1,localhost,root,,,,)'
'connection user1'
SELECT @@global.delay_key_write AS res_is_ALL;
res_is_ALL
ALL
'#--------------------FN_DYNVARS_023_02-------------------------#'
'---check when delay_key_write is OFF---'
SET @@global.delay_key_write = OFF;
FLUSH STATUS;
CALL sp_addRecords(1,10);
SHOW STATUS LIKE 'Key_reads';
Variable_name	Value
Key_reads	0
SHOW STATUS LIKE 'Key_writes';
Variable_name	Value
Key_writes	9
SHOW STATUS LIKE 'Key_write_requests';
Variable_name	Value
Key_write_requests	9
SELECT COUNT(*) FROM t1;
COUNT(*)
9
'----check when delay_key_write is ON---'
SET @@global.delay_key_write = ON;
FLUSH STATUS;
CALL sp_addRecords(1,10);
SHOW STATUS LIKE 'Key_reads';
Variable_name	Value
Key_reads	0
SHOW STATUS LIKE 'Key_writes';
Variable_name	Value
Key_writes	0
SHOW STATUS LIKE 'Key_write_requests';
Variable_name	Value
Key_write_requests	9
SELECT COUNT(*) FROM t1;
COUNT(*)
9
'----check when delay_key_write is ALL---'
SET @@global.delay_key_write = ALL;
FLUSH STATUS;
CALL sp_addRecords(1,10);
SHOW STATUS LIKE 'Key_reads';
Variable_name	Value
Key_reads	0
SHOW STATUS LIKE 'Key_writes';
Variable_name	Value
Key_writes	0
SHOW STATUS LIKE 'Key_write_requests';
Variable_name	Value
Key_write_requests	9
SELECT COUNT(*) FROM t1;
COUNT(*)
9
DROP PROCEDURE sp_addRecords;
DROP TABLE t1;
SET @@global.delay_key_write= @start_value;
