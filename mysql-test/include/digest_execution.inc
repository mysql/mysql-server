--echo ####################################
--echo # EXECUTION
--echo ####################################

# -----------------------------------
# SQL Queries to test normalizations.
# -----------------------------------
select 1 from t1;
select 1,2 from t1;
select 1, 2, 3, 4 from t1;
select 1 from t2;
select 1,2 from t2;
select 1, 2, 3, 4 from t2;

# (NUM) => (#)
insert into t1 values (1);
insert into t2 values (1);

# (NUM,NUM) => (#,#)
insert into t3 values (1, 2);
insert into t4 values (1, 2);
# (NUM,NUM,NUM) => (#,#)
insert into t5 values (1, 2, 3);

# (NUM),(NUM) => (#),(#)
insert into t1 values (1), (2), (3);
# (NUM),(NUM),(NUM) => (#),(#)
insert into t1 values (1), (2), (3), (4);

# (NUM,NUM),(NUM,NUM) => (#,#),(#,#)
insert into t3 values (1, 2), (3, 4), (5, 6);
# (NUM,NUM,NUM),(NUM,NUM,NUM),(NUM,NUM,NUM) => (#,#),(#,#)
insert into t5 values (1, 2, 3), (4, 5, 6), (7, 8, 9);

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
create schema statements_digest_temp;
drop schema statements_digest_temp;
create database statements_digest_temp;
drop database statements_digest_temp;
# TODO : add more
--enable_warnings

# -----------------------------------------------------------------------
# Tests to show how the digest tables behaves when it is full (special NULL
# entry) 
# -----------------------------------------------------------------------
# TODO:
