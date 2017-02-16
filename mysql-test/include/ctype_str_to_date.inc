--echo #
--echo # MDEV-4842 STR_TO_DATE does not work with UCS2/UTF16/UTF32
--echo #

SELECT @@character_set_connection, HEX(CAST(_utf8'÷' AS CHAR));
SELECT STR_TO_DATE(CAST(_utf8'2001÷01÷01' AS CHAR),CAST(_utf8'%Y÷%m÷%d' AS CHAR));
CREATE TABLE t1 AS SELECT REPEAT(' ', 64) AS subject, REPEAT(' ',64) AS pattern LIMIT 0;
SHOW COLUMNS FROM t1;
INSERT INTO t1 VALUES (_utf8'2001÷01÷01',_utf8'%Y÷%m÷%d');
SELECT HEX(subject),HEX(pattern),STR_TO_DATE(subject, pattern) FROM t1;
DROP TABLE t1;

