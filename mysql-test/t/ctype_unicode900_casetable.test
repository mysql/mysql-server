# This test takes long time, so only run it with the --big-test mtr-flag.
--source include/big_test.inc
--source include/not_valgrind.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
SET NAMES utf8mb4;
SET collation_connection=utf8mb4_0900_ai_ci;

#
# Populate table t1 with all ucs2 codes [00..FF][00..FF]
#
CREATE TABLE t1 (b CHAR(1)) ENGINE=INNODB;
INSERT INTO t1 VALUES ('0'), ('1'), ('2'), ('3'), ('4'), ('5'), ('6'), ('7');
INSERT INTO t1 VALUES ('8'), ('9'), ('A'), ('B'), ('C'), ('D'), ('E'), ('F');

CREATE TEMPORARY TABLE head AS SELECT concat(b1.b, b2.b) AS head
FROM t1 b1, t1 b2;
CREATE TEMPORARY TABLE tail AS SELECT concat(b1.b, b2.b) AS tail
FROM t1 b1, t1 b2;
DROP TABLE t1;

CREATE TABLE t1 (code char(4)) ENGINE=INNODB;
INSERT INTO t1 SELECT CONCAT(head, tail) FROM head, tail ORDER BY
head, tail;

CREATE TABLE t2 (code CHAR(4)) ENGINE=INNODB;
INSERT INTO t2 VALUES
('0000'), ('0001'), ('0002'), ('0003'), ('0004'), ('0005'),
('0006'), ('0007'), ('0008'), ('0009'), ('000A'), ('000B'),
('000C'), ('000D'), ('000E'), ('000F'), ('0010');

CREATE TABLE fulltable (code char(1)) CHARACTER SET UTF32 ENGINE=INNODB;
INSERT INTO fulltable SELECT UNHEX(CONCAT(t2.code, t1.code)) FROM t1, t2
WHERE (UNHEX(CONCAT(t2.code, t1.code)) <= 0x0000D7FF)
 OR
      ( (UNHEX(CONCAT(t2.code, t1.code)) >= 0x0000E000) AND
        (UNHEX(CONCAT(t2.code, t1.code)) <= 0x0010FFFF)    )
ORDER BY t2.code, t1.code;

DROP TABLE t1, t2;
DROP TEMPORARY TABLE head, tail;

#####################################################################

--echo # Print all the upper/lower in utf8mb4

SELECT HEX(CONVERT(code USING utf8mb4)),
HEX(UPPER(CONVERT(code USING utf8mb4))),
HEX(LOWER(CONVERT(code USING utf8mb4))),
code,
UPPER(CONVERT(code USING utf8mb4)),
LOWER(CONVERT(code USING utf8mb4))
FROM fulltable
WHERE (code <= 0x0000D7FF OR code >= 0x0000E000) AND
(HEX(CONVERT(code USING utf8mb4)) <> HEX(UPPER(CONVERT(code USING utf8mb4)))
OR HEX(CONVERT(code USING utf8mb4)) <> HEX(LOWER(CONVERT(code USING utf8mb4))))
ORDER BY HEX(CONVERT(code USING utf8mb4));

#####################################################################
DROP TABLE fulltable;
