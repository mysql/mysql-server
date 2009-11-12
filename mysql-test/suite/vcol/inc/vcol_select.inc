################################################################################
# inc/vcol_select.inc                                                          #
#                                                                              #
# Purpose:                                                                     #
#  Testing different SELECTs.                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-18                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

# Table t1 is used below to test:
#  - Join type of ALL (sequential scan of the entire table)
#  - Join type of Index
#  - Join type of Range
#  - Join type of Ref_or_null
create table t1 (a int,
                 b int as (-a),
                 c int as (-a) persistent,
                 index (c));
insert into t1 (a) values (2), (1), (1), (3), (NULL);

# Table t2 is used below to test:
#  - Join type of system and const
create table t2 like t1;
insert into t2 (a) values (1);

# Table t3 is used below to test
#  - Join type of Eq_ref with a unique virtual column
#  - Join type of Const
create table t3 (a int primary key, 
                 b int as (-a),
		 c int as (-a) persistent unique);
insert into t3 (a) values (2),(1),(3);


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

--echo # SELECT COUNT(DISTINCT <non-vcol>) FROM tbl_name
let $s = select count(distinct a) from t1;
eval $s;
eval explain $s;

--echo # SELECT COUNT(DISTINCT <non-stored vcol>) FROM tbl_name
let $s = select count(distinct b) from t1;
eval $s;
eval explain $s;

--echo # SELECT COUNT(DISTINCT <stored vcol>) FROM tbl_name
let $s = select count(distinct c) from t1;
eval $s;
eval explain $s;

--echo ###
--echo ### filesort & range-based utils
--echo ###
--echo # SELECT * FROM tbl_name WHERE <vcol expr>
let $s = select * from t3 where c >= -2;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-vcol expr>
let $s = select * from t3 where a between 1 and 2;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed vcol expr>
let $s = select * from t3 where b between -2 and -1;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed vcol expr>
let $s = select * from t3 where c between -2 and -1;
eval $s;
eval explain $s;

#### Remove for MyISAM due to a bug
#### when all the three records are returned (a=1,2,3) 
#### instead of just two (a=1,2).
#### This bug is presumably in base SQL routines as the same happens
#### with this table:
####   create table t4 (a int primary key, b int, c int unique);
let $myisam_engine = `SELECT @@session.storage_engine='myisam'`;
if (!$myisam_engine)
{
  --echo # SELECT * FROM tbl_name WHERE <non-vcol expr> ORDER BY <non-indexed vcol>
  let $s = select * from t3 where a between 1 and 2 order by b;
  eval $s;
  eval explain $s;
}

--echo # SELECT * FROM tbl_name WHERE <non-vcol expr> ORDER BY <indexed vcol>
let $s = select * from t3 where a between 1 and 2 order by c;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed vcol expr> ORDER BY <non-vcol>
let $s = select * from t3 where b between -2 and -1 order by a;
eval $s;
eval explain $s;

#### Remove for MyISAM due to a bug
#### when all the three records are returned (a=1,2,3) 
#### instead of just two (a=1,2).
#### This bug is presumably in base SQL routines as the same happens
#### with this table:
####   create table t4 (a int primary key, b int, c int unique);
let $innodb_engine = `SELECT @@session.storage_engine='innodb'`;
if (!$innodb_engine)
{
  --echo # SELECT * FROM tbl_name WHERE <indexed vcol expr> ORDER BY <non-vcol>
  let $s = select * from t3 where c between -2 and -1 order by a;
  eval $s;
  eval explain $s;
}

--echo # SELECT * FROM tbl_name WHERE <non-indexed vcol expr> ORDER BY <non-indexed vcol>
let $s = select * from t3 where b between -2 and -1 order by b;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed vcol expr> ORDER BY <non-indexed vcol>
let $s = select * from t3 where c between -2 and -1 order by b;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <non-indexed vcol expr> ORDER BY <indexed vcol>
let $s = select * from t3 where b between -2 and -1 order by c;
eval $s;
eval explain $s;

--echo # SELECT * FROM tbl_name WHERE <indexed vcol expr> ORDER BY <indexed vcol>
let $s = select * from t3 where c between -2 and -1 order by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<non-indexed vcol>) FROM tbl_name GROUP BY <non-indexed vcol>
let $s = select sum(b) from t1 group by b;
eval $s;
eval explain $s;

--echo # SELECT sum(<indexed vcol>) FROM tbl_name GROUP BY <indexed vcol>
let $s = select sum(c) from t1 group by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<non-indexed vcol>) FROM tbl_name GROUP BY <indexed vcol>
let $s = select sum(b) from t1 group by c;
eval $s;
eval explain $s;

--echo # SELECT sum(<indexed vcol>) FROM tbl_name GROUP BY <non-indexed vcol>
let $s = select sum(c) from t1 group by b;
eval $s;
eval explain $s;

