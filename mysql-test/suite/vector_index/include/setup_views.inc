# Copyright 2026 Google LLC

use test;

delimiter |;
create procedure fill_vector_table(tname varchar(40), n_rows int)
begin
  declare i int default 1;
  declare d1 float default 1.0;
  declare d2 float default 2.0;
  declare d3 float default 3.0;
  while i <= n_rows do
    set @s = concat("insert into ", tname, " (id, emb, name, next_id) values (", i , ", string_to_vector('[", d1, ",", d2, ",", d3, "]'), name, ", i+1, ")");
    prepare stmt from @s;
    execute stmt;
    set i = i + 1;
    set d1 = d1 + 1;
    set d2 = d2 + 1;
    set d3 = d3 + 1;
  end while;
end |
delimiter ;|

--sleep 2

CREATE TABLE t1(
  id int, emb vector(3), name VARCHAR(20), next_id int, PRIMARY KEY(id));

CREATE INDEX i1 on t1(name);
CREATE INDEX i2 on t1(next_id);
CREATE INDEX i3 on t1(name, next_id);
CREATE INDEX i4 on t1(next_id, name);

CREATE TABLE t2(
  id int, emb vector(3), name VARCHAR(20), next_id int, PRIMARY KEY(id, name));

CREATE INDEX i5 on t2(name);
CREATE UNIQUE INDEX i6 on t2(next_id);
CREATE INDEX i7 on t2(name, next_id);
CREATE UNIQUE INDEX i8 on t2(next_id, name);

--disable_query_log
CALL fill_vector_table('t1', 1000);
CALL fill_vector_table('t2', 1000);
--enable_query_log

--echo # create vector index
create vector index vecIndex1 on t1(emb)distance_measure=l2_squared;
create vector index vecIndex2 on t2(emb)distance_measure=cosine;

ANALYZE TABLE t1;
ANALYZE TABLE t2;

CREATE VIEW v1 AS SELECT * from t1;
CREATE VIEW v3 AS SELECT * from t1 WHERE id > 10;

CREATE VIEW v2 AS SELECT * from t2;
CREATE VIEW v4 AS SELECT * from t2 WHERE id < 1000;
