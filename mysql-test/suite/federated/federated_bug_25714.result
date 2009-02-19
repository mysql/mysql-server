CREATE DATABASE federated;
CREATE DATABASE federated;
SET @OLD_MASTER_CONCURRENT_INSERT= @@GLOBAL.CONCURRENT_INSERT;
SET @@GLOBAL.CONCURRENT_INSERT= 0;
SET @OLD_SLAVE_CONCURRENT_INSERT= @@GLOBAL.CONCURRENT_INSERT;
SET @@GLOBAL.CONCURRENT_INSERT= 0;
DROP TABLE IF EXISTS federated.bug_13118_table;
CREATE TABLE federated.t1 (
`id` int auto_increment primary key, 
`value` int
) ENGINE=MyISAM;
INSERT INTO federated.t1 SET value=1;
INSERT INTO federated.t1 SET value=2;
INSERT INTO federated.t1 SET value=2;
DROP TABLE IF EXISTS federated.t1;
CREATE TABLE federated.t1 (
`id` int auto_increment primary key, 
`value` int
) ENGINE=FEDERATED
CONNECTION='mysql://root@127.0.0.1:SLAVE_PORT/federated/t1';
SELECT * from federated.t1;
id	value
1	1
2	2
3	2
INSERT INTO federated.t1 SET value=4;
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
4

5 inserted
6 inserted

7 inserted
8 inserted
SELECT * from federated.t1;
id	value
1	1
2	2
3	2
4	4
5	54
6	55
7	54
8	55
DROP TABLE federated.t1;
SET @@GLOBAL.CONCURRENT_INSERT= @OLD_MASTER_CONCURRENT_INSERT;
DROP TABLE federated.t1;
SET @@GLOBAL.CONCURRENT_INSERT= @OLD_SLAVE_CONCURRENT_INSERT;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
