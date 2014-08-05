# Regressiontest for statements that failed with optimizer tracing enabled.

--source include/have_optimizer_trace.inc
# InnoDB page size influences cost numbers
--source include/have_innodb_16k.inc

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
) ENGINE=InnoDB;
  
CREATE TABLE t2 (  
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  col_varchar_nokey VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

CREATE TABLE t3 (  
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  col_varchar_nokey VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

CREATE TABLE t4 (
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

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
--disable_ps_protocol # because of BUG#12595688
eval SELECT * FROM where_subselect_19379 WHERE (field1, field2) NOT IN
($query); 
--enable_ps_protocol

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

eval CREATE TABLE where_updatedelete_20769 $query;

eval UPDATE where_updatedelete_20769 SET field1 = ( $query );

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

DROP TABLE t1,t2,t3,t4;
