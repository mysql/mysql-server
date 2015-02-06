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
eval $s;
eval explain $s;

--echo # select_type=PRIMARY, type=range,ref
let $s = select * from t1 where c in (select c from t3 where c between -2 and -1);
eval $s;
eval explain $s;

--echo # select_type=UNION, type=system
--echo # select_type=UNION RESULT, type=<union1,2>
let $s = select * from t1 union select * from t2;
eval $s;
eval explain $s;

--echo # select_type=DERIVED, type=system
let $s = select * from (select a,b,c from t1) as t11;
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
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-gcol expr>
let $s = select * from t3 where a between 1 and 2;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed gcol expr>
let $s = select * from t3 where b between -2 and -1;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed gcol expr>
let $s = select * from t3 where c between -2 and -1;
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
let $s = select sum(c) from t1 group by b;
eval $s;
eval explain $s;

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
