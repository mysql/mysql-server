#
# Bug#55436: buffer overflow in debug binary of dbug_buff in
#            Field_new_decimal::store_value
#
SET SQL_MODE='';
CREATE TABLE t1(f1 DECIMAL(44,24)) ENGINE=MYISAM;
INSERT INTO t1 SET f1 = -64878E-85;
Warnings:
Note	1265	Data truncated for column 'f1' at row 1
SELECT f1 FROM t1;
f1
0.000000000000000000000000
DROP TABLE t1;
