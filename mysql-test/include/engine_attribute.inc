--echo #
--echo # WL#13341: Store options for secondary engines.
--echo # Testing engine and secondary engine attributes on tables, fields and 
--echo # indices.
--echo #
--echo ### Testing ENGINE_ATTRIBUTE on table

--error ER_PARSE_ERROR
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE;

--error ER_INVALID_JSON_ATTRIBUTE
CREATE TABLE t1(i INT) engine_ATTRIBUTE='{bad:json';

--echo # Testing attributes on tables
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE='';
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
DROP TABLE t1;
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE='{"table attr": "for engine"}';
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
DROP TABLE t1;
}

--echo ### Testing SECONDARY_ENGINE_ATTRIBUTE on table

--error ER_PARSE_ERROR
CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE;

--error ER_INVALID_JSON_ATTRIBUTE
CREATE TABLE t1(i INT) SECONDARY_engine_ATTRIBUTE='{bad:json';

CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE='';
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE='{"table attr": "for secondary engine"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
DROP TABLE t1;

--echo ### Testing ENGINE_ATTRIBUTE on column

--error ER_PARSE_ERROR
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE);

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE='');

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW FULL COLUMNS FROM t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
DROP TABLE t1;
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE='{"column attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"column attr": "for secondary engine"}');

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW FULL COLUMNS FROM t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
DROP TABLE t1;
}

--echo ### Testing ENGINE_ATTRIBUTE on index specified as a part of CREATE
--error ER_PARSE_ERROR
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE);

--error ER_INVALID_JSON_ATTRIBUTE
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": }');

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr": "for secondary engine"}');

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW INDEX FROM t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
DROP TABLE t1;
}

--echo ### Testing ENGINE_ATTRIBUTE with CREATE INDEX

CREATE TABLE t1(i INT, j INT);

--error ER_PARSE_ERROR
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE;

--error ER_INVALID_JSON_ATTRIBUTE
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{attr';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr":"for secondary engine"}';
SHOW CREATE TABLE t1;
SHOW INDEX FROM t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
DROP TABLE t1;

--echo ### Creating table with all possible engine attributes.
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT
       ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}', 
       j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}', 
       INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' 
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}')
       ENGINE_ATTRIBUTE = '{"table attr":"t1"}' SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW FULL COLUMNS FROM t1;
SHOW INDEX FROM t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;

--echo # Verify that SHOW CREATE TABLE output can be used to create
--echo # an identical table
--let $SCT1= query_get_value(SHOW CREATE TABLE t1, Create Table, 1)

DROP TABLE t1;
--eval $SCT1
--let $SCT1_= query_get_value(SHOW CREATE TABLE t1, Create Table, 1)

--echo # There should be no output beween here
if ($SCT1 != $SCT1_)
{
--echo SHOW CREATE TABLE output not stable
--echo $SCT1
--echo vs
--echo $SCT1_
}
--echo # ... and here
DROP TABLE t1;
}

--echo ## Verify engine attributes are duplicated in the same way as comment
--echo ## in CREATE LIKE/CREATE AS SELECT
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT
       ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}'
       COMMENT 'Column comment', 
       j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}', 
       INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' 
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}' COMMENT 'index comment')
       ENGINE_ATTRIBUTE = '{"table attr":"t1"}' SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}' COMMENT='Table comment';

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW FULL COLUMNS FROM t1;
SHOW INDEX FROM t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;

CREATE TABLE t2 LIKE t1;

--echo # Expect all engine attributes from t1 to be present also in t2
SHOW CREATE TABLE t2;
SHOW FULL COLUMNS FROM t2;
SHOW INDEX FROM t2;

CREATE TABLE t3 ENGINE_ATTRIBUTE='{"table attr": "t3, create as select"}' AS SELECT * FROM t1;

--echo # Expect column engine attributes from t1 to be present in t3.
--echo # Indices are not duplicated so index engine attributes will not be
--echo # present.
--echo # Table engine attributes are not duplicated, just like table comments.
SHOW CREATE TABLE t3;
SHOW FULL COLUMNS FROM t3;
SHOW INDEX FROM t3;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't3';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't3' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't3' ORDER BY constraint_name;

DROP TABLE t3;
DROP TABLE t2;
DROP TABLE t1;
}

--echo ## Verfy expected behavior for repeated engine attributes
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
CREATE TABLE t1(i INT
       ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       ENGINE_ATTRIBUTE = '{"column attr": "override"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "override"}'
       COMMENT 'Column comment'
       COMMENT 'Column comment override', 
       j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       ENGINE_ATTRIBUTE = '{"column attr": "override"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}' 
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "override"}', 
       INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' ENGINE_ATTRIBUTE='{"index attr": "index on j override"}' 
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}' 
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j override"}' 
COMMENT 'index comment'
COMMENT 'index comment override'
)
       ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       ENGINE_ATTRIBUTE = '{"table attr":"t1 override"}'	
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1 override"}'
       COMMENT='Table comment'
       COMMENT='Table comment override';

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
SHOW FULL COLUMNS FROM t1;
SHOW INDEX FROM t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;

