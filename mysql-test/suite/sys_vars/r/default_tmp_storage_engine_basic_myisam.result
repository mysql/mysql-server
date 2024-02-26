SET @start_global_value = @@global.default_tmp_storage_engine;
SELECT @start_global_value;
@start_global_value
MyISAM
SET @start_session_value = @@session.default_tmp_storage_engine;
SELECT @start_session_value;
@start_session_value
MyISAM
'#--------------------FN_DYNVARS_005_01-------------------------#'
SET @@global.default_tmp_storage_engine = INNODB;
SET @@global.default_tmp_storage_engine = DEFAULT;
SELECT @@global.default_tmp_storage_engine;
@@global.default_tmp_storage_engine
MyISAM
SET @@session.default_tmp_storage_engine = INNODB;
SET @@session.default_tmp_storage_engine = DEFAULT;
SELECT @@session.default_tmp_storage_engine;
@@session.default_tmp_storage_engine
MyISAM
'#--------------------FN_DYNVARS_005_02-------------------------#'
SET @@global.default_tmp_storage_engine = MYISAM;
SELECT @@global.default_tmp_storage_engine;
@@global.default_tmp_storage_engine
MyISAM
'#--------------------FN_DYNVARS_005_03-------------------------#'
SET @@session.default_tmp_storage_engine = MYISAM;
SELECT @@session.default_tmp_storage_engine;
@@session.default_tmp_storage_engine
MyISAM
'#---------------------FN_DYNVARS_001_8----------------------#'
SET @@default_tmp_storage_engine = MYISAM;
SELECT @@default_tmp_storage_engine = @@local.default_tmp_storage_engine;
@@default_tmp_storage_engine = @@local.default_tmp_storage_engine
1
SELECT @@local.default_tmp_storage_engine = @@session.default_tmp_storage_engine;
@@local.default_tmp_storage_engine = @@session.default_tmp_storage_engine
1
SET @@default_tmp_storage_engine = @start_global_value;
SET default_tmp_storage_engine = MyISAM;
SET default_storage_engine = MyISAM;
CREATE TABLE t1 (a INT);
CREATE TEMPORARY TABLE t2 (b INT);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TEMPORARY TABLE `t2` (
  `b` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1, t2;
SET default_tmp_storage_engine = DEFAULT;
SET default_storage_engine = DEFAULT;
CREATE TABLE t1 (a INT);
CREATE TEMPORARY TABLE t2 (b INT);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TEMPORARY TABLE `t2` (
  `b` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1, t2;
SET @@global.default_tmp_storage_engine = @start_global_value;
SELECT @@global.default_tmp_storage_engine;
@@global.default_tmp_storage_engine
MyISAM
SET @@session.default_tmp_storage_engine = @start_session_value;
SELECT @@session.default_tmp_storage_engine;
@@session.default_tmp_storage_engine
MyISAM
