SET GLOBAL innodb_fast_shutdown=0;
# restart: --debug=d,innodb_purge_sleep_12
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 JSON);
SET @data_1 = REPEAT('Ax', 16384);
SET @data_2 = REPEAT('By', 16384);
SET @data_3 = REPEAT('Cz', 16384);
SET @data_4 = REPEAT('Pn', 16384);
SET @json_doc_1 = CONCAT('["', @data_1, '", "', @data_2, '", "', @data_3, '", "', @data_4, '"]');
INSERT INTO t1 (f1, f2) VALUES (1, @json_doc_1);
start transaction;
SELECT f1 FROM t1;
f1
1
SET @e1 = REPEAT('Do', 16384);
SET @e2 = REPEAT('Em', 16384);
SET @e3 = REPEAT('Fi', 16384);
SET @e4 = REPEAT('Gj', 16384);
SET @json_doc_2 = CONCAT('["', @e1, '", "', @e2, '", "', @e3, '", "', @e4, '"]');
UPDATE t1 SET f2 = @json_doc2;
SET @e4 = REPEAT('Hu', 16384);
SET @elem4 = CONCAT('"', @e4, '"');
UPDATE t1 SET f2 = JSON_SET(f2, '$[3]', @elem4) WHERE f1 = 1;
DELETE FROM t1;
commit;
DROP TABLE t1;
# restart:
