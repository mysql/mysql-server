CREATE TABLE t1 (
id INT PRIMARY KEY,
u CHAR(2) NOT NULL,
v CHAR(2) NOT NULL,
UNIQUE INDEX by_u_1 (u(1)),
INDEX by_v_1 (v(1))
);
INSERT INTO t1 VALUES (0,"aa","aa"),(10,"ee","ee"),(20,"oo","oo");
BEGIN;
SELECT COUNT(*) FROM t1 FORCE INDEX ( by_v_1 ) WHERE v>"b" AND id<>10 FOR SHARE;
COUNT(*)
1
BEGIN;
UPDATE t1 SET u="oo",v="ef" WHERE id=10;
ERROR 23000: Duplicate entry 'o' for key 't1.by_u_1'
ROLLBACK;
DROP TABLE t1;
