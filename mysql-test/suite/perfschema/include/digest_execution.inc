--echo ####################################
--echo # EXECUTION
--echo ####################################

# -----------------------------------
# SQL Queries to test normalizations.
# -----------------------------------
SELECT 1 FROM t1;
SELECT 1 FROM `t1`;
SELECT 1,2 FROM t1;
SELECT 1, 2, 3, 4 FROM t1;
SELECT 1 FROM t2;
SELECT 1,2 FROM t2;
SELECT 1, 2, 3, 4 FROM t2;

# (NUM) => (#)
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);

# (NUM,NUM) => (#,#)
INSERT INTO t3 VALUES (1, 2);
INSERT INTO t4 VALUES (1, 2);
# (NUM,NUM,NUM) => (#,#)
INSERT INTO t5 VALUES (1, 2, 3);

# (NUM),(NUM) => (#),(#)
INSERT INTO t1 VALUES (1), (2), (3);
# (NUM),(NUM),(NUM) => (#),(#)
INSERT INTO t1 VALUES (1), (2), (3), (4);

# (NUM,NUM),(NUM,NUM) => (#,#),(#,#)
INSERT INTO t3 VALUES (1, 2), (3, 4), (5, 6);
# (NUM,NUM,NUM),(NUM,NUM,NUM),(NUM,NUM,NUM) => (#,#),(#,#)
INSERT INTO t5 VALUES (1, 2, 3), (4, 5, 6), (7, 8, 9);

# -----------------------------------------------------------------------
# Test case for handling spaces in statement.
# -----------------------------------------------------------------------

SELECT                                          1           +        1;

# -----------------------------------------------------------------------
# Test case for handling comments.
# -----------------------------------------------------------------------

# comment starting with "--"
# TODO : SELECT 1; -- This comment continues to the end of line
# comment starting from "#"
SELECT 1; # This comment continues to the end of line

# Inline comment
SELECT 1 /* This is an inline comment */ + 1;

# Multiple line comments
 SELECT 1+
 /*
 this is a
 multiple-line comment
 */
 1;


# -----------------------------------------------------------------------
# Tests to show how the digest behaves with tokens that can have multiple
# names (such as DATABASE = "DATABASE" or "SCHEMA", SUBSTRING, STD_SYM,
# VARIANCE_SYM ... )
# -----------------------------------------------------------------------

--disable_warnings
CREATE SCHEMA statements_digest_temp;
DROP SCHEMA statements_digest_temp;
CREATE DATABASE statements_digest_temp;
DROP DATABASE statements_digest_temp;
# TODO : add more
--enable_warnings

# -----------------------------------------------------------------------
# Test case to show stats for statements giving ERRORS/WARNINGS, are also
# captured.
# -----------------------------------------------------------------------
--ERROR ER_NO_SUCH_TABLE
SELECT 1 from t11;
create table t11 (c char(4));
--ERROR ER_TABLE_EXISTS_ERROR
create table t11 (c char(4));
insert into t11 values("MySQL");
