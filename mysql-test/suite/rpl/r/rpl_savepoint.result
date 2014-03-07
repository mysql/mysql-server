include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Bug#50124 Rpl failure on DROP table with concurrent txn/non-txn
#           DML flow and SAVEPOINT
#
# Connection master
DROP TABLE IF EXISTS tt, nt;
CREATE TABLE tt (i INT) ENGINE = InnoDB;
CREATE TABLE nt (i INT) ENGINE = MyISAM;
FLUSH LOGS;
START TRANSACTION;
INSERT INTO nt VALUES (1);
SAVEPOINT insert_statement;
INSERT INTO tt VALUES (1);
# Connection master1
# Sending:
DROP TABLE tt;
# Connection master
ROLLBACK TO SAVEPOINT insert_statement;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
COMMIT;
# Connection master1
# Reaping: DROP TABLE tt
FLUSH LOGS;
# Connection master
DROP TABLE nt;
include/rpl_end.inc
