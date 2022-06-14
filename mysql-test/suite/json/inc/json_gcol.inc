# Order on stored and virtual generated columns with type JSON.
CREATE TABLE t(
  j JSON,
  stored_gc JSON GENERATED ALWAYS AS (JSON_EXTRACT(j, '$[0]')) STORED,
  virtual_gc JSON GENERATED ALWAYS AS (JSON_EXTRACT(j, '$[1]')) VIRTUAL);
INSERT INTO t(j) VALUES
(JSON_ARRAY(1, 2)), (JSON_ARRAY(2, 1)), (JSON_ARRAY(10, 10)), (JSON_ARRAY(5));
SELECT * FROM t ORDER BY stored_gc;
SELECT * FROM t ORDER BY virtual_gc;
DROP TABLE t;

--echo # ----------------------------------------------------------------------
--echo # Test of generated columns that call JSON functions
--echo # ----------------------------------------------------------------------
CREATE TABLE t(id INT, j JSON,
               gc INT GENERATED ALWAYS AS (JSON_EXTRACT(j, '$[0]')));
INSERT INTO t(id, j) VALUES (0, '"5"'), (1, '[]'), (2, '[1,2]'), (3, '5');
--error ER_INVALID_JSON_VALUE_FOR_CAST
INSERT INTO t(j) VALUES ('{}');
--error ER_INVALID_JSON_VALUE_FOR_CAST
INSERT INTO t(j) VALUES ('{"a":1}');
--error ER_INVALID_JSON_VALUE_FOR_CAST
INSERT INTO t(j) VALUES ('"abc"');
--error ER_INVALID_JSON_TEXT
INSERT INTO t(j) VALUES ('');
--error ER_INVALID_JSON_TEXT
INSERT INTO t(j) VALUES ('[');
SELECT * FROM t ORDER BY id;
UPDATE t SET j = '[123]';
SELECT * FROM t ORDER BY id;
--error ER_INVALID_JSON_TEXT
UPDATE t SET j = '[';
DROP TABLE t;

CREATE TABLE t(id INT, j JSON,
               gc JSON GENERATED ALWAYS AS (JSON_ARRAY(j)));
INSERT INTO t(id, j)
  VALUES (1, '1'), (2, '[true, false]'), (3, '{"a":1,"b":2}');
--error ER_INVALID_JSON_TEXT
INSERT INTO t(j) VALUES ('');
--error ER_INVALID_JSON_TEXT
INSERT INTO t(j) VALUES ('[');
SELECT * FROM t ORDER BY id;
UPDATE t SET j = '"abc"';
SELECT * FROM t ORDER BY id;
--error ER_INVALID_JSON_TEXT
UPDATE t SET j = '[';
DROP TABLE t;

# Timestamp values sometimes got printed as base64 strings.
CREATE TABLE t(ts TIMESTAMP, j JSON AS (CAST(ts AS JSON)));
INSERT INTO t(ts) VALUES ('2000-01-01 00:00:00');
SELECT CAST(JSON_ARRAY(ts, j) AS CHAR) FROM t;
DROP TABLE t;

--echo #
--echo # Bug#21491442 VARIANT::FORCED_RETURN() [WITH T = JSON_SCALAR*]:
--echo #              ASSERTION `FALSE' FAILED.
--echo #
create table t (a json, b blob,
c int generated always as (1!=a) virtual not null) engine=innodb;
insert into t(a) values('[1]');
insert into t(a) values('[1]');
select a,c from t;
prepare ps1 from 'insert into t(a) values(?)';
set @a='[1]';
execute ps1 using @a;
execute ps1 using @a;
select a,c from t;
drop table t;

create temporary table t (a json, b blob,
c int generated always as (1!=a) virtual not null) engine=innodb;
insert into t(a) values('[1]');
insert into t(a) values('[1]');
select a,c from t;
prepare ps1 from 'insert into t(a) values(?)';
set @a='[1]';
execute ps1 using @a;
execute ps1 using @a;
select a,c from t;

drop table t;

--echo #
--echo # WL#8170: Expression analyzer for GC.
--echo # Queries with GC and JSON_EXTRACT  compared to strings should use index
--echo #
create table t1(
  f1 json,
  gc varchar(20) character set utf8mb4 as
    (json_unquote(json_extract(f1,"$"))) stored,
  key gc_idx(gc));
insert into t1(f1) values ('"qwe"'),('"rty"'),('"uiop"');
insert into t1(f1) values ('"zxc"'),('"vbn"'),('"mnb"');
insert into t1(f1) select f1 from t1;
insert into t1(f1) select f1 from t1;
insert into t1(f1) values ('"asd"'),('"asdf"'),('"asasas"');

set @save_opt_sw= @@optimizer_switch;
# Innodb has some issues with ICP & GC stuff
set @@optimizer_switch="index_condition_pushdown=off";

set @string_literal='asd';
--sorted_result
select f1 from t1 where gc = "asd";
explain select f1 from t1 where gc = "asd";
--sorted_result
select f1 from t1 where gc = @string_literal;
explain select f1 from t1 where gc = @string_literal;
--sorted_result
select f1 from t1 where json_extract(f1,"$") = "asd";
explain select f1 from t1 where json_extract(f1,"$") = "asd";
--sorted_result
select f1 from t1 where json_extract(f1,"$") = @string_literal;
explain select f1 from t1 where json_extract(f1,"$") = @string_literal;

--sorted_result
select f1 from t1 where "asd" = json_extract(f1,"$");
explain select f1 from t1 where "asd" = json_extract(f1,"$");
--sorted_result
select f1 from t1 where @string_literal = json_extract(f1,"$");
explain select f1 from t1 where @string_literal = json_extract(f1,"$");