DROP TABLE t1;
}

--echo #
--echo # Testing altering engine attribute on tables,
--echo # columns and indices.
--echo #

CREATE TABLE t1(i INT, j INT, k INT);

--echo ## Altering attributes on table
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';


if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;
}

--echo ## Adding columns with attributes
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ADD COLUMN l VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ADD COLUMN m VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ADD COLUMN n VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"instant"}', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--echo ## Modifying columns with attributes
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 MODIFY COLUMN l INT ENGINE_ATTRIBUTE='{"modify column algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 MODIFY COLUMN m INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN m INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 MODIFY COLUMN n INT ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN n INT ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
}

--echo ## Changing columns with attributes
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 CHANGE COLUMN l ll CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
}

--echo ## Adding index with attributes
if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ADD INDEX (i) ENGINE_ATTRIBUTE='{"index algo":"none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 ADD INDEX (m) ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
}
if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
ALTER TABLE t1 ADD INDEX (m) ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
}

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
ALTER TABLE t1 ADD INDEX (n) ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;
}

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ADD INDEX (n) ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;
}

DROP TABLE t1;

--echo #
--echo # Testing altering secondary engine attribute on tables and
--echo # columns, and indices.
--echo #

CREATE TABLE t1(i INT, j INT, k INT);

--echo ## Altering attributes on table
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';

ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;

--echo ## Adding columns with attributes
ALTER TABLE t1 ADD COLUMN l VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

ALTER TABLE t1 ADD COLUMN m VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

ALTER TABLE t1 ADD COLUMN n VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"instant"}', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--echo ## Modifying columns with attributes
ALTER TABLE t1 MODIFY COLUMN l INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN m INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN m INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN n INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 MODIFY COLUMN n INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';


--echo ## Changing columns with attributes
ALTER TABLE t1 CHANGE COLUMN l ll CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';

--echo ## Adding index with attributes
ALTER TABLE t1 ADD INDEX (i) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"none"}';
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';

ALTER TABLE t1 ADD INDEX (m) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';

--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ADD INDEX (n) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;

DROP TABLE t1;


--echo # Testing all possible engine attribute clauses together
--echo #

CREATE TABLE t1(i INT, j INT, INDEX jix (j));

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 MODIFY COLUMN i INT
       ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}',
       MODIFY COLUMN j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}',
       DROP INDEX jix,
       ADD INDEX jix (i) ENGINE_ATTRIBUTE='{"index attr": "index on j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}',
       ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
}
DROP TABLE t1;

--echo # Verfy that handler::check_if_supported_inplace_alter
--echo # (default if not overridden by SE) behaves the same way for
--echo # engine attributes as for comments.
CREATE TABLE t1(i INT, j INT, INDEX jix (j)) ENGINE=MYISAM;

ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INPLACE;

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1
    SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}',
       MODIFY COLUMN j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}',
       DROP INDEX jix,
       ADD INDEX jix (i) ENGINE_ATTRIBUTE='{"index attr": "index on j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}',
       ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';

if ($SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
--sorted_result
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
--sorted_result
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
}
DROP TABLE t1;

--echo # Verfy that handler::check_if_supported_inplace_alter
--echo # (default if not overridden by SE) behaves the same way for
--echo # engine attributes as for comments.
CREATE TABLE t1(i INT, j INT) ENGINE=MYISAM;

ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INSTANT;

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INSTANT;

ALTER TABLE t1 MODIFY COLUMN i INT COMMENT 'Modified column with comment', ALGORITHM=INSTANT;

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 MODIFY COLUMN i INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INSTANT;

ALTER TABLE t1 CHANGE COLUMN i mm INT COMMENT 'Changed column with comment', ALGORITHM=INSTANT;

if (!$SIMULATED_ENGINE_ATTRIBUTE_SUPPORT) {
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED
}
ALTER TABLE t1 CHANGE COLUMN j nn INT ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INSTANT;

DROP TABLE t1;


--echo # Verfy that handler::check_if_supported_inplace_alter
--echo # (default if not overridden by SE) behaves the same way for
--echo # secondary engine attributes as for comments.
CREATE TABLE t1(i INT, j INT) ENGINE=MYISAM;

ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INSTANT;

ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INSTANT;

ALTER TABLE t1 MODIFY COLUMN i INT COMMENT 'Modified column with comment', ALGORITHM=INSTANT;

ALTER TABLE t1 MODIFY COLUMN i INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INSTANT;

ALTER TABLE t1 CHANGE COLUMN i mm INT COMMENT 'Changed column with comment', ALGORITHM=INSTANT;

ALTER TABLE t1 CHANGE COLUMN j nn INT SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INSTANT;

DROP TABLE t1;

