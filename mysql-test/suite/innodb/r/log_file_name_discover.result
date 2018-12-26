SET GLOBAL innodb_file_per_table=ON;
CREATE DATABASE discover;
use discover;
SET GLOBAL debug="+d,periodical_checkpoint_disabled";
SET GLOBAL innodb_master_thread_disabled_debug=1;
SET SESSION debug="+d,fil_ibd_create_log";
CREATE TABLE discover.t1(c INT) engine=InnoDB;
ERROR HY000: Lost connection to MySQL server during query
# restart
CREATE TABLE discover.t2(c INT) ENGINE=InnoDB;
BEGIN;
INSERT INTO t2 VALUES(1);
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
# Kill the server
# restart
SELECT * FROM t2;
c
DROP DATABASE discover;
