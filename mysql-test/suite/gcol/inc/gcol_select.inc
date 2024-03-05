################################################################################
# inc/gcol_select.inc                                                          #
#                                                                              #
# Purpose:                                                                     #
#  Testing different SELECTs.                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-18                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

# Table t1 is used below to test:
#  - Join type of ALL (sequential scan of the entire table)
#  - Join type of Index
#  - Join type of Range
#  - Join type of Ref_or_null
create table t1 (a int,
                 b int generated always as (-a) virtual,
                 c int generated always as (-a) stored,
                 index (c));
insert into t1 (a) values (2), (1), (1), (3), (NULL);

# Table t2 is used below to test:
#  - Join type of system and const
create table t2 like t1;
insert into t2 (a) values (1);

# Table t3 is used below to test
#  - Join type of Eq_ref with a unique generated column
#  - Join type of Const
create table t3 (a int primary key, 
                 b int generated always as (-a) virtual,
                 c int generated always as (-a) stored unique);
insert into t3 (a) values (2),(1),(3);
analyze table t1,t2,t3;

--echo # select_type=SIMPLE, type=system
let $s = select * from t2;
eval $s;
eval explain $s;

let $s = select * from t2 where c=-1;
eval $s;
eval explain $s;

--echo # select_type=SIMPLE, type=ALL
let $s = select * from t1 where b=-1;
eval $s;
eval explain $s;

--echo # select_type=SIMPLE, type=const
let $s = select * from t3 where a=1;
eval $s;
eval explain $s;

--echo # select_type=SIMPLE, type=range
let $s = select * from t3 where c>=-1;
eval $s;
eval explain $s;

--echo # select_type=SIMPLE, type=ref
let $s = select * from t1,t3 where t1.c=t3.c and t3.c=-1;
eval $s;
eval explain $s;

--echo # select_type=PRIMARY, type=index,ALL
let $s = select * from t1 where b in (select c from t3);
--sorted_result
eval $s;
eval explain $s;

--echo # select_type=PRIMARY, type=range,ref
--sorted_result
let $s = select * from t1 where c in (select c from t3 where c between -2 and -1);
eval $s;
eval explain $s;

--echo # select_type=UNION, type=system
--echo # select_type=UNION RESULT, type=<union1,2>
let $s = select * from t1 union select * from t2;
--sorted_result
eval $s;
eval explain $s;

--echo # select_type=DERIVED, type=system
let $s = select * from (select a,b,c from t1) as t11;
--sorted_result
eval $s;
eval explain $s;

--echo ###
--echo ### Using aggregate functions with/without DISTINCT
--echo ###
--echo # SELECT COUNT(*) FROM tbl_name
let $s = select count(*) from t1;
eval $s;
eval explain $s;

--echo # SELECT COUNT(DISTINCT <non-gcol>) FROM tbl_name
let $s = select count(distinct a) from t1;
eval $s;
eval explain $s;

--echo # SELECT COUNT(DISTINCT <non-stored gcol>) FROM tbl_name
let $s = select count(distinct b) from t1;
eval $s;
eval explain $s;

--echo # SELECT COUNT(DISTINCT <stored gcol>) FROM tbl_name
let $s = select count(distinct c) from t1;
eval $s;
eval explain $s;

--echo ###
--echo ### filesort & range-based utils
--echo ###
--echo # SELECT * FROM tbl_name WHERE <gcol expr>
let $s = select * from t3 where c >= -2;
--sorted_result
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-gcol expr>
let $s = select * from t3 where a between 1 and 2;
--sorted_result
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed gcol expr>
let $s = select * from t3 where b between -2 and -1;
--sorted_result
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed gcol expr>
let $s = select * from t3 where c between -2 and -1;
--sorted_result
eval $s;
eval explain $s;

