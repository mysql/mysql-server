call mtr.add_suppression("Failed to find tablespace for table .* in the cache. Attempting to load the tablespace with space id");
CREATE TABLE t1(
a INT AUTO_INCREMENT PRIMARY KEY,
b CHAR(1),
c INT,
INDEX(b))
ENGINE=InnoDB STATS_PERSISTENT=0;
SET GLOBAL innodb_change_buffering_debug = 1;
INSERT INTO t1 VALUES(0,'x',1);
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
INSERT INTO t1 SELECT 0,b,c FROM t1;
# restart: --innodb-force-recovery=6
insert into t1 values(0, 'y', 1);
ERROR HY000: Can't lock file (errno: 165 - Table is read only)
# restart
DROP TABLE t1;