set @string_literal = 'z';

--sorted_result
select f1 from t1 where gc > "z";
explain select f1 from t1 where gc > "z";
--sorted_result
select f1 from t1 where gc > @string_literal;
explain select f1 from t1 where gc > @string_literal;
--sorted_result
select f1 from t1 where json_extract(f1,"$") > "z";
explain select f1 from t1 where json_extract(f1,"$") > "z";
--sorted_result
select f1 from t1 where json_extract(f1,"$") > @string_literal;
explain select f1 from t1 where json_extract(f1,"$") > @string_literal;

set @string_lo = 'v';
set @string_hi = 'z';

--sorted_result
select f1 from t1 where gc > "v" and gc < "z";
explain select f1 from t1 where gc > "v" and gc < "z";
--sorted_result
select f1 from t1 where gc > @string_lo and gc < @string_hi;
explain select f1 from t1 where gc > @string_lo and gc < @string_hi;
--sorted_result
select f1 from t1
where json_extract(f1,"$") > "v" and json_extract(f1,"$") < "z";
--sorted_result
select f1 from t1
where json_extract(f1,"$") > @string_lo and json_extract(f1,"$") < @string_hi;
explain
select f1 from t1
where json_extract(f1,"$") > @string_lo and json_extract(f1,"$") < @string_hi;

--sorted_result
select f1 from t1 where gc between "v" and "z";
explain select f1 from t1 where gc between "v" and "z";
--sorted_result
select f1 from t1 where gc between @string_lo and @string_hi;
explain select f1 from t1 where gc between @string_lo and @string_hi;
--sorted_result
select f1 from t1 where json_extract(f1,"$") between "v" and "z";
explain select f1 from t1 where json_extract(f1,"$") between "v" and "z";
--sorted_result
select f1 from t1 where json_extract(f1,"$") between @string_lo and @string_hi;
explain
select f1 from t1 where json_extract(f1,"$") between @string_lo and @string_hi;

set @string_1 = 'asd';
set @string_2 = 'asasas';
set @string_3 = 'asdf';

--sorted_result
select f1 from t1 where gc in ("asd","asasas","asdf");
explain select f1 from t1 where gc in ("asd","asasas","asdf");
--sorted_result
select f1 from t1 where gc in (@string_1, @string_2, @string_3);
explain select f1 from t1 where gc in (@string_1, @string_2, @string_3);
--sorted_result
select f1 from t1 where json_extract(f1,"$") in ("asd","asasas","asdf");
explain select f1 from t1 where json_extract(f1,"$") in ("asd","asasas","asdf");

--sorted_result
select f1 from t1
where json_extract(f1,"$") in (@string_1, @string_2, @string_3);
explain
select f1 from t1
where json_extract(f1,"$") in (@string_1, @string_2, @string_3);

select f1 from t1 where json_unquote(json_extract(f1,"$")) = "asd";
explain select f1 from t1 where json_unquote(json_extract(f1,"$")) = "asd";
select f1 from t1 where json_unquote(json_extract(f1,"$")) = @string_literal;
explain
select f1 from t1 where json_unquote(json_extract(f1,"$")) = @string_literal;

set @@optimizer_switch= @save_opt_sw;
drop table t1;

create table t1(f1 varchar(10), gc varchar(10) as (json_unquote(f1)) stored,
  key gc_idx(gc));
insert into t1(f1) values ('"qwe"'),('"rty"'),('"uiop"');
select f1 from t1 where lower(f1)="qwe";
drop table t1;


--echo #
--echo #
--echo # Bug#21054516:QUERY HAVING SQL_BIG_RESULT ON JSON DATA GIVES EXTRA
--echo #              ROWS IN OUTPUT
--echo #
CREATE TABLE t1 (
pk integer auto_increment key,
col_varchar_255_utf8_key varchar(255)  CHARACTER SET utf8
);

INSERT INTO t1 VALUES (NULL, 'q') , (NULL, 'tgzvsj') ,
(NULL, 'b') , (NULL, 'q') , (NULL, 'up') , (NULL, 'up') ;

ALTER TABLE t1 ADD COLUMN json_varchar255_utf8_key json;

UPDATE t1 SET json_varchar255_utf8_key =
  JSON_OBJECT('col_varchar_255_utf8_key', col_varchar_255_utf8_key);

ALTER TABLE t1 MODIFY col_varchar_255_utf8_key VARCHAR(255)
  GENERATED ALWAYS AS
    (JSON_EXTRACT(json_varchar255_utf8_key,'$.col_varchar_255_utf8_key[0]'))
  STORED;

SELECT SQL_BIG_RESULT table1.json_varchar255_utf8_key AS field1, count(*)
  FROM t1 AS table1 LEFT JOIN t1 AS table2 ON table1.pk <= table2.pk
  GROUP  BY field1;
DROP TABLE t1;

--echo #
--echo # Bug#26352119: WHERE JSON_EXTRACT() ON GEN KEY
--echo #

# The index should be used even if the collation is not the default
# collation of the character set.
SET NAMES utf8mb4 COLLATE utf8mb4_ja_0900_as_cs;
CREATE TABLE t1 (j JSON, pk VARCHAR(10) AS (j->>'$.id') STORED PRIMARY KEY);
INSERT INTO t1 (j) VALUES ('{"id":"a"}'), ('{"id":"b"}'), ('{"id":"c"}');

# Analyze to ensure table statistics are up to date before EXPLAIN.
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE j->>'$.id'='b';
SELECT * FROM t1 WHERE j->>'$.id'='b';

DROP TABLE t1;
SET NAMES DEFAULT;
