--echo
--echo # WL#16054 Remove support for prefix keys in partition functions.
--echo # Test added by WL#13588 Deprecate support for prefix partition keys
--echo # in deprecated_features.test are moved to this test file and modified
--echo # for feature removal
--echo

CREATE SCHEMA testdb;

--echo
--echo # Should give error on CREATE TABLE with partition prefix keys.

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t1 (
    a VARCHAR (10000),
    b VARCHAR (25),
    c VARCHAR (10),
    PRIMARY KEY (a(10),b,c(2))
) PARTITION BY KEY() PARTITIONS 2;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t2 (
    a VARCHAR (200),
    b VARCHAR (10),
    PRIMARY KEY (a(2),b)
) PARTITION BY KEY() PARTITIONS 2;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t3 (
    a VARCHAR (200),
    b VARCHAR (10),
    PRIMARY KEY (a(2),b)
) PARTITION BY KEY() PARTITIONS 10;

--echo
--echo # Should not give error if prefix key is not used
--echo # in the PARTITION BY KEY() clause.
CREATE TABLE testdb.t4 (
    a VARCHAR (200),
    b VARCHAR (10),
    c VARCHAR (100),
    KEY (a),
    KEY (b(5))
) PARTITION BY KEY(c) PARTITIONS 10;

--echo
--echo # Should give error if a table having prefix keys is
--echo # altered to be partitioned by key.
CREATE TABLE testdb.t5 (
    a VARCHAR (10000),
    b VARCHAR (25),
    c VARCHAR (10),
    PRIMARY KEY (a(10),b,c(2))
);
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
ALTER TABLE testdb.t5 PARTITION BY KEY() PARTITIONS 10;

--echo
--echo # Should not give error if prefix length = column length.
CREATE TABLE testdb.t6 (
    a VARCHAR (200),
    b VARCHAR (10),
    PRIMARY KEY (a(200),b)
) PARTITION BY KEY() PARTITIONS 10;

--echo
--echo # Should give error if prefix key is used in the
--echo # PARTITION BY KEY() clause, regardless of lettercase.
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t7 (
    a VARCHAR (200),
    b VARCHAR (10),
    c VARCHAR (100),
    KEY (a),
    KEY (b(5))
) PARTITION BY KEY(B) PARTITIONS 2;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t8 (
    A VARCHAR (200),
    B VARCHAR (10),
    C VARCHAR (100),
    KEY (A),
    KEY (B(5))
) PARTITION BY KEY(b) PARTITIONS 2;

--echo
--echo # Should not give error when there is no index.
CREATE TABLE testdb.m1 (
    firstname VARCHAR (25) NOT NULL,
    lastname VARCHAR (25) NOT NULL,
    username VARCHAR (16) NOT NULL,
    email VARCHAR (35),
    joined DATE NOT NULL
) PARTITION BY KEY(joined) PARTITIONS 6;

--echo
--echo # Should not give error for table partitioned by range.
CREATE TABLE testdb.m2 (
    firstname VARCHAR (25) NOT NULL,
    lastname VARCHAR (25) NOT NULL,
    username VARCHAR (16) NOT NULL,
    email VARCHAR (35),
    joined DATE NOT NULL
) PARTITION BY RANGE(YEAR(joined)) (
    PARTITION p0 VALUES LESS THAN (1960),
    PARTITION p1 VALUES LESS THAN (1970),
    PARTITION p2 VALUES LESS THAN (1980),
    PARTITION p3 VALUES LESS THAN (1990),
    PARTITION p4 VALUES LESS THAN MAXVALUE
);

--echo
--echo # Should not give error for table with a prefix key
--echo # partitioned by range
CREATE TABLE testdb.m3 (
    firstname VARCHAR (25) NOT NULL,
    lastname VARCHAR (25) NOT NULL,
    username VARCHAR (16) NOT NULL,
    email VARCHAR (35),
    joined DATE NOT NULL,
    PRIMARY KEY(firstname(5),joined)
) PARTITION BY RANGE(YEAR(joined)) (
    PARTITION p0 VALUES LESS THAN (1960),
    PARTITION p1 VALUES LESS THAN (1970),
    PARTITION p2 VALUES LESS THAN (1980),
    PARTITION p3 VALUES LESS THAN (1990),
    PARTITION p4 VALUES LESS THAN MAXVALUE
);

--echo
--echo # Should give error for table with a prefix key
--echo # on text field type as it is not supported
--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
CREATE TABLE testdb.m4 (
    f1 int(11) DEFAULT '0' NOT NULL,
    f2 varchar(16) DEFAULT '' NOT NULL,
    f5 text,
    PRIMARY KEY(f1,f2,f5(16))
) PARTITION BY KEY() PARTITIONS 2;

--echo
--echo # Should give error for table with a prefix key
--echo # on blob field type as it is not supported
--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
CREATE TABLE testdb.m4 (
    f1 int(11) DEFAULT '0' NOT NULL,
    f2 varchar(16) DEFAULT '' NOT NULL,
    f5 blob,
    PRIMARY KEY(f1,f2,f5(6))
) PARTITION BY KEY() PARTITIONS 2;


--echo
--echo # Should give not supported error for combinations of column type
--echo # CHAR|VARCHAR|BINARY|VARBINARY, [LINEAR] KEY, and ALGORITHM=1|2

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_char_linear_alg1 (
    prefix_col CHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY () PARTITIONS 3;

--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_char_linear_alg1 (
    prefix_col CHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varchar_linear_alg1 (
    prefix_col VARCHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_binary_linear_alg1 (
    prefix_col BINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varbinary_linear_alg1 (
    prefix_col VARBINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_char_nonlinear_alg1 (
    prefix_col CHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varchar_nonlinear_alg1 (
    prefix_col VARCHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_binary_nonlinear_alg1 (
    prefix_col BINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varbinary_nonlinear_alg1 (
    prefix_col VARBINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=1 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_char_linear_alg2 (
    prefix_col CHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varchar_linear_alg2 (
    prefix_col VARCHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_binary_linear_alg2 (
    prefix_col BINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varbinary_linear_alg2 (
    prefix_col VARBINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY LINEAR KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_char_nonlinear_alg2 (
    prefix_col CHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varchar_nonlinear_alg2 (
    prefix_col VARCHAR (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_binary_nonlinear_alg2 (
    prefix_col BINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--error ER_PARTITION_PREFIX_KEY_NOT_SUPPORTED
CREATE TABLE testdb.t_varbinary_nonlinear_alg2 (
    prefix_col VARBINARY (100),
    other_col VARCHAR (5),
    PRIMARY KEY (prefix_col(10), other_col)
) PARTITION BY KEY ALGORITHM=2 () PARTITIONS 3;

--echo
--echo # Cleanup.

--echo
DROP SCHEMA testdb;
