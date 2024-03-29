# Test 6: Unsupported storage engines.
#
CREATE TABLE t1(a INT) engine= MyISAM;
FLUSH TABLE t1 FOR EXPORT;
ERROR HY000: Table storage engine for 't1' doesn't have this option
DROP TABLE t1;
