# Regressiontest for statements that failed with optimizer tracing enabled.

--source include/have_optimizer_trace.inc
# InnoDB page size influences cost numbers

let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

SET end_markers_in_json=on;
SET optimizer_trace="enabled=on,one_line=off";

--echo #
--echo # BUG#12430646 - SEL_ARG::LEFT AND RIGHT POINTERS INCORRECTLY 
--echo #                USED. CRASHES OPTIMIZER TRACING
--echo #

CREATE TABLE t1 (
  a INT,
  b CHAR(2),
  c INT,
  d INT,
  KEY (c),
  KEY (d,a,b(2)),
  KEY (b(1))
);

INSERT INTO t1 VALUES (NULL, 'a', 1, 2), (NULL, 'a', 1, 2),
                      (1,    'a', 1, 2), (1,    'a', 1, 2);

CREATE TABLE t2 (
  a INT,
  c INT,
  e INT,
  KEY (e)
);

INSERT INTO t2 VALUES (1, 1, NULL), (1, 1, NULL);

SELECT 1
FROM t1, t2
WHERE t1.d <> '1' AND t1.b > '1'
AND t1.a = t2.a AND t1.c = t2.c;

DROP TABLE t1, t2;

--echo #
--echo # BUG#12595210 - JSON SYNTAX ERROR ASSERT ON WHERE FIELD NOT IN SUBQUERY
--echo #

# This trace exhibits a non-empty
# "evaluating_constant_where_conditions" object which is rare.