#### Remove for MyISAM due to a bug
#### when all the three records are returned (a=1,2,3) 
#### instead of just two (a=1,2).
#### This bug is presumably in base SQL routines as the same happens
#### with this table:
####   create table t4 (a int primary key, b int, c int unique);
let $myisam_engine = `SELECT @@session.default_storage_engine='myisam'`;
if (!$myisam_engine)
{
  --echo # SELECT * FROM tbl_name WHERE <non-gcol expr> ORDER BY <non-indexed gcol>
  let $s = select * from t3 where a between 1 and 2 order by b;
  eval $s;
  eval explain $s; 
  --echo # bug#20022189: WL411:DEBUG ASSERT AT FIELD_LONG::VAL_INT IN SQL/FIELD.CC
  --echo # SELECT * FROM tbl_name WHERE <non-gcol expr> ORDER BY <non-indexed stored gcol>
  let $s = select * from t3 where a between 1 and 2 order by c;
  eval $s;
  eval explain $s;
}
--echo # bug#20022189: WL411:DEBUG ASSERT AT FIELD_LONG::VAL_INT IN SQL/FIELD.CC
CREATE TABLE t4 (
  `pk` int(11) NOT NULL ,
  `col_int_nokey` int(11) GENERATED ALWAYS AS (pk + col_int_key) STORED,
  `col_int_key` int(11) DEFAULT NULL,
  `col_date_nokey` date DEFAULT NULL,
  `col_datetime_key` datetime DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_int_key` (`col_int_key`),
  KEY `col_datetime_key` (`col_datetime_key`)
);

INSERT INTO t4 VALUES
(1,default,4,'2008-12-05','1900-01-01 00:00:00');

SELECT
SQL_BIG_RESULT 
GRANDPARENT1 . `col_int_nokey` AS g1
FROM t4 AS GRANDPARENT1 LEFT JOIN t4 AS GRANDPARENT2 ON ( GRANDPARENT2 .
`col_datetime_key` <= GRANDPARENT1 . `col_date_nokey` )
GROUP BY GRANDPARENT1 . `pk`;
DROP TABLE t4;

--echo # SELECT * FROM tbl_name WHERE <non-gcol expr> ORDER BY <indexed gcol>
let $s = select * from t3 where a between 1 and 2 order by c;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed gcol expr> ORDER BY <non-gcol>
let $s = select * from t3 where b between -2 and -1 order by a;
eval $s;
eval explain $s;

#### Remove for MyISAM due to a bug
#### when all the three records are returned (a=1,2,3) 
#### instead of just two (a=1,2).
#### This bug is presumably in base SQL routines as the same happens
#### with this table:
####   create table t4 (a int primary key, b int, c int unique);
let $innodb_engine = `SELECT @@session.default_storage_engine='innodb'`;
if (!$innodb_engine)
{
  --echo # SELECT * FROM tbl_name WHERE <indexed gcol expr> ORDER BY <non-gcol>
  let $s = select * from t3 where c between -2 and -1 order by a;
  eval $s;
  eval explain $s;
}

--echo # SELECT * FROM tbl_name WHERE <non-indexed gcol expr> ORDER BY <non-indexed gcol>
let $s = select * from t3 where b between -2 and -1 order by b;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed gcol expr> ORDER BY <non-indexed gcol>
let $s = select * from t3 where c between -2 and -1 order by b;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed gcol expr> ORDER BY <indexed gcol>
let $s = select * from t3 where b between -2 and -1 order by c;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed gcol expr> ORDER BY <indexed gcol>
let $s = select * from t3 where c between -2 and -1 order by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<non-indexed gcol>) FROM tbl_name GROUP BY <non-indexed gcol>
--sorted_result
let $s = select sum(b) from t1 group by b;
eval $s;
eval explain $s;

--echo # SELECT sum(<indexed gcol>) FROM tbl_name GROUP BY <indexed gcol>
let $s = select sum(c) from t1 group by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<non-indexed gcol>) FROM tbl_name GROUP BY <indexed gcol>
let $s = select sum(b) from t1 group by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<indexed gcol>) FROM tbl_name GROUP BY <non-indexed gcol>
--sorted_result
let $s = select sum(c) from t1 group by b;
eval $s;
eval explain $s;

drop table t1;

--echo #
--echo # Bug#20241655: WL411:FAILING ASSERTION ASSERTION 
--echo # 
CREATE TABLE BB (
  col_time_key time NOT NULL,
  col_time_nokey time GENERATED ALWAYS AS (ADDTIME(col_datetime_key, col_time_key)) VIRTUAL,
  col_datetime_key datetime NOT NULL);
INSERT INTO BB VALUES('23:28:02', default, '2005-03-15 22:48:25');

CREATE TABLE CC (
  col_time_key time NOT NULL,
  col_time_nokey time GENERATED ALWAYS AS (ADDTIME(col_datetime_key, col_time_key)) VIRTUAL,
  col_datetime_key datetime NOT NULL
);
INSERT INTO CC VALUES('16:22:51', default, '1900-01-01 00:00:00');

SELECT 1 AS g1 FROM BB AS gp1 LEFT JOIN BB AS gp2 USING ( col_time_nokey);
DROP TABLE BB, CC;

--echo #
--echo # Bug#20328786: WL411:VALGRIND WARNINGS OF CONDITIONAL
--echo #               JUMP WHILE SELECTING FROM VIEW
--echo # 
CREATE TABLE A (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER,
  col_int_key INTEGER GENERATED ALWAYS AS (2 + 2 + col_int_nokey) STORED,
  PRIMARY KEY (pk)
);

CREATE TABLE C (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER,
  col_int_key INTEGER GENERATED ALWAYS AS (2 + 2 + col_int_nokey) STORED,
  col_varchar_nokey VARCHAR(1),
  col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
  (CONCAT(col_varchar_nokey, col_varchar_nokey)) STORED,
  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_varchar_key, col_int_key)
);

INSERT INTO C (
  col_int_nokey,
  col_varchar_nokey
) VALUES (4, 'v'),(62, 'v'),(7, 'c'),(1, NULL),(0, 'x'),(7, 'i'),(7, 'e'),(1, 'p'),(7, 's'),(1, 'j'),(5, 'z'),(2, 'c'),(0, 'a'),(1, 'q'),(8, 'y'),(1, NULL),(1, 'r'),(9, 'v'),(1, NULL),(5, 'r');

CREATE OR REPLACE ALGORITHM=MERGE VIEW V1 AS SELECT alias1.
col_varchar_key AS field1 , alias1.pk AS field2, alias2.
col_int_nokey AS field3 FROM  C AS alias1  LEFT  JOIN A AS alias2 ON
alias1.pk =  alias2.col_int_key  WHERE  alias1.pk > 8 AND alias1
.pk < ( 9 + 2 ) AND  alias1.col_int_key <> 1 OR alias1.col_int_key
> 0 AND alias1.col_int_key <= ( 3 + 2 )  ORDER BY field1, field2, field3
LIMIT 100 OFFSET 6;

SELECT * FROM V1;

DROP VIEW V1;
DROP TABLE A,C;

--echo #
--echo # Bug#20406510: WL411:VALGRIND WARNINGS WITH
--echo #    COUNT DISTINCT QUERY ON VIRTUAL GC VARCHAR COLUMN
--echo # 
CREATE TABLE A (
  pk INTEGER AUTO_INCREMENT,
  col_time_key TIME NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  PRIMARY KEY (pk),
  KEY (col_time_key),
  KEY (col_datetime_key)
);

CREATE TABLE C (
  pk INTEGER AUTO_INCREMENT,
  col_int_key INTEGER NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(2) GENERATED ALWAYS AS
  (CONCAT(col_varchar_key, col_varchar_key)),
  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_varchar_key, col_int_key)
);

INSERT INTO C (col_int_key,col_varchar_key) VALUES (0, 'j'),(8, 'v'),(1, 'c'),(8, 'm'),(9, 'd');
SELECT MIN(  alias2 . col_int_key ) AS field1,
COUNT( DISTINCT alias2 . col_varchar_nokey ) AS field2
FROM ( A AS alias1 , C AS alias2 )
ORDER BY alias1.col_time_key, alias1.col_datetime_key, alias1.pk ASC;
DROP TABLE A,C;

--echo #
--echo # Bug#20566325: WL8149: INNODB: FAILING ASSERTION:
--echo #               COL_NR < TABLE->N_DEF
--echo #
CREATE TABLE A (
pk INTEGER AUTO_INCREMENT,
col_varchar_nokey VARCHAR(1) NOT NULL,
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
PRIMARY KEY (pk)
);

INSERT /*! IGNORE */ INTO A (col_varchar_nokey) VALUES ('k');

CREATE TABLE CC (
pk INTEGER AUTO_INCREMENT,
col_datetime_nokey DATETIME /*! NULL */,
col_time_nokey TIME /*! NULL */,
col_time_key TIME GENERATED ALWAYS AS
(ADDTIME(col_datetime_nokey, col_time_nokey)),
col_varchar_nokey VARCHAR(1) /*! NULL */,
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
PRIMARY KEY (pk));

INSERT INTO CC (col_time_nokey,col_datetime_nokey,col_varchar_nokey) VALUES
('13:06:13.033877','1900-01-01 00:00:00', 'p'),
(NULL, '2007-05-25 11:58:54.015689', 'g');

SELECT
table1.col_time_key AS field1,
'z' AS field2
FROM
(CC AS table1 LEFT OUTER JOIN (A AS table2 STRAIGHT_JOIN CC AS table3 ON
(table3.col_varchar_key = table2.col_varchar_nokey)) ON
(table3.col_varchar_key = table2.col_varchar_nokey))
WHERE
table2.pk != 6
AND table1.col_varchar_key IN ('l', 's' , 'b' )
AND table3.col_varchar_key != table1.col_varchar_key
ORDER BY table1.col_varchar_key , field1 , field2;

DROP TABLE A,CC;

if ($support_virtual_index)
{
--echo #
--echo # Bug#20573302: WL8149: SEGV IN HA_INNOBASE::
--echo #               BUILD_TEMPLATE AT INNOBASE/HANDLER/HA_INNODB.CC:665
--echo #
CREATE TABLE c (
                        pk INTEGER AUTO_INCREMENT,
                        col_int_nokey INTEGER NOT NULL,
                        col_int_key INTEGER GENERATED ALWAYS AS (col_int_nokey) VIRTUAL,

                        col_date_nokey DATE NOT NULL,
                        col_date_key DATE GENERATED ALWAYS AS (DATE_ADD(col_date_nokey,interval 30 day)) VIRTUAL,

                        col_datetime_nokey DATETIME NOT NULL,
                        col_time_nokey TIME NOT NULL,

                        col_datetime_key DATETIME GENERATED ALWAYS AS (ADDTIME(col_datetime_nokey, col_time_nokey)),
                        col_time_key TIME GENERATED ALWAYS AS (ADDTIME(col_datetime_nokey, col_time_nokey)),

                        col_varchar_nokey VARCHAR(1) NOT NULL,
                        col_varchar_key VARCHAR(2) GENERATED ALWAYS AS (CONCAT(col_varchar_nokey, col_varchar_nokey)),

                        PRIMARY KEY (pk),
                        KEY (col_int_key),
                        KEY (col_varchar_key),
                        KEY (col_date_key),
                        KEY (col_time_key),
                        KEY (col_datetime_key),
                        KEY (col_int_key, col_varchar_key),
                        KEY (col_int_key, col_varchar_key, col_date_key,
                             col_time_key, col_datetime_key));

INSERT /*! IGNORE */ INTO c (
                                col_int_nokey,
                                col_date_nokey,
                                col_time_nokey,
                                col_datetime_nokey,
                                col_varchar_nokey
                        ) VALUES
(1, '2009-12-01', '00:21:38.058143', '2007-05-28 00:00:00', 'c'),
(8, '2004-12-17', '04:08:02.046897', '2009-07-25 09:21:20.064099', 'm'),
(9, '2000-03-14', '16:25:11.040240', '2002-01-16 00:00:00', 'd'),
(24, '2000-10-08', '10:14:58.018534', '2006-10-12 04:32:53.031976', 'd'),
(6, '2006-05-25', '19:47:59.011283', '2001-02-15 03:08:38.035426', 'y'),
(1, '2008-01-23', '11:14:24.032949', '2004-10-02 20:31:15.022553', 't'),
(6, '2007-06-18', NULL, '2002-08-20 22:48:00.035785', 'd'),
(2, '2002-10-13', '00:00:00', '1900-01-01 00:00:00', 's'),
(4, '1900-01-01', '15:57:25.019666', '2005-08-15 00:00:00', 'r'),
(8, NULL, '07:05:51.006712', '1900-01-01 00:00:00', 'm'),
(4, '2006-03-09', '19:22:21.057406', '2008-05-16 08:09:06.002924', 'b'),
(4, '2001-06-05', '03:53:16.001370', '2001-01-20 12:47:23.022022', 'x'),
(7, '2006-05-28', '09:16:38.034570', '2008-07-02 00:00:00', 'g'),
(4, '2001-04-19', '15:37:26.028315', '1900-01-01 00:00:00', 'p'),
(1, '1900-01-01', '00:00:00', '2002-12-08 11:34:58.001571', 'q'),
(9, '2004-08-20', '05:03:03.047452', '1900-01-01 00:00:00', 'w'),
(4, '2004-10-10', '02:59:24.063764', '1900-01-01 00:00:00', 'd'),
(8, '2000-04-02', '00:01:58.064243', '2002-08-25 20:35:06.064634', 'e'),
(4, '2006-11-02', '00:00:00', '2001-10-22 11:13:24.048128', 'b'),
(8, '2009-01-28', '02:20:16.024931', '2003-03-12 02:00:34.029335', 'y');

CREATE TABLE cc (
                        pk INTEGER AUTO_INCREMENT,
                        col_int_nokey INTEGER NOT NULL,
                        col_int_key INTEGER GENERATED ALWAYS AS (col_int_nokey) VIRTUAL,

                        col_date_nokey DATE NOT NULL,
                        col_date_key DATE GENERATED ALWAYS AS (DATE_ADD(col_date_nokey,interval 30 day)) VIRTUAL,

                        col_datetime_nokey DATETIME NOT NULL,
                        col_time_nokey TIME NOT NULL,

                        col_datetime_key DATETIME GENERATED ALWAYS AS (ADDTIME(col_datetime_nokey, col_time_nokey)),
                        col_time_key TIME GENERATED ALWAYS AS (ADDTIME(col_datetime_nokey, col_time_nokey)),

                        col_varchar_nokey VARCHAR(1) NOT NULL,
                        col_varchar_key VARCHAR(2) GENERATED ALWAYS AS (CONCAT(col_varchar_nokey, col_varchar_nokey)),

                        PRIMARY KEY (pk),
                        KEY (col_int_key),
                        KEY (col_varchar_key),
                        KEY (col_date_key),
                        KEY (col_time_key),
                        KEY (col_datetime_key),
                        KEY (col_int_key, col_varchar_key),
                        KEY (col_int_key, col_varchar_key, col_date_key,
                             col_time_key, col_datetime_key));

INSERT /*! IGNORE */ INTO cc (
                                col_int_nokey,
                                col_date_nokey,
                                col_time_nokey,
                                col_datetime_nokey,
                                col_varchar_nokey
                        ) VALUES
(0, '2003-02-06', '22:02:09.059926', '2003-08-07 14:43:09.011144', 'x'),
(0, '2005-04-16', '19:33:15.014160', '2005-12-11 00:00:00', 'n'),
(1, '2005-07-23', '22:03:16.058787', '2005-12-26 20:48:07.043628', 'w'),
(7, '2001-11-15', '06:31:23.027263', '2008-06-12 06:41:21.012493', 's'),
(0, '2006-03-24', '02:19:08.013275', '2007-10-11 18:46:28.030000', 'a'),
(4, '2008-07-10', NULL, '2006-04-04 22:22:40.057947', 'd'),
(1, '2009-12-07', NULL, '2002-08-10 20:52:58.035137', 'w'),
(1, '2008-05-01', '10:28:01.038587', '2008-10-03 11:17:23.005299', 'j'),
(1, '2008-06-22', '00:00:00', '2009-01-06 20:11:01.034339', 'm'),
(4, '2001-11-11', '15:02:50.048785', '2009-09-19 00:00:00', 'k'),
(7, '2000-12-21', '05:29:13.012729', '2007-09-02 12:14:27.029187', 't'),
(4, '2007-09-03', '23:45:33.048507', '2003-09-26 00:00:00', 'k'),
(2, '2003-02-18', '19:10:53.057455', '2001-11-18 18:10:16.063189', 'e'),
(0, '2008-12-01', '01:45:27.037313', '2005-02-15 04:08:17.015554', 'i'),
(1, '2008-10-18', '03:56:03.060218', '2009-06-13 23:04:40.013006', 't'),
(91, '2004-08-28', '12:43:17.023797', '1900-01-01 00:00:00', 'm'),
(6, '2006-10-05', '13:33:46.053634', '2005-03-20 02:48:24.045653', 'z'),
(3, '2003-05-16', NULL, '2002-03-16 11:47:27.045297', 'c'),
(6, '2008-10-10', NULL, '2000-05-22 00:00:00', 'i'),
(8, '2002-01-19', '05:18:40.006865', '2009-02-12 00:00:00', 'v');

--disable_query_log
--disable_result_log
ANALYZE TABLE c, cc;
--enable_query_log
--enable_result_log
--replace_column 10 # 11 #
EXPLAIN
SELECT subquery2_t2.col_int_key AS subquery2_field1
FROM (c AS subquery2_t1 RIGHT JOIN
      (c AS subquery2_t2 LEFT JOIN cc AS subquery2_t3 ON
      (subquery2_t3.col_int_nokey = subquery2_t2.col_int_key )) ON
      (subquery2_t3.col_varchar_key = subquery2_t2.col_varchar_key))
ORDER BY subquery2_field1;

SELECT subquery2_t2.col_int_key AS subquery2_field1
FROM (c AS subquery2_t1 RIGHT JOIN
      (c AS subquery2_t2 LEFT JOIN cc AS subquery2_t3 ON
      (subquery2_t3.col_int_nokey = subquery2_t2.col_int_key )) ON
      (subquery2_t3.col_varchar_key = subquery2_t2.col_varchar_key))
ORDER BY subquery2_field1;
SELECT subquery2_t2.col_int_key AS subquery2_field1
FROM (c AS subquery2_t1 RIGHT JOIN
      (c AS subquery2_t2 LEFT JOIN cc AS subquery2_t3 ON
      (subquery2_t3.col_int_nokey = subquery2_t2.col_int_key )) ON
      (subquery2_t3.col_varchar_key = subquery2_t2.col_varchar_key))
ORDER BY subquery2_field1;

DROP TABLE c,cc;

--echo #
--echo # Bug#2081065: WL8149:RESULT DIFF SEEN FOR SIMPLE
--echo #              RANGE QUERIES WITH ORDER BY
--echo #
CREATE TABLE cc (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER NOT NULL,
  col_int_key INTEGER GENERATED ALWAYS AS
  (col_int_nokey + col_int_nokey) VIRTUAL,
  PRIMARY KEY (pk),
  KEY (col_int_key)
);
INSERT INTO cc (col_int_nokey) VALUES (0),(1),(7),(0),(4),(5);
--disable_result_log
--disable_query_log
ANALYZE TABLE cc;
--enable_result_log
--enable_query_log
--replace_column 10 # 11 #
EXPLAIN SELECT pk FROM cc WHERE col_int_key > 3;
--sorted_result
SELECT pk FROM cc WHERE col_int_key > 3;
--replace_column 10 # 11 #
EXPLAIN SELECT pk FROM cc WHERE col_int_key > 3 ORDER BY 1;
SELECT pk FROM cc WHERE col_int_key > 3 ORDER BY 1;
DROP TABLE cc;

--echo #
--echo # Bug#20849676 :WL8149:ASSERTION `!TABLE || (!TABLE->READ_SET
--echo #  || BITMAP_IS_SET(TABLE->READ_SET
--echo #
CREATE TABLE c (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER NOT NULL,
  col_int_key INTEGER GENERATED ALWAYS AS
  (col_int_nokey + col_int_nokey) VIRTUAL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
  (CONCAT(col_varchar_nokey, col_varchar_nokey)),
  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_varchar_key),
  KEY (col_int_key, col_varchar_key)
) ;

INSERT INTO c (col_int_nokey, col_varchar_nokey) VALUES
(1, 'c'),(8, 'm'),(9, 'd'),(24, 'd'),(6, 'y'),(1, 't'),(6, 'd'),
(2, 'r'),(8, 'm'),(4, 'b'),(4, 'x'),(7, 'g'),(4, 'p'),(1, 'q'),
(9, 'w'),(4, 'd'),(8, 'e'),(4, 'b'),(8, 'y');

CREATE TABLE a (
  pk INTEGER AUTO_INCREMENT,
  col_datetime_nokey DATETIME NOT NULL,
  col_time_nokey TIME NOT NULL,
  col_datetime_key DATETIME GENERATED ALWAYS AS
  (ADDTIME(col_datetime_nokey, col_time_nokey)),
  col_time_key TIME GENERATED ALWAYS AS
  (ADDTIME(col_datetime_nokey, col_time_nokey)),
  col_varchar_nokey VARCHAR(1) NOT NULL,
  col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
  (CONCAT(col_varchar_nokey, col_varchar_nokey)),
  PRIMARY KEY (pk),
  KEY (col_varchar_key),
  KEY (col_time_key),
  KEY (col_datetime_key),
  KEY (col_varchar_key, col_time_key, col_datetime_key)
);

INSERT INTO a (
  col_time_nokey,
  col_datetime_nokey,
  col_varchar_nokey) VALUES
('04:08:02.046897', '2001-11-04 19:07:55.051133', 'k');

ANALYZE TABLE a, c;

--replace_column 10 #
--disable_warnings
EXPLAIN
SELECT
table1.pk AS field1 ,
table1.col_datetime_key AS field2
FROM
( a AS table1 LEFT JOIN ( ( c AS table2 STRAIGHT_JOIN (  SELECT
SUBQUERY1_t1.* FROM ( c AS SUBQUERY1_t1 INNER JOIN ( c AS SUBQUERY1_t2
STRAIGHT_JOIN c AS SUBQUERY1_t3 ON (SUBQUERY1_t3.col_varchar_key =
SUBQUERY1_t2.col_varchar_key  ) )
ON (SUBQUERY1_t3.pk = SUBQUERY1_t2.col_int_key
OR SUBQUERY1_t1.col_int_key <> 1 ) )
WHERE SUBQUERY1_t2.pk >= 9 ) AS table3
ON (table3.col_int_key = table2.col_int_key  ) ) )
ON (table3.col_int_nokey = table2.pk  ) )
GROUP BY field1, field2;
SELECT
table1.pk AS field1 ,
table1.col_datetime_key AS field2
FROM
( a AS table1 LEFT JOIN ( ( c AS table2 STRAIGHT_JOIN (  SELECT
SUBQUERY1_t1.* FROM ( c AS SUBQUERY1_t1 INNER JOIN ( c AS SUBQUERY1_t2
STRAIGHT_JOIN c AS SUBQUERY1_t3 ON (SUBQUERY1_t3.col_varchar_key =
SUBQUERY1_t2.col_varchar_key  ) )
ON (SUBQUERY1_t3.pk = SUBQUERY1_t2.col_int_key
OR SUBQUERY1_t1.col_int_key <> 1 ) )
WHERE SUBQUERY1_t2.pk >= 9 ) AS table3
ON (table3.col_int_key = table2.col_int_key  ) ) )
ON (table3.col_int_nokey = table2.pk  ) )
GROUP BY field1, field2;

--enable_warnings
DROP TABLE IF EXISTS c,a;
CREATE TABLE c (
col_int_nokey INTEGER NOT NULL,
col_int_key INTEGER GENERATED ALWAYS AS 
  (col_int_nokey + col_int_nokey) VIRTUAL,
col_varchar_nokey VARCHAR(1) NOT NULL,
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
  (CONCAT(col_varchar_nokey, col_varchar_nokey)),

KEY (col_int_key),
KEY (col_int_key, col_varchar_key)
) ;

INSERT INTO c (
col_int_nokey,
col_varchar_nokey
) VALUES (1, 'c'),(8, 'm'),(9, 'd'),(24, 'd'),(6, 'y'),(1, 't'),
(6, 'd'),(2, 's'),(4, 'r'),(8, 'm'),(4, 'b'),(4, 'x'),(7, 'g'),(4, 'p'),
(1, 'q'),(9, 'w'),(4, 'd'),(8, 'e'),(4, 'b'),(8, 'y');

CREATE TABLE cc (
col_int_nokey INTEGER,
col_int_key INTEGER GENERATED ALWAYS AS
(col_int_nokey + col_int_nokey) VIRTUAL,
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
KEY (col_int_key),
KEY (col_varchar_key),
KEY (col_int_key, col_varchar_key),
KEY (col_int_key, col_int_nokey),
KEY (col_varchar_key, col_varchar_nokey)
);
INSERT INTO cc (
col_int_nokey,
col_varchar_nokey
) VALUES (8, 'p'),(9, 'g'),(9, 'i'),(4, 'p'),(7, 'h'),(1, 'e'),(8, 'e'),(6, 'u'),
(6, 'j'),(6, 'e'),(1, 'z'),(227, 'w'),(NULL, 't'),(9, 'i'),(1, 'i'),(8, 'i'),
(5, 'b'),(8,'m'),(7, 'j'),(2, 'v');
ANALYZE TABLE c, cc;

--replace_column 10 #
--disable_warnings

let query=SELECT
alias2 . col_varchar_key AS field1
FROM ( cc AS alias1 , cc AS alias2 )
WHERE
( alias2 . col_int_key , alias1 . col_int_nokey )
NOT IN
(
SELECT
DISTINCT  SQ1_alias2 . col_int_nokey AS SQ1_field1 ,
SQ1_alias1 . col_int_key AS SQ1_field2
FROM ( cc AS SQ1_alias1 , c AS SQ1_alias2 )
GROUP BY SQ1_field1 , SQ1_field2
)
GROUP BY field1;

eval EXPLAIN $query;
--sorted_result
eval $query;

DROP TABLE IF EXISTS c,cc;

SET @save_old_sql_mode= @@sql_mode;
SET sql_mode="";
CREATE TABLE d (
  col_int int(11) DEFAULT NULL,
  col_varchar_10_utf8mb3 varchar(10) CHARACTER SET utf8mb3 DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) GENERATED ALWAYS AS (col_int+col_int) VIRTUAL,
  col_varchar_10_utf8mb3_key varchar(10) CHARACTER SET utf8mb3 GENERATED ALWAYS AS (REPEAT(SUBSTRING(col_varchar_10_utf8mb3, -1), 5)) VIRTUAL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_10_utf8mb3_key (col_varchar_10_utf8mb3_key),
  KEY cover_key1 (col_int_key, col_varchar_10_utf8mb3_key)
);

