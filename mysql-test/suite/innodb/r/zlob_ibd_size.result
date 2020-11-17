# Ensure that there is not already lot of data to be purged.
# If timeout happens here, then check the chain of previously
# executed test cases.
SET GLOBAL innodb_compression_level = 0;
CREATE TABLE `t`
(`id` INT NOT NULL AUTO_INCREMENT,
`b` LONGBLOB,
PRIMARY KEY(`id`)) ENGINE=InnoDB ROW_FORMAT=COMPRESSED;
CREATE PROCEDURE p(p_num INT)
BEGIN
DECLARE v_i INT DEFAULT 0;
REPEAT
UPDATE t SET b:=REPEAT('a',1024*1024*4);
UPDATE t SET b:=REPEAT('b',1024*1024*4);
SET v_i:=v_i+1;
UNTIL v_i>p_num END REPEAT;
END $
SET @b:=repeat('b',4*1024*1024);
INSERT INTO t(id,b) VALUES(1,@b);
START TRANSACTION;
CALL p(20);
COMMIT;
FLUSH TABLES `t` FOR EXPORT;
UNLOCK TABLES;
SET @fs1 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/t');
START TRANSACTION;
CALL p(20);
COMMIT;
FLUSH TABLES `t` FOR EXPORT;
UNLOCK TABLES;
SET @fs2 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/t');
SET @growing = (SELECT (@fs2 - @fs1));
SELECT @growing;
@growing
0
START TRANSACTION;
CALL p(20);
ROLLBACK;
FLUSH TABLES `t` FOR EXPORT;
UNLOCK TABLES;
SET @fs3 = (SELECT file_size FROM information_schema.innodb_tablespaces
WHERE name = 'test/t');
SET @growing = (SELECT (@fs3 - @fs2));
SELECT @growing;
@growing
0
DROP PROCEDURE p;
DROP TABLE t;
SET GLOBAL innodb_compression_level = DEFAULT;
