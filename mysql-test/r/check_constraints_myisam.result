#-----------------------------------------------------------------------
# Test case to verify alter operations on non-transactional SE table
# with check constraints.
#-----------------------------------------------------------------------
CREATE TABLE t1(f1 INT CHECK (f1 < 10)) ENGINE = myisam;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  CONSTRAINT `t1_chk_1` CHECK ((`f1` < 10))
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ADD f2 INT, ALGORITHM=copy;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL,
  CONSTRAINT `t1_chk_1` CHECK ((`f1` < 10))
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER f1 SET DEFAULT 10, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT '10',
  `f2` int DEFAULT NULL,
  CONSTRAINT `t1_chk_1` CHECK ((`f1` < 10))
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