INSERT INTO d (col_int, col_varchar_10_utf8mb3) VALUES  ('qhlhtrovam',1),('how',2),('htrovamzqr',3),('rovamzqrdc',4),('well',5),('g',6),('rdcenchyhu',7),('want',8);

SELECT table1.pk AS field1 FROM d AS table1 LEFT JOIN d AS table2 ON table1.col_varchar_10_utf8mb3_key = table2.col_varchar_10_utf8mb3_key WHERE table1.col_int_key IS NULL GROUP BY table1.pk ;

DROP TABLE d;

--echo #
--echo # Bug#21153237: WL8149: QUERIES USING FILESORT
--echo #   ON VIRTUAL GC HAVING INDEX GIVES WRONG RESULTS
--echo #
CREATE TABLE j (
col_int int(11),
pk int(11) NOT NULL,
col_varchar_10_utf8mb3 varchar(10) CHARACTER SET utf8mb3 DEFAULT NULL,
col_varchar_255_utf8mb3_key varchar(255) CHARACTER SET utf8mb3 GENERATED ALWAYS AS 
(col_varchar_10_utf8mb3) VIRTUAL,
PRIMARY KEY (pk),
KEY cover_key1 (col_int, col_varchar_255_utf8mb3_key));

INSERT INTO j(col_int, pk, col_varchar_10_utf8mb3) VALUES(9, 1, '951910400'),
(-1934295040, 2, '1235025920'),(-584581120, 3, '-1176633344'),(3, 4, '1074462720');
--disable_query_log
--disable_result_log
ANALYZE TABLE j;
--enable_query_log
--enable_result_log

