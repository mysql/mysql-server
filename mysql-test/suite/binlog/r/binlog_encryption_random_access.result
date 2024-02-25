CALL mtr.add_suppression('Unsafe statement written to the binary log using statement format');
CREATE TABLE t1 (c1 INT PRIMARY KEY, c2 TEXT, pos INT);
# Inserting 100 random transaction
# Asserting we can show binlog events from each transaction
DROP TABLE t1;
