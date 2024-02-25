CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SET GLOBAL DEBUG = '+d, sdi_set_failure';
CREATE TABLE t1(a INT) TABLESPACE `ts1`;
ERROR HY000: Failed to set SDI 'test.t1' in tablespace 'ts1'.
SET GLOBAL DEBUG = '-d, sdi_set_failure';
CREATE TABLE t1(a INT) TABLESPACE `ts1`;
INSERT INTO t1 VALUES (1), (2), (3);
SET GLOBAL DEBUG = '+d, sdi_delete_failure';
DROP TABLE t1;
ERROR HY000: Failed to delete SDI 'test.t1' in tablespace 'ts1'.
SET GLOBAL DEBUG = '-d, sdi_delete_failure';
SELECT * FROM t1;
a
1
2
3
DROP TABLE t1;
DROP TABLESPACE ts1;