--replace_column 10 #
EXPLAIN SELECT col_varchar_255_utf8mb3_key FROM j ORDER BY 1;
SELECT col_varchar_255_utf8mb3_key FROM j ORDER BY col_varchar_255_utf8mb3_key;

DROP TABLE j;

set sql_mode= @save_old_sql_mode;
--enable_warnings
}

CREATE TABLE cc (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_nokey int(11) NOT NULL,
  col_int_key int(11) GENERATED ALWAYS AS (col_int_nokey) STORED,
  col_date_nokey date NOT NULL,
  col_date_key date GENERATED ALWAYS AS (col_date_nokey) STORED,
  col_datetime_nokey datetime NOT NULL,
  col_time_nokey time NOT NULL,
  col_datetime_key datetime GENERATED ALWAYS AS (col_datetime_nokey)STORED,
  col_time_key time GENERATED ALWAYS AS (col_time_nokey) STORED,
  col_varchar_nokey varchar(1) NOT NULL,
  col_varchar_key varchar(1) GENERATED ALWAYS AS (col_varchar_nokey)STORED,
  PRIMARY KEY (pk),
  KEY gc_idx1 (col_int_key),
  KEY gc_idx2 (col_varchar_key),
  KEY gc_idx3 (col_date_key),
  KEY gc_idx4 (col_time_key),
  KEY gc_idx5 (col_datetime_key),
  KEY gc_idx6 (col_varchar_key,col_int_key),
  KEY gc_idx7 (col_date_key,col_datetime_key,col_time_key),
  KEY gc_idx8(col_int_key,col_varchar_key,col_date_key,col_time_key,
  col_datetime_key)
);

