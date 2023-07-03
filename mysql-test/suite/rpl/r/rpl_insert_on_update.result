include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,k INT,UNIQUE KEY(k)) ENGINE=InnoDB;
INSERT INTO t(k) VALUES (1), (2), (3);
INSERT INTO t(k) VALUES (2), (4), (5) ON DUPLICATE KEY UPDATE id=10;
include/sync_slave_sql_with_master.inc
SET SESSION sql_log_bin= 0;
CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format*");
SET SESSION sql_log_bin= 1;
include/diff_tables.inc [master:t,slave:t]
DROP TABLE t;
include/rpl_end.inc
