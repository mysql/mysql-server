# In connection CON1
INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
SHOW VARIABLES LIKE 'clone_ddl_timeout';
Variable_name	Value
clone_ddl_timeout	300
SHOW VARIABLES LIKE 'clone_block_ddl';
Variable_name	Value
clone_block_ddl	OFF
# Set backup lock timeout to 1 sec
SET GLOBAL clone_block_ddl = true;
SET GLOBAL clone_ddl_timeout = 1;
SHOW VARIABLES LIKE 'clone_block_ddl';
Variable_name	Value
clone_block_ddl	ON
SHOW VARIABLES LIKE 'clone_ddl_timeout';
Variable_name	Value
clone_ddl_timeout	1
SHOW VARIABLES LIKE "lock_wait_timeout";
Variable_name	Value
lock_wait_timeout	1
# In connection DEFAULT
# 1A. Clone while CREATE TABLE in progress
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
CREATE TABLE t1 (col1 INT PRIMARY KEY);
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 1B. CREATE TABLE while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
CREATE TABLE t2 (col1 INT PRIMARY KEY);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 1C. ALTER INSTANCE while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER INSTANCE DISABLE INNODB REDO_LOG;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
SHOW STATUS LIKE 'innodb_redo_log_enabled';
Variable_name	Value
Innodb_redo_log_enabled	ON
# 1C. Clone while ALTER INSTANCE in progress
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER INSTANCE DISABLE INNODB REDO_LOG;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW STATUS LIKE 'innodb_redo_log_enabled';
Variable_name	Value
Innodb_redo_log_enabled	OFF
ALTER INSTANCE ENABLE INNODB REDO_LOG;
# 2A. Clone while ALTER TABLE ADD COLUMN [COPY] in progress
INSERT INTO t1 values(10), (20), (30);
SELECT * FROM t1 ORDER BY col1;
col1
10
20
30
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 ADD COLUMN col2 int, ALGORITHM=COPY;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1 ORDER BY col1;
col1	col2
10	NULL
20	NULL
30	NULL
UPDATE t1 SET col2 = col1 + 1000;
# 2B. ALTER TABLE ADD COLUMN [COPY] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 ADD COLUMN col3 int, ALGORITHM=COPY;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 2C. Clone while ALTER TABLE ADD COLUMN [INPLACE] in progress
SELECT * FROM t1 ORDER BY col1;
col1	col2
10	1010
20	1020
30	1030
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 ADD COLUMN col3 int, ALGORITHM=INPLACE;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1 ORDER BY col1;
col1	col2	col3
10	1010	NULL
20	1020	NULL
30	1030	NULL
UPDATE t1 SET col3 = col2 + 1000;
# 2D. ALTER TABLE ADD COLUMN [INPLACE] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 ADD COLUMN col4 int, ALGORITHM=INPLACE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 3A. Clone while ADD INDEX [COPY] in progress
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 ADD INDEX col2_idx(col2), ALGORITHM=COPY;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 3B. ADD INDEX [COPY] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 ADD INDEX col3_idx(col3), ALGORITHM=COPY;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 3C. Clone while ADD INDEX [INPLACE] in progress
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 ADD INDEX col3_idx(col3), ALGORITHM=INPLACE;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`),
  KEY `col3_idx` (`col3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 3D. ADD INDEX [INPLACE] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 ADD INDEX col1_idx(col1), ALGORITHM=COPY;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 4A. DROP INDEX [COPY] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 DROP INDEX col3_idx, ALGORITHM=COPY;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 4B. Clone while DROP INDEX [COPY] in progress
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`),
  KEY `col3_idx` (`col3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 DROP INDEX col3_idx, ALGORITHM=COPY;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 4C. DROP INDEX [INPLACE] while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
ALTER TABLE t1 DROP INDEX col2_idx, ALGORITHM=INPLACE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 4D. Clone while DROP INDEX [INPLACE] in progress
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`),
  KEY `col2_idx` (`col2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
ALTER TABLE t1 DROP INDEX col2_idx, ALGORITHM=INPLACE;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 5A. TRUNCATE TABLE while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
TRUNCATE TABLE t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
# 5B. Clone while TRUNCATE TABLE in progress
SELECT * FROM t1 ORDER BY col1;
col1	col2	col3
10	1010	2010
20	1020	2020
30	1030	2030
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
TRUNCATE TABLE t1;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
SELECT * FROM t1 ORDER BY col1;
col1	col2	col3
# 6A. DROP TABLE while clone in progress
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
DROP TABLE t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC = 'now SIGNAL resume_clone';
# In connection DEFAULT
SET DEBUG_SYNC = 'RESET';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` int DEFAULT NULL,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# 6B. Clone while DROP TABLE in progress
# In connection CON1
SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
DROP TABLE t1;
# In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
ERROR HY000: Clone Donor Error: 1205 : Lock wait timeout exceeded; try restarting transaction.
select ID, STATE, ERROR_NO from performance_schema.clone_status;
ID	STATE	ERROR_NO
1	Failed	3862
select ID, STAGE, STATE from performance_schema.clone_progress;
ID	STAGE	STATE
SET DEBUG_SYNC = 'now SIGNAL resume_ddl';
# In connection CON1
# In connection DEFAULT
#Cleanup
SET DEBUG_SYNC = 'RESET';
UNINSTALL PLUGIN clone;
