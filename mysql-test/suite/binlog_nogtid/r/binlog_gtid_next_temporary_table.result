RESET MASTER;
CREATE TABLE t1 (c1 INT);
SET @@GLOBAL.GTID_MODE = OFF_PERMISSIVE;
SET SESSION autocommit = 0;
CREATE TEMPORARY TABLE Temp1 SELECT RAND();
SET SESSION GTID_NEXT = '11111111-AAAA-2222-BBBB-000000000000:1';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
COMMIT;
CREATE TEMPORARY TABLE Temp2(c1 INT);
SET SESSION GTID_NEXT = '11111111-AAAA-2222-BBBB-000000000000:2';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
COMMIT;
CREATE TEMPORARY TABLE Temp3 LIKE Temp2;
SET SESSION GTID_NEXT = '11111111-AAAA-2222-BBBB-000000000000:3';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
COMMIT;
SET SESSION GTID_NEXT = '11111111-AAAA-2222-BBBB-000000000000:4';
DROP TABLE t1;
SET SESSION GTID_NEXT = 'automatic';
DROP TEMPORARY TABLE Temp1,Temp2,Temp3;
SET SESSION GTID_NEXT='11111111-AAAA-2222-BBBB-000000000000:5';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
COMMIT;
SET SESSION autocommit = 1;
SET @@GLOBAL.GTID_MODE = OFF;