INSERT INTO cc (
    col_int_nokey,
    col_date_nokey,
    col_time_nokey,
    col_datetime_nokey,
    col_varchar_nokey
) VALUES (1, '2009-12-01', '00:21:38.058143', '2007-05-28 00:00:00', 'c'),
(8, '2004-12-17', '04:08:02.046897', '2009-07-25 09:21:20.064099', 'm'),
(9, '2000-03-14', '16:25:11.040240', '2002-01-16 00:00:00', 'd'),
(24, '2000-10-08', '10:14:58.018534', '2006-10-12 04:32:53.031976', 'd'),
(6, '2006-05-25', '19:47:59.011283', '2001-02-15 03:08:38.035426', 'y'),
(1, '2008-01-23', '11:14:24.032949', '2004-10-02 20:31:15.022553', 't');
SET @save_old_sql_mode= @@sql_mode;
SET sql_mode="";

# Warnings arrive in unpredictable order and cannot be sorted
--disable_warnings
SELECT DISTINCT alias1.col_varchar_key AS field1
FROM ( cc AS alias1 STRAIGHT_JOIN
      (( cc AS alias2 STRAIGHT_JOIN cc AS alias3 ON
       (alias3.col_varchar_key > alias2.col_varchar_key ) ) ) ON
       (( alias3 .pk >= alias2.col_int_nokey ) AND
        (alias3 .pk >= alias2.col_int_nokey ) ))
