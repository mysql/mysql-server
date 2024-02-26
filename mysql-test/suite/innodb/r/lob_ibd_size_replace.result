#
# BUG 31222046 - .IDB GROWS INDEFENITELY (BUG #96466 WITH REPLACE)
#
CREATE TABLE `my_sample_table`
(`id` INT NOT NULL AUTO_INCREMENT,
`name` VARCHAR(128) NOT NULL,
`some_data` MEDIUMBLOB,
`update_time` TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
PRIMARY KEY (`id`),
UNIQUE KEY `name` (`name`));
CREATE PROCEDURE p(p_num INT)
BEGIN
DECLARE v_i INT DEFAULT 0;
REPEAT
REPLACE INTO my_sample_table (id, name, some_data) VALUES (1, 'data1', REPEAT('x',7609));
REPLACE INTO my_sample_table (id, name, some_data) VALUES (1, 'data1', REPEAT('t', 4184950));
SET v_i:=v_i+1;
UNTIL v_i>p_num END REPEAT;
END $
REPLACE INTO my_sample_table (id, name, some_data) VALUES (1, 'data1', REPEAT('t', 4184950));
START TRANSACTION;
CALL p(30);
COMMIT;
FLUSH TABLES `my_sample_table` FOR EXPORT;
UNLOCK TABLES;
SET @fs1 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/my_sample_table');
START TRANSACTION;
CALL p(30);
COMMIT;
FLUSH TABLES `my_sample_table` FOR EXPORT;
UNLOCK TABLES;
SET @fs2 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/my_sample_table');
SET @growing = (SELECT (@fs2 - @fs1));
SELECT @growing;
@growing
0
START TRANSACTION;
CALL p(30);
ROLLBACK;
FLUSH TABLES `my_sample_table` FOR EXPORT;
UNLOCK TABLES;
SET @fs3 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/my_sample_table');
SET @growing = (SELECT (@fs3 - @fs2));
SELECT @growing;
@growing
0
DROP PROCEDURE p;
DROP TABLE my_sample_table;
