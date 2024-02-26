# Bug#34009876: When table_open_cache_instances = 1, the result of
#               CONNECTION_ID() called on the trigger is wrong.
Must be 1
SELECT @@GLOBAL.table_open_cache_instances;
@@GLOBAL.table_open_cache_instances
1
CREATE FUNCTION transaction_id()
RETURNS BIGINT
BEGIN
RETURN connection_id();
END;
//
CREATE TABLE t1
(
id  BIGINT NOT NULL PRIMARY KEY,
tr1 BIGINT NOT NULL,
tr2 BIGINT NOT NULL
);
CREATE TRIGGER t1_insert_trigger
BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
SET NEW.tr1 = connection_id();
SET NEW.tr2 = transaction_id();
END;
//
INSERT INTO t1(id) VALUES (1);
SELECT id,
tr1 = connection_id() AS "tr1 valid",
tr2 = connection_id() AS "tr2 valid"
FROM t1
WHERE id=1;
id	tr1 valid	tr2 valid
1	1	1
connect  conn1, localhost, root, , test;
connection conn1;
INSERT INTO t1(id) VALUES (2);
SELECT id,
tr1 = connection_id() AS "tr1 valid", 
tr2 = connection_id() AS "tr2 valid"
FROM t1
WHERE id=2;
id	tr1 valid	tr2 valid
2	1	1
SELECT COUNT(DISTINCT tr1), COUNT(DISTINCT tr2) FROM t1;
COUNT(DISTINCT tr1)	COUNT(DISTINCT tr2)
2	2
disconnect conn1;
connection default;
DROP TABLE t1;
DROP FUNCTION transaction_id;
