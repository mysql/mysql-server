CREATE TABLE t1 (c1 INT);
LOAD DATA INFILE "t1.MYI" into table t1;
ERROR HY000: The MySQL server is running with the --secure-file-priv option so it cannot execute this statement
LOAD DATA INFILE "/test" into table t1;
ERROR HY000: The MySQL server is running with the --secure-file-priv option so it cannot execute this statement
DROP TABLE t1;