WHERE alias1.col_varchar_key <= 'v'
GROUP BY field1 HAVING field1 = 91
ORDER BY field1, alias1.col_date_key, field1 ASC, field1 DESC,
        alias1.col_time_key ASC, field1;
DROP TABLE cc;
SET sql_mode=@save_old_sql_mode;
--enable_warnings

--echo #
--echo # Bug#20797941: WL8149:ASSERTION !TABLE ||
--echo #  (!TABLE->READ_SET || BITMAP_IS_SET(TABLE->READ_SET
--echo #
CREATE TABLE t(a int, b int as(a+1));
INSERT INTO t(a) values(1),(2);
SELECT * FROM t ORDER BY b;
DROP TABLE t;

if ($support_virtual_index)
{
--echo #
--echo # Testing a few index-based accesses on the virtual column
--echo #

CREATE TABLE t1 (
id int(11) NOT NULL,
b int(11) GENERATED ALWAYS AS (id+1) VIRTUAL NOT NULL,
UNIQUE KEY (b) );

--error ER_BAD_NULL_ERROR
INSERT INTO t1 (id) VALUES(NULL);
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t1 VALUES(2,3);

INSERT INTO t1 (id) VALUES(2),(3);
--disable_result_log
--disable_query_log
ANALYZE TABLE t1;
--enable_query_log
--enable_result_log

# constant table read with one index lookup
EXPLAIN SELECT * FROM t1 FORCE INDEX(b) WHERE b=3;

# eq_ref
EXPLAIN SELECT * FROM t1 AS t2 STRAIGHT_JOIN t1 FORCE INDEX(b) WHERE t1.b=t2.b;

# covering index scan
EXPLAIN SELECT b FROM t1 FORCE INDEX(b);

# range scan
INSERT INTO t1 (id) VALUES(4),(5),(6),(7),(8),(9),(10);
--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_query_log
--enable_result_log

EXPLAIN SELECT b FROM t1 FORCE INDEX(b) WHERE b BETWEEN 1 AND 5;

# index-subquery
EXPLAIN SELECT * FROM t2 AS t1 WHERE b NOT IN (SELECT b FROM t1 FORCE INDEX(b));

DROP TABLE t1;
}

DROP TABLE t2, t3;

