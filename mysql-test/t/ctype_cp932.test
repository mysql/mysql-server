--echo #
--echo # Bug #11755818 LIKE DOESN'T MATCH WHEN CP932_BIN/SJIS_BIN COLLATIONS ARE 
--echo #               USED.
--echo #

SET @old_character_set_client= @@character_set_client;
SET @old_character_set_connection= @@character_set_connection;
SET @old_character_set_results= @@character_set_results;
SET character_set_client= 'utf8mb3';
SET character_set_connection= 'utf8mb3';
SET character_set_results= 'utf8mb3';

CREATE TABLE t1 (a VARCHAR(10) COLLATE cp932_bin);
INSERT INTO t1 VALUES('ｶｶ');
SELECT * FROM t1 WHERE a LIKE '%ｶ';
SELECT * FROM t1 WHERE a LIKE '_ｶ';
SELECT * FROM t1 WHERE a LIKE '%_ｶ';

ALTER TABLE t1 MODIFY a VARCHAR(100) COLLATE sjis_bin;
SELECT * FROM t1 WHERE a LIKE '%ｶ';
SELECT * FROM t1 WHERE a LIKE '_ｶ';
SELECT * FROM t1 WHERE a LIKE '%_ｶ';
DROP TABLE t1;

## Reset to initial values
SET @@character_set_client= @old_character_set_client;
SET @@character_set_connection= @old_character_set_connection;
SET @@character_set_results= @old_character_set_results;
