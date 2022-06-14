include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE `log_data` (
`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
`series` INT UNSIGNED NOT NULL,
`tm_ts` VARCHAR(14) DEFAULT NULL,
`data` BLOB,
`v1_gcol` INT GENERATED ALWAYS AS (`series` + 1) VIRTUAL NOT NULL,
`s1_gcol` INT GENERATED ALWAYS AS (`series` + 1) STORED NOT NULL,
`v2_gcol` INT GENERATED ALWAYS AS (`series` + 2) VIRTUAL NOT NULL,
`s2_gcol` INT GENERATED ALWAYS AS (`series` + 2) STORED NOT NULL,
PRIMARY KEY (`id`),
KEY `v1_idx` (`v1_gcol`),
KEY `s1_idx` (`s1_gcol`)
) ENGINE = InnoDB;
include/sync_slave_sql_with_master.inc
ALTER TABLE
`log_data`
MODIFY
`v1_gcol` INT GENERATED ALWAYS AS (`series` - 1) VIRTUAL NOT NULL,
MODIFY
`s1_gcol` INT GENERATED ALWAYS AS (`series` - 1) STORED NOT NULL,
MODIFY
`v2_gcol` INT GENERATED ALWAYS AS (`series` + 3) VIRTUAL NOT NULL,
MODIFY
`s2_gcol` INT GENERATED ALWAYS AS (`series` + 3) STORED NOT NULL,
ADD
COLUMN `s_len` INT GENERATED ALWAYS AS (
LENGTH(`data`)
) STORED,
ADD
COLUMN `s_date` DATE GENERATED ALWAYS AS (
CAST(`tm_ts` AS DATE)
) STORED,
ADD
COLUMN `v_date` DATE GENERATED ALWAYS AS (
CAST(`tm_ts` AS DATE)
) VIRTUAL,
ADD
COLUMN `s_hash` char(40) GENERATED ALWAYS AS (
SHA(
HEX(`data`)
)
) STORED,
ADD
COLUMN `v_hash` char(40) GENERATED ALWAYS AS (
SHA(
HEX(`data`)
)
) VIRTUAL,
ADD
KEY `k_s_len` (s_len),
ADD
KEY `k_s_date` (s_date),
ADD
KEY `k_v_date`(v_date),
ADD
KEY `k_s_hash`(s_hash),
ADD
KEY `k_v_hash`(v_hash);
[connection master]
INSERT INTO `log_data` (`series`, `tm_ts`, `data`)
SELECT *
FROM
(
WITH RECURSIVE sequence AS (
SELECT
1 AS level
UNION ALL
SELECT
level + 1
FROM
sequence
WHERE
level < 10
)
SELECT
1,
CURDATE()+ 0,
REPEAT(
rand(),
3
)
FROM
sequence
) AS F;
include/sync_slave_sql_with_master.inc
include/assert.inc [Stored columns not NULL count is 10]
include/assert.inc [Stored columns with series equal to 4 count is 10]
include/assert.inc [Stored columns with series equal to 0 count is 10]
[connection master]
UPDATE `log_data` SET `tm_ts` = "20210101", `series` = 2;
include/sync_slave_sql_with_master.inc
include/assert.inc [Stored columns with s_date equal to 2021-01-01 count is 10]
include/assert.inc [Stored columns with series equal to 5 count is 10]
include/assert.inc [Stored columns with series equal to 1 count is 10]
[connection master]
DELETE FROM `log_data` WHERE `id` < 11;
include/sync_slave_sql_with_master.inc
include/assert.inc [Count is 0]
[connection master]
DROP TABLE `log_data`;
include/rpl_end.inc
