--source include/have_max_indexes_64.inc

###################### ps_general.test #######################
#                                                            #
#   basic and miscellaneous tests for prepared statements    #
#                                                            #
##############################################################

let $type= 'MYISAM' ;
# create the tables (t1 and t9) used in many tests
--source include/ps_create.inc
# insert data into these tables
--source include/ps_renew.inc

PREPARE stmt1 FROM ' EXPLAIN SELECT a FROM t1 ORDER BY b ';

# PS protocol gives slightly different metadata
--disable_ps_protocol
--enable_metadata
EXECUTE stmt1;
--disable_metadata
SET @arg00=1 ;
PREPARE stmt1 FROM ' EXPLAIN SELECT a FROM t1 WHERE a > ? ORDER BY b ';
--enable_metadata
EXECUTE stmt1 USING @arg00;
--disable_metadata
--enable_ps_protocol

DROP TABLE t1, t9;

###############################################
#                                             #
#  Prepared Statements test on MYISAM tables  #
#                                             #
###############################################

let $type= 'MYISAM' ;
# create the tables (t1 and t9) used in many tests
--source include/ps_create.inc
# insert data into these tables
--source include/ps_renew.inc

--source include/ps_query_explain_select.inc

DROP TABLE t1, t9;

###############################################
#                                             #
#  Prepared Statements test on InnoDB tables  #
#                                             #
###############################################

let $type= 'InnoDB' ;
-- source include/ps_create.inc
-- source include/ps_renew.inc

--source include/ps_query_explain_select.inc

DROP TABLE t1, t9;

###############################################
#                                             #
#   Prepared Statements test on HEAP tables   #
#                                             #
###############################################

let $type= 'HEAP' ;
eval CREATE TABLE t1
(
  a INT, b VARCHAR(30),
  PRIMARY KEY(a)
) ENGINE = $type ;

# The used table type doesn't support BLOB/TEXT columns.
# (The server would send error 1163  .)
# So we use char(100) instead.
eval CREATE TABLE t9
(
  c1  tinYINT, c2  SMALLINT, c3  MEDIUMINT, c4  INT,
  c5  INTEGER, c6  BIGINT, c7  FLOAT, c8  DOUBLE,
  c9  DOUBLE PRECISION, c10 REAL, c11 DECIMAL(7, 4), c12 NUMERIC(8, 4),
  c13 DATE, c14 DATETIME, c15 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
  ON UPDATE CURRENT_TIMESTAMP, c16 TIME,
  c17 YEAR, c18 TINYINT, c19 BOOL, c20 CHAR,
  c21 CHAR(10), c22 VARCHAR(30), c23 VARCHAR(100), c24 VARCHAR(100),
  c25 VARCHAR(100), c26 VARCHAR(100), c27 VARCHAR(100), c28 VARCHAR(100),
  c29 VARCHAR(100), c30 VARCHAR(100), c31 ENUM('one', 'two', 'three'),
  c32 SET('monday', 'tuesday', 'wednesday'),
  PRIMARY KEY(c1)
) ENGINE = $type ;

# insert data into these tables
--source include/ps_renew.inc
--source include/ps_query_explain_select.inc

DROP TABLE t1, t9;

###############################################
#                                             #
#  Prepared Statements test on MERGE tables   #
#                                             #
###############################################

#    
# NOTE: PLEASE SEE ps_1general.test (bottom) 
#       BEFORE ADDING NEW TEST CASES HERE !!!

--disable_warnings
DROP TABLE IF EXISTS t1, t1_1, t1_2, t9, t9_1, t9_2;
--enable_warnings
let $type= 'MYISAM' ;
-- source include/ps_create.inc
RENAME TABLE t1 TO t1_1, t9 TO t9_1 ;
-- source include/ps_create.inc
RENAME TABLE t1 TO t1_2, t9 TO t9_2 ;

CREATE TABLE t1
(
  a INT, b VARCHAR(30),
  PRIMARY KEY(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=FIRST;

CREATE TABLE t9
(
  c1  TINYINT, c2  SMALLINT, c3  MEDIUMINT, c4  INT,
  c5  INTEGER, c6  BIGINT, c7  FLOAT, c8  DOUBLE,
  c9  DOUBLE PRECISION, c10 REAL, c11 DECIMAL(7, 4), c12 NUMERIC(8, 4),
  c13 DATE, c14 DATETIME, c15 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, c16 TIME,
  c17 YEAR, c18 TINYINT, c19 BOOL, c20 CHAR,
  c21 CHAR(10), c22 VARCHAR(30), c23 TINYBLOB, c24 TINYTEXT,
  c25 BLOB, c26 TEXT, c27 MEDIUMBLOB, c28 MEDIUMTEXT,
  c29 LONGBLOB, c30 LONGTEXT, c31 ENUM('one', 'two', 'three'),
  c32 SET('monday', 'tuesday', 'wednesday'),
  PRIMARY KEY(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=FIRST;
-- source include/ps_renew.inc

-- source include/ps_query.inc
-- source include/ps_modify.inc
# no test of ps_modify1, because insert .. select 
# is not allowed on MERGE tables
# -- source include/ps_modify1.inc
-- source include/ps_conv.inc

# Lets's try the same tests with INSERT_METHOD=LAST
DROP TABLE t1, t9 ;
CREATE TABLE t1
(
  a int, b varchar(30),
  primary key(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=LAST;
CREATE TABLE t9
(
  c1  TINYINT, c2  SMALLINT, c3  MEDIUMINT, c4  INT,
  c5  INTEGER, c6  BIGINT, c7  FLOAT, c8  DOUBLE,
  c9  DOUBLE PRECISION, c10 REAL, c11 DECIMAL(7, 4), c12 NUMERIC(8, 4),
  c13 DATE, c14 DATETIME, c15 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, c16 TIME,
  c17 YEAR, c18 TINYINT, c19 BOOL, c20 CHAR,
  c21 CHAR(10), c22 VARCHAR(30), c23 TINYBLOB, c24 TINYTEXT,
  c25 BLOB, c26 TEXT, c27 MEDIUMBLOB, c28 MEDIUMTEXT,
  c29 LONGBLOB, c30 LONGTEXT, c31 ENUM('one', 'two', 'three'),
  c32 SET('monday', 'tuesday', 'wednesday'),
  PRIMARY KEY(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=LAST;
-- source include/ps_renew.inc
-- source include/ps_query_explain_select.inc
-- source include/ps_modify.inc
-- source include/ps_conv.inc

DROP TABLE t1, t1_1, t1_2, t9_1, t9_2, t9;

--echo "End of tests"