--echo #
--echo # Bug#21317507:GC: STORED COLUMN REJECTED, BUT VIRTUAL IS ACCEPTED
--echo #
# TODO: Figure out appropriate NDB tests here
let $innodb_engine = `SELECT @@session.default_storage_engine='innodb'`;
let $myisam_engine = `SELECT @@session.default_storage_engine='MyISAM'`;
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(2147483647);
--error 1221
ALTER TABLE t1 ADD INDEX idx(a), WITH VALIDATION;
if ($myisam_engine)
{
  --echo Should fail and column shouldn't be added
  --error 1264
  ALTER TABLE t1 ADD COLUMN b SMALLINT AS (a) VIRTUAL;
  --error 1264
  ALTER TABLE t1 ADD COLUMN c SMALLINT AS (a) VIRTUAL, WITH VALIDATION;
  --error 1264
  ALTER TABLE t1 ADD COLUMN d SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION;
  ALTER TABLE t1 ADD COLUMN c INT AS(a) VIRTUAL;
  --error 1264
  ALTER TABLE t1 CHANGE c c SMALLINT AS(a) VIRTUAL, WITH VALIDATION;
  --error 1264
  ALTER TABLE t1 CHANGE c c SMALLINT AS(a) VIRTUAL, WITHOUT VALIDATION;
  --error 1264
  ALTER TABLE t1 MODIFY c TINYINT AS(a) VIRTUAL, WITH VALIDATION;
  --error 1264
  ALTER TABLE t1 MODIFY c TINYINT AS(a) VIRTUAL, WITHOUT VALIDATION;
  SELECT * FROM t1;
}
if ($innodb_engine)
{
  ALTER TABLE t1 ADD COLUMN b SMALLINT AS (a) VIRTUAL;
  ALTER TABLE t1 DROP COLUMN b;
  --error 1264
  ALTER TABLE t1 ADD COLUMN c SMALLINT AS (a) VIRTUAL, WITH VALIDATION;
  ALTER TABLE t1 ADD COLUMN d SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION;
  ALTER TABLE t1 DROP COLUMN d;
  ALTER TABLE t1 ADD COLUMN c INT AS(a) VIRTUAL;
# Change of storage type isn't in-place in InnoDB even for VGC
  --error 1264
  ALTER TABLE t1 CHANGE c c SMALLINT AS(a) VIRTUAL, WITH VALIDATION;
  --error 1264
  ALTER TABLE t1 CHANGE c c SMALLINT AS(a) VIRTUAL, WITHOUT VALIDATION;
  --error 1264
  ALTER TABLE t1 MODIFY c TINYINT AS(a) VIRTUAL, WITH VALIDATION;
  --error 1264
  ALTER TABLE t1 MODIFY c TINYINT AS(a) VIRTUAL, WITHOUT VALIDATION;
  SELECT * FROM t1;
}
DROP TABLE t1;
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(2147483647);
ALTER TABLE t1 ADD COLUMN h INT AS (a) VIRTUAL, WITHOUT VALIDATION;
--error 1845, 1846
ALTER TABLE t1 CHANGE h i INT AS (a) VIRTUAL, WITH VALIDATION,
  ALGORITHM=INPLACE;
if (!$testing_ndb)
{
ALTER TABLE t1 CHANGE h i INT AS (a) VIRTUAL, WITHOUT VALIDATION,
  ALGORITHM=INPLACE;
}
--error 1845, 1846
ALTER TABLE t1 ADD COLUMN b SMALLINT AS (a) VIRTUAL, WITH VALIDATION,
  ALGORITHM=INPLACE, LOCK=NONE;
--error 1845, 1846
ALTER TABLE t1 ADD COLUMN c SMALLINT AS (a) VIRTUAL, WITH VALIDATION,
  ALGORITHM=INPLACE, LOCK=SHARED;
--error 1845, 1846
ALTER TABLE t1 ADD COLUMN d SMALLINT AS (a) VIRTUAL, WITH VALIDATION,
  ALGORITHM=INPLACE, LOCK=EXCLUSIVE;
--echo Shouldn't fail on Innodb
if ($myisam_engine)
{
  --error 1845
  ALTER TABLE t1 ADD COLUMN e SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=NONE;
  --echo Shouldn't fail on Innodb
  --error 1845
  ALTER TABLE t1 ADD COLUMN f SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=SHARED;
  --echo Shouldn't fail on Innodb
  --error 1845
  ALTER TABLE t1 ADD COLUMN g SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=EXCLUSIVE;
}
if ($innodb_engine)
{
  ALTER TABLE t1 ADD COLUMN e SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=NONE;
  --echo Shouldn't fail on Innodb
  ALTER TABLE t1 ADD COLUMN f SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=SHARED;
  --echo Shouldn't fail on Innodb
  ALTER TABLE t1 ADD COLUMN g SMALLINT AS (a) VIRTUAL, WITHOUT VALIDATION,
    ALGORITHM=INPLACE, LOCK=EXCLUSIVE;
}
DROP TABLE t1;

--echo #
--echo # Bug#21980430 GCOLS: CRASHING
--echo #
CREATE TABLE t (
  a INT,
  b BLOB,
  c BLOB GENERATED ALWAYS AS (a+b) VIRTUAL,
  UNIQUE KEY i0008 (a)
);

INSERT INTO t(a,b) VALUES(1,'cccc');
--disable_query_log
--disable_result_log
ANALYZE TABLE t;
--enable_query_log
--enable_result_log
let $query=
SELECT /*+ bka() */ 1 AS c FROM t AS b RIGHT JOIN t AS c ON b.a > c.c
WHERE b.b>c.a;
eval EXPLAIN $query;
eval $query;
DROP TABLE t;

# Force DS-MRR to be used
set @optimizer_switch_save = @@optimizer_switch;
set optimizer_switch='mrr_cost_based=off';

# Reduce the size of the DS-MRR sort buffer to force multiple rounds
set @read_rnd_buffer_size_save= @@read_rnd_buffer_size;
set read_rnd_buffer_size=32;

CREATE TABLE t0 (
  i1 INTEGER NOT NULL
);

INSERT INTO t0 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  i1 INTEGER NOT NULL,
  i2 INTEGER NOT NULL,
  v1 INTEGER GENERATED ALWAYS AS (i2 + 1) VIRTUAL,
  v2 INTEGER GENERATED ALWAYS AS (i1 / (i1 - i2 + 57)) VIRTUAL,
  PRIMARY KEY (pk),
  INDEX idx(i1)
);

INSERT INTO t1 (pk, i1, i2)
SELECT a0.i1 + a1.i1*10 + a2.i1*100,
       a0.i1 + a1.i1*10,
       a0.i1 + a1.i1*10
FROM t0 AS a0, t0 AS a1, t0 AS a2;
--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_query_log
--enable_result_log

# Do a DS-MRR scan on an index on a non-generated column
# (this caused Division by 0 errors to be reported).
let query1=
SELECT * FROM t1
WHERE i1 > 41 AND i1 <= 43;

eval EXPLAIN $query1;
--sorted_result
eval $query1;

if ($support_virtual_index)
{
ALTER TABLE t1 ADD INDEX idx2(v1);
}

--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_query_log
--enable_result_log

# Do a DS-MRR scan on an index on a virtual column
# (this query returned too few records).
let query2=
SELECT * FROM t1
WHERE v1 > 41 AND v1 <= 43;

eval EXPLAIN $query2;
--sorted_result
eval $query2;

DROP TABLE t0, t1;

# Restore defaults
set optimizer_switch= @optimizer_switch_save;
set @@read_rnd_buffer_size= @read_rnd_buffer_size_save;

--echo #
--echo # Bug#21872184 CONDITIONAL JUMP AT JOIN_CACHE::WRITE_RECORD_DATA IN 
--echo #              SQL_JOIN_BUFFER.CC
--echo #

--echo #
--echo # Test 1: Dynamic range scan with one covering index
--echo #

# This is the original test case which produces the valgrind error when
# inserting data into the join buffer. The test failure only occurs with
# InnoDB since it is only InnoDB that currently supports indexes on
# virtual columns and is the only storage engine that includes the
# primary key in each secondary key. 

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t1
VALUES (10, 'c'), (10, 'i'), (2, 't'), (4, 'g');

