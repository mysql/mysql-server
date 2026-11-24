--write_file $MYSQL_TMP_DIR/duckdb_index_meta.inc
  CALL dbms_duckdb.query("SELECT schema_name, table_name, index_name, comment, is_unique, is_primary, expressions FROM duckdb_indexes WHERE table_name = 't'");
  CALL dbms_duckdb.query("SELECT table_name, constraint_type, constraint_text FROM duckdb_constraints WHERE table_name = 't'");
  SELECT TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, INDEX_COMMENT, NON_UNIQUE, COLUMN_NAME FROM information_schema.statistics WHERE TABLE_NAME = 't';
  --echo
EOF

--echo #
--echo # 1) ADD non primary key will be ignore.
--echo #
CREATE TABLE t(a INT, b INT, c INT, d INT, e INT, PRIMARY KEY(a) COMMENT 'primary key') ENGINE = DuckDB;
eval ALTER TABLE t ADD UNIQUE KEY uk_b(b) COMMENT 'unique key b', ADD UNIQUE KEY uk_cd(c, d) COMMENT 'unique key cd', ADD INDEX ke(e) COMMENT 'index e', ALGORITHM = $algorithm;
CREATE UNIQUE INDEX uk_b ON t(b) COMMENT 'unique key b';
CREATE UNIQUE INDEX uk_cd ON t(c, d) COMMENT 'unique key cd';
CREATE INDEX ke ON t(e) COMMENT 'index e';
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc


--echo #
--echo # 2) Special indexes
--echo #
DROP TABLE t;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
CREATE TABLE t (id INT PRIMARY KEY, a INT, b VARCHAR(10), c INT, d JSON, UNIQUE INDEX uk_a((ABS(a))), INDEX uk_d((CAST(CAST(d AS JSON) AS UNSIGNED ARRAY)))) ENGINE = DuckDB;

--echo
--echo # functional index is not supported.
CREATE TABLE t(a INT, b INT, c INT, d INT, e INT, PRIMARY KEY(a) COMMENT 'primary key') ENGINE = DuckDB;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
eval ALTER TABLE t ADD INDEX uk_a((ABS(a))), ALGORITHM = $algorithm;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
CREATE UNIQUE INDEX uk_a ON t ((ABS(a)));
--error ER_DUCKDB_TABLE_STRUCT_INVALID
eval ALTER TABLE t ADD UNIQUE INDEX uk_d((CAST(CAST(d AS JSON) AS UNSIGNED ARRAY))), ALGORITHM = $algorithm;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
CREATE UNIQUE INDEX uk_d ON t ((CAST(CAST(d AS JSON) AS UNSIGNED ARRAY)));
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc

--echo
--echo # Spatial index is not supported for spatial types are not supported. 
DROP TABLE t;
--error ER_CHECK_NOT_IMPLEMENTED # The storage engine for the table doesn't support GEOMETRY
CREATE TABLE t (id INT PRIMARY KEY, a INT, b INT, c VARCHAR(10), d VARCHAR(10), e POINT, f POINT) ENGINE = DuckDB;

--echo
--echo # Fulltext index is ignored too.
CREATE TABLE t (id INT PRIMARY KEY, a INT, b INT, c VARCHAR(10), d VARCHAR(10)) ENGINE = DuckDB;
eval ALTER TABLE t ADD FULLTEXT INDEX idx_c(c) COMMENT 'index c', ALGORITHM = $algorithm;
CREATE FULLTEXT INDEX idx_d ON t(d) COMMENT 'index d';
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc

--echo
--echo # Foreign key is treated as normal index, which is ignored too.
DROP TABLE t;
CREATE TABLE t (id INT PRIMARY KEY, a INT, b INT) ENGINE = DuckDB;
CREATE TABLE t2 LIKE t;
eval ALTER TABLE t ADD FOREIGN KEY fk(b) REFERENCES t2(b), ALGORITHM = $algorithm;
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc


--echo #
--echo # 3) Unsupported
--echo #

--echo
--echo # create table without primary key is not supported.
DROP TABLE t;
--error ER_REQUIRES_PRIMARY_KEY
CREATE TABLE t (id INT, a INT, b INT) ENGINE = DuckDB;

--echo
--echo # drop PK is not supported.
CREATE TABLE t (a INT, b VARCHAR(10), c INT, PRIMARY KEY(a)) ENGINE = DuckDB;
--error ER_REQUIRES_PRIMARY_KEY
eval ALTER TABLE t DROP PRIMARY KEY, ALGORITHM = $algorithm;
--error ER_REQUIRES_PRIMARY_KEY
DROP INDEX `PRIMARY` ON t;

# Fix ha_duckdb::external_lock
if ($copy_ddl == 1) {
  # for COPY ddl, the table name is '#sql-*', so we skip this warning to fix.
  --disable_warnings
}
eval ALTER TABLE t ENABLE KEYS, ALGORITHM = $algorithm;
eval ALTER TABLE t DISABLE KEYS, ALGORITHM = $algorithm;
if ($copy_ddl == 1) {
  --enable_warnings
}

--echo #
--echo # 4) DESC, desc will be ignored by DuckDB
--echo #
DROP TABLE t;
CREATE TABLE t(id INT, name VARCHAR(255), PRIMARY KEY(id DESC)) ENGINE = DuckDB;
eval ALTER TABLE t ADD INDEX idx(name DESC), ALGORITHM = $algorithm;
INSERT INTO t VALUES(1, "1");
INSERT INTO t VALUES(2, "2");
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc


--echo #
--echo # 5) DROP PRIMARY KEY
--echo #
DROP TABLE t;
CREATE TABLE t(a INT PRIMARY KEY, b INT) ENGINE = DuckDB;
SET GLOBAL duckdb_require_primary_key = OFF;
eval ALTER TABLE t DROP PRIMARY KEY, ALGORITHM = $algorithm;
SHOW CREATE TABLE t;
--source $MYSQL_TMP_DIR/duckdb_index_meta.inc
SET GLOBAL duckdb_require_primary_key = default;


--echo #
--echo # 6) Cleanup
--echo #
DROP TABLE t, t2;
--remove_file $MYSQL_TMP_DIR/duckdb_index_meta.inc
