--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug#55436: buffer overflow in debug binary of dbug_buff in
--echo #            Field_new_decimal::store_value
--echo #

# this threw memory warnings on Windows. Also make sure future changes
# dont change these results, as per usual.
SET SQL_MODE='';
CREATE TABLE t1(f1 DECIMAL(44,24)) ENGINE=MYISAM;
INSERT INTO t1 SET f1 = -64878E-85;
SELECT f1 FROM t1;
DROP TABLE t1;