CREATE TABLE t2 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t2
VALUES (2, 'k'), (9, 'k'), (7, 'o'), (5, 'n'), (7, 'e');

CREATE TABLE t3 (
  pk INTEGER NOT NULL,
  i1 INTEGER,
  i2_key INTEGER GENERATED ALWAYS AS (i1 + i1) VIRTUAL NOT NULL,
  PRIMARY KEY (pk)
);

if ($support_virtual_index)
{
--echo # Add a covering index. The reason for this index being covering is that
--echo # secondary indexes in InnoDB include the primary key.
ALTER TABLE t3 ADD INDEX v_idx (i2_key);
}

INSERT INTO t3 (pk, i1)
VALUES (1, 1), (2, 48), (3, 228), (4, 3), (5, 5),
       (6, 39), (7, 6), (8, 8), (9, 3);

CREATE TABLE t4 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t4 
VALUES (1, 'j'), (2, 'c'), (0, 'a');

ANALYZE TABLE t1, t2, t3, t4;

# Hint is added to avoid materialization of the subquery
let query=
SELECT /*+ NO_SEMIJOIN(@subq1) */ t1.c1, t2.i1
FROM t1 STRAIGHT_JOIN t3 STRAIGHT_JOIN t2 
WHERE ( t3.pk IN 
  (
    SELECT /*+ QB_NAME(subq1) */ t4.i1
    FROM t4
    WHERE t4.c1 < 'o'
  )
) 
AND t1.i1 <= t3.i2_key;

eval EXPLAIN $query;
--sorted_result
eval $query;

--echo #
--echo # Test 2: Two alternative covering indexes for the range scan
--echo #

# Adding second covering index
if ($support_virtual_index)
{
ALTER TABLE t3 ADD INDEX v_idx2 (i2_key, i1);
}

--disable_query_log
--disable_result_log
ANALYZE TABLE t3;
--enable_query_log
--enable_result_log
# Hint is added to avoid materialization of the subquery
let query=
SELECT /*+ NO_SEMIJOIN(@subq1) */ t1.c1, t2.i1
FROM t1 STRAIGHT_JOIN t3 STRAIGHT_JOIN t2 
WHERE ( t3.pk IN 
  (
    SELECT /*+ QB_NAME(subq1) */ t4.i1
    FROM t4
    WHERE t4.c1 < 'o'
  )
) 
AND t1.i1 <= t3.i2_key;

eval EXPLAIN $query;
--sorted_result
eval $query;

--echo #
--echo # Test 3: One covering index including the base column for the virtual
--echo #         column
--echo #

if ($support_virtual_index)
{
--echo # Drop the index with only the virtual column
ALTER TABLE t3 DROP INDEX v_idx;
}

--disable_query_log
--disable_result_log
ANALYZE TABLE t3;
--enable_query_log
--enable_result_log
# Hint is added to avoid materialization of the subquery
let query=
SELECT /*+ NO_SEMIJOIN(@subq1) */ t1.c1, t2.i1
FROM t1 STRAIGHT_JOIN t3 STRAIGHT_JOIN t2 
WHERE ( t3.pk IN 
  (
    SELECT /*+ QB_NAME(subq1) */ t4.i1
    FROM t4
    WHERE t4.c1 < 'o'
  )
) 
AND t1.i1 <= t3.i2_key;

eval EXPLAIN $query;
--sorted_result
eval $query;

--echo #
--echo # Test 4: One non-covering index
--echo #

if ($support_virtual_index)
{
--echo # Drop the index on two columns, add index on just one virtual column
ALTER TABLE t3 DROP INDEX v_idx2;
ALTER TABLE t3 ADD INDEX v_idx (i2_key);
}

--echo # Add more data to the table so that it will run the dynamic range scan
--echo # as both table scan and range scan (the purpose of this is to make the
--echo # table scan more expensive).
INSERT INTO t3 (pk, i1)
VALUES (10,1), (11,1), (12,1), (13,1), (14,1),(15,1), (16,1),(17,1), (18,1),
       (19,1), (20,1), (21,1), (22,1), (23,1), (24,1),(25,1),(26,1),(27,1), 
       (28,1), (29,1);

--disable_query_log
--disable_result_log
ANALYZE TABLE t3;
--enable_query_log
--enable_result_log
--echo # Change the query to read an extra column (t3.i1) making the index
--echo # non-covering.
# Hint is added to avoid materialization of the subquery
let query=
SELECT /*+ NO_SEMIJOIN(@subq1) */ t1.c1, t2.i1, t3.i1
FROM t1 STRAIGHT_JOIN t3 STRAIGHT_JOIN t2 
WHERE ( t3.pk IN 
  (
    SELECT /*+ QB_NAME(subq1) */ t4.i1
    FROM t4
    WHERE t4.c1 < 'o'
  )
) 
AND t1.i1 <= t3.i2_key;

eval EXPLAIN $query;
--sorted_result
eval $query;

--echo #
--echo # Test 5: Test where the added primary key to secondary indexes is
--echo #         used after it has been included in the join buffer
--echo #

# This test is only relevant for storage engines that add the primary key
# to all secondary keys (e.g. InnoDB). For these engines, the fields in the
# primary key might be included when deciding that a secondary index is
# covering for the query. This is the case for most of the secondary indexes
# on t3 in this test. But in the above queries, the subquery is non-dependent
# and the "t3.pk IN .." will be evaluated after rows for t3 are read. At this
# time t3.pk is in the record buffer. t3.pk is not used after it has been
# inserted into the join buffer. To test that t3.pk is actually correctly
# included in the join buffer we change the subquery to be dependent and
# only evaluated after the join has been done.
# The purpose of this test is to ensure that we correctly handle and
# include primary key fields that are added to a covering secondary index.

# The difference between this query and the query in test 1 is that
# an extra query condition is added to the subquery.
# Hint is added to avoid materialization of the subquery
let query=
SELECT /*+ NO_SEMIJOIN(@subq1) */ t1.c1, t2.i1
FROM t1 STRAIGHT_JOIN t3 STRAIGHT_JOIN t2 
WHERE ( t3.pk IN 
  (
    SELECT /*+ QB_NAME(subq1) */ t4.i1
    FROM t4
    WHERE t4.c1 < 'o' and t4.i1 < (t2.i1 + 1)
  )
) 
AND t1.i1 <= t3.i2_key;

eval EXPLAIN $query;
--sorted_result
eval $query;

DROP TABLE t1, t2, t3, t4;
