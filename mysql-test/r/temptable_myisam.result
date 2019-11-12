CREATE DATABASE temptable_test;
USE temptable_test;
# Test 02
SET optimizer_switch = "derived_merge=off";
CREATE TABLE t1 (
col1 BIGINT NOT NULL,
col2 BIGINT NOT NULL
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (8, 109), (4, 98), (4, 120), (7, 103), (4, 112);
CREATE VIEW v1 AS SELECT * FROM t1;
SELECT col1
FROM t1
WHERE ( NOT EXISTS (
SELECT col2
FROM t1
WHERE ( 7 ) IN (
SELECT v1.col1
FROM ( v1 JOIN ( SELECT * FROM t1 ) AS d1
ON ( d1.col2 = v1.col2 )
)
)
));
col1
DROP VIEW v1;
DROP TABLE t1;
SET optimizer_switch = default;
SET SESSION internal_tmp_mem_storage_engine = default;
USE test;
DROP DATABASE temptable_test;