CREATE TABLE t1 (  
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB, CHARSET utf8mb4;
  
CREATE TABLE t2 (  
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  col_varchar_nokey VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB, CHARSET utf8mb4;

CREATE TABLE t3 (  
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  col_varchar_nokey VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB, CHARSET utf8mb4;

CREATE TABLE t4 (
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB, CHARSET utf8mb4;

CREATE VIEW view_t4 AS SELECT * FROM t4;

let $query=
SELECT
  (
  SELECT SUM(sq1_alias1.pk) AS sq1_field1
  FROM view_t4 AS sq1_alias1
    INNER JOIN t1 AS sq1_alias2 ON  (sq1_alias2.col_varchar_key =
sq1_alias1.col_varchar_key )
  ) AS field1,
  alias1.col_varchar_nokey AS field2
FROM (t2 AS alias1
  LEFT JOIN ( t2 AS alias2
    LEFT OUTER JOIN t2 AS alias3 ON  (alias3.col_varchar_nokey =
alias2.col_varchar_key )
  ) ON (alias3.col_varchar_key = alias2.col_varchar_key)
)
WHERE  ( alias2.col_varchar_key IN (
  SELECT sq2_alias1.col_varchar_nokey AS sq2_field1
  FROM t3 AS sq2_alias1
  WHERE sq2_alias1.col_varchar_nokey <= alias1.col_varchar_key
  )
);

eval CREATE TABLE where_subselect_19379 $query;
eval SELECT * FROM where_subselect_19379 WHERE (field1, field2) NOT IN
($query); 

select * from information_schema.OPTIMIZER_TRACE;

drop table t1,t2,t3,t4,where_subselect_19379;
drop view view_t4;

--echo #
--echo # BUG#12607524 JSON PARSE ERROR ON SELECT ... FROM ... WHERE .. IN (SUBQUERY)
--echo #

CREATE TABLE t1 (  
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (8,'g');

CREATE TABLE t2 (
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;
INSERT INTO t2 VALUES (7,'x');

CREATE TABLE where_subselect_19033  
  SELECT
    ( SELECT col_int_key FROM t2 ) as field1
  FROM t1
;

SELECT * FROM where_subselect_19033;

SELECT field1
FROM where_subselect_19033
WHERE field1 IN
 ( SELECT
   ( SELECT col_int_key FROM t2 )
   FROM t1
 )
;

SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE where_subselect_19033,t1,t2;

--echo
--echo # BUG#12612201 - SEGFAULT IN
--echo # SUBSELECT_UNIQUESUBQUERY_ENGINE::PRINT WITH OPTIMIZER TRACE 
--echo

CREATE TABLE t1 (
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL
);

CREATE TABLE t2 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (1,4,'v','v'),(20,5,'r','r');

CREATE TABLE t3 (
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL
);

INSERT INTO t3 VALUES (NULL,'j','j'),(8,'c','c');

let $query=
  select  count(  alias2 . col_varchar_key ) as field1
  from (
    (select sq1_alias1 . *
     from ( t3 as sq1_alias1
       straight_join t1 as sq1_alias2
       on (sq1_alias2 . col_varchar_key = sq1_alias1 . col_varchar_key)
     )
     where  sq1_alias1 . col_int_key  in (
       select   c_sq1_alias1 . pk as c_sq1_field1
       from t2 as c_sq1_alias1
     )
    ) as alias1
      left outer join t1 as alias2
      on (alias2 . col_varchar_key = alias1 . col_varchar_key  )
  )
  where (  alias2 . col_varchar_key  in (
    select   sq2_alias1 . col_varchar_nokey as sq2_field1
    from t2 as sq2_alias1
    where  sq2_alias1 . col_int_key  in (
      select distinct  c_sq2_alias1 . col_int_key as c_sq2_field1
      from t3 as c_sq2_alias1
    )
  ) )
    or alias1 . col_int_key = 2
      and alias2 . col_varchar_nokey <= alias1 . col_varchar_nokey  
  order by alias1 . col_varchar_key  , field1
;

--source include/turn_off_only_full_group_by.inc

eval CREATE TABLE where_updatedelete_20769 $query;
eval UPDATE where_updatedelete_20769 SET field1 = ( $query );

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

DROP TABLE where_updatedelete_20769;
DROP TABLE t1,t2,t3;

--echo
--echo # BUG#12710761 - INVALID JSON TRACE ON SUBQUERY IN IN-CLAUSE
--echo

CREATE TABLE t1 (col_int_key int, KEY col_int_key (col_int_key));
INSERT INTO t1 VALUES (0),(8),(1),(8);

CREATE TABLE where_subselect_20070
  SELECT table2 .col_int_key AS field1,
        ( SELECT COUNT( col_int_key )
          FROM t1
        )
  FROM t1 AS table1
    JOIN t1 AS table2
    ON table2 .col_int_key = table1 .col_int_key;

SELECT *
FROM where_subselect_20070
WHERE (field1, ( SELECT COUNT( col_int_key ) FROM t1 )) IN (
  SELECT table2 .col_int_key AS field1,
        ( SELECT COUNT( col_int_key )
          FROM t1
        )
  FROM t1 AS table1
    JOIN t1 AS table2
    ON table2 .col_int_key = table1 .col_int_key
);

select * from information_schema.optimizer_trace;

DROP TABLE where_subselect_20070,t1;

--echo #
--echo # Bug#13430443 - ASSERTION `NEW_TYPE[0] != 'U'' FAILED. WHEN
--echo # OPTIMIZER_TRACE IS ENABLED
--echo #
CREATE TABLE t1
 (a INT,b INT,c INT, KEY(a),KEY (a,c)) ENGINE=INNODB;
SELECT 1 FROM t1 WHERE 1 LIKE
 (SELECT a FROM t1 WHERE a = 1 ORDER BY c);
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
DROP TABLE t1;

--echo #
--echo # Bug #18346750 OPTIMIZER_TRACE & DBUG_PRINT CRASH IN ST_SELECT_LEX::PRINT
--echo #
if ($support_native_partitioning)
{
  CREATE TABLE t1 (a INT, PRIMARY KEY (a))
   PARTITION BY KEY (a) PARTITIONS 2;
  INSERT INTO t1 VALUES (1),(2);
  --ERROR ER_SUBQUERY_NO_1_ROW
  SELECT  1 FROM t1 ,t1 w
   WHERE t1.a <=> (SELECT 22 FROM t1 GROUP BY (SELECT 1 FROM t1));
  SELECT TRACE LIKE "%select had some error%" FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
  DROP TABLE t1;
}
if (!$support_native_partitioning)
{
  --echo "Skipped since this engine does not support native partitioning.
}

--echo #
--echo # Bug#18791851 CRASH IN ST_SELECT_LEX::PRINT WITH OPTIMIZER_TRACE ON SUBQUERY
--echo #

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_time_key time,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

CREATE TABLE t3 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

CREATE TABLE t4 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_time_key time,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

--source include/turn_off_only_full_group_by.inc

SELECT alias1.col_int_nokey AS field1,
       alias2.col_varchar_key AS field2,
       alias1.col_time_key AS field3,
       MIN( alias1.col_int_nokey ) AS field4
FROM (
       (
         SELECT SQ1_alias1.*
         FROM ( t1 AS SQ1_alias1, t2 AS SQ1_alias2 )
       ) AS alias1,
       t4 AS alias2
     )
WHERE
  EXISTS (
         SELECT DISTINCT  SQ2_alias2.col_varchar_nokey AS SQ2_field1
         FROM t2 AS SQ2_alias1
              INNER JOIN (t4 AS SQ2_alias2
              INNER JOIN t3 AS SQ2_alias3
              ON SQ2_alias3.pk = SQ2_alias2.pk)
              ON SQ2_alias3.col_varchar_key = SQ2_alias2.col_varchar_nokey
         )
  AND alias1.col_int_key = alias2.pk
HAVING  alias1.col_int_nokey  IN ( SELECT 2 FROM DUAL ) ;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

DROP TABLE t1,t2,t3,t4;

# Inspired by testcase of
# Bug #18345786	CRASH AROUND ST_JOIN_TABLE::AND_WITH_CONDITION
CREATE TABLE t1(a INT);
SET @a:=(SELECT ROW(1, 2)=
ROW((SELECT 1 FROM t1 LEFT JOIN t1 t2 ON 1
               HAVING 3 IN (SELECT 2 FROM DUAL)),
    1));
DROP TABLE t1;

--echo #
--echo # Bug #18945693 CRASH IN PRINT_TABLE_ARRAY AT SQL/SQL_LEX.CC ON 2ND EXEC OF PREPARED STATEMENT
--echo #

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t1 VALUES (13,7,3,'y','y'),(14,0,4,'c','c');

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t2 VALUES (20,9,8,'e','e');

CREATE TABLE t3 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t3 VALUES (1,1,7,'k','k');

PREPARE prep_stmt FROM "
SELECT
  alias1.col_varchar_nokey AS field1,
  MAX(alias1.pk) AS field2
FROM t3 AS alias1
GROUP BY field1
HAVING
  1 > (
       SELECT MAX(SQ4_alias1.col_varchar_nokey)
       FROM t1 AS SQ4_alias1
            RIGHT OUTER JOIN
            (
              t2 AS SQ4_alias2
              JOIN t1 AS SQ4_alias3
              ON SQ4_alias3.col_int_key = SQ4_alias2.col_int_nokey
            )
            ON SQ4_alias3.pk = SQ4_alias2.col_int_nokey
      )
";

EXECUTE prep_stmt;
EXECUTE prep_stmt;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#19063289 CRASH IN PRINT_TABLE_ARRAY ON 2ND EXECUTION OF PS WITH SUBQUERY AND VIEW
--echo #

CREATE TABLE B (
pk INTEGER,
col_int_key INTEGER,
col_varchar_nokey VARCHAR(1)
);

CREATE TABLE C (
col_int_nokey INTEGER,
col_int_key INTEGER,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1)
) ;

CREATE TABLE CC (
pk INTEGER,
col_int_key INTEGER
) ENGINE=MYISAM;

INSERT INTO CC VALUES (1, 0),(2, 7);

PREPARE stmt FROM "
SELECT
alias1.col_int_key AS field1 ,
COUNT(alias1.pk) AS field2
FROM
CC AS alias1
GROUP BY field1
HAVING
alias1.col_int_key >
  (
    SELECT
    MAX(  SQ2_alias2.col_int_key ) AS SQ2_field1
    FROM
    C AS SQ2_alias1 LEFT  JOIN
       ( B AS SQ2_alias2 INNER JOIN C AS SQ2_alias3
         ON 1)
       ON 1
  )
OR field2 < 2
";
EXECUTE stmt;
EXECUTE stmt;

DROP TABLE B,C,CC;

--echo #
--echo # Bug #23259872: OPTIMIZER CHOOSES TO USE NON PRIMARY
--echo #                INDEX, EVEN THOUGH COST IS HIGHER
--echo #

CREATE TABLE t1 (
  a TINYTEXT NOT NULL,
  b TINYINT(3) UNSIGNED NOT NULL,
  PRIMARY KEY (a(32),b)
) ENGINE=MyISAM, CHARSET utf8mb4;
CREATE TABLE t2 (
  a TINYTEXT NOT NULL,
  b TINYINT(3) UNSIGNED NOT NULL,
  PRIMARY KEY (a(32),b)
) ENGINE=INNODB, CHARSET utf8mb4;

INSERT INTO t1 VALUES ('a',1),('a',2);
INSERT INTO t2 VALUES ('a',1),('a',2),('a',3),('b',1),('c',1),('c',4),('e',2);
ANALYZE TABLE t1;
ANALYZE TABLE t2;

# range uses more keyparts than ref, so range is chosen.
EXPLAIN SELECT * FROM t1 WHERE a='a' AND b in (1,2);
SELECT TRACE into @trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT @trace RLIKE "range_uses_more_keyparts";

EXPLAIN SELECT * FROM t2 WHERE a='a' AND b in (1,2);
SELECT TRACE into @trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT @trace RLIKE "range_uses_more_keyparts";

DROP TABLE t1, t2;

--echo #
--echo # Bug #23227428: SQL PLAN IS NOT ACCORDING WITH OPTIMIZER_TRACE
--echo #

CREATE TABLE t1(c1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                c2 CHAR(12)) ENGINE=INNODB;
disable_query_log;
let $1 = 1000;
while ($1)
{
 eval INSERT INTO t1(c2) VALUES ('$1');
 dec $1;
}
enable_query_log;
ANALYZE TABLE t1;

EXPLAIN SELECT c1, c2 FROM t1 ORDER BY c1 DESC LIMIT 1;
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
DROP TABLE t1;
