 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

# check 0x00 padding
create table t1 (s1 binary(3));
insert into t1 values (0x61), (0x6120), (0x612020);
select hex(s1) from t1;
drop table t1;

# check that 0x00 is not stripped in val_str
create table t1 (s1 binary(2), s2 varbinary(2));
insert into t1 values (0x4100,0x4100);
select length(concat('*',s1,'*',s2,'*')) from t1;
delete from t1;
insert into t1 values (0x4120,0x4120);
select length(concat('*',s1,'*',s2,'*')) from t1;
drop table t1;

# check that trailing 0x00 and 0x20 do matter on comparison
create table t1 (s1 varbinary(20), s2 varbinary(20));

#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table t1;
insert into t1 values (0x41,0x4100),(0x41,0x4120),(0x4100,0x4120);
select hex(s1), hex(s2) from t1;
select count(*) from t1 where s1 < s2;
drop table t1;

# check that trailing 0x00 do matter on filesort
create table t1 (s1 varbinary(2), s2 varchar(1));
insert into t1 values (0x41,'a'), (0x4100,'b'), (0x41,'c'), (0x4100,'d');
select hex(s1),s2 from t1 order by s1,s2;
drop table t1;

# check that 0x01 is padded to 0x0100 and thus we get a duplicate value
create table t1 (s1 binary(2) primary key);
insert into t1 values (0x01);
insert into t1 values (0x0120);
--error ER_DUP_ENTRY
insert into t1 values (0x0100);
select hex(s1) from t1 order by s1;
# check index search
select hex(s1) from t1 where s1=0x01;
select hex(s1) from t1 where s1=0x0120;
select hex(s1) from t1 where s1=0x0100;
select count(distinct s1) from t1;
alter table t1 drop primary key;
# check non-indexed search
select hex(s1) from t1 where s1=0x01;
select hex(s1) from t1 where s1=0x0120;
select hex(s1) from t1 where s1=0x0100;
select count(distinct s1) from t1;
drop table t1;

# check that 0x01 is not padded, and all three values are unique
create table t1 (s1 varbinary(2) primary key);
insert into t1 values (0x01);
insert into t1 values (0x0120);
insert into t1 values (0x0100);
select hex(s1) from t1 order by s1;
# check index search
select hex(s1) from t1 where s1=0x01;
select hex(s1) from t1 where s1=0x0120;
select hex(s1) from t1 where s1=0x0100;
select count(distinct s1) from t1;
alter table t1 drop primary key;
# check non-indexed search
select hex(s1) from t1 where s1=0x01;
select hex(s1) from t1 where s1=0x0120;
select hex(s1) from t1 where s1=0x0100;
select count(distinct s1) from t1;
drop table t1;

# check that cast appends trailing zeros
select hex(cast(0x10 as binary(2)));

#
# Bug #14299: BINARY space truncation should cause warning or error
# 
create table t1 (b binary(2), vb varbinary(2));
insert into t1 values(0x4120, 0x4120);
insert ignore into t1 values(0x412020, 0x412020);
drop table t1;
create table t1 (c char(2), vc varchar(2));
insert into t1 values(0x4120, 0x4120);
insert into t1 values(0x412020, 0x412020);
drop table t1;

set @old_sql_mode= @@sql_mode, sql_mode= 'traditional';
create table t1 (b binary(2), vb varbinary(2));
insert into t1 values(0x4120, 0x4120);
--error ER_DATA_TOO_LONG
insert into t1 values(0x412020, NULL);
--error ER_DATA_TOO_LONG
insert into t1 values(NULL, 0x412020);
drop table t1;
set @@sql_mode= @old_sql_mode;

#
# Bug#14171: Wrong default value for a BINARY field
#
create table t1(f1 int, f2 binary(2) not null, f3 char(2) not null);
insert ignore into t1 set f1=1;
select hex(f2), hex(f3) from t1;
drop table t1;

--echo End of 5.0 tests

--echo #
--echo # Bug#21922414 CAST OF TOO BIG HEX LITERAL TO BIGINT UNSIGNED: BAD RESULT AND NO WARNING
--echo #

--echo # Those casts overflow and must send a warning
select convert(9999999999999999999999999999999999999999999,unsigned);
select convert('9999999999999999999999999999999999999999999',unsigned);
select convert(0x9999999999999999999999999999999999999999999,unsigned);

--echo # and an error in strict mode
--error ER_TRUNCATED_WRONG_VALUE
create table t1
select convert(0x9999999999999999999999999999999999999999999,unsigned);

--echo # Same here
select 9999999999999999999999999999999999999999999 | 0;
select '9999999999999999999999999999999999999999999' | 0;
select 0x9999999999999999999999999999999999999999999 | 0;

--echo # DECIMAL addition, no warning
select 9999999999999999999999999999999999999999999 + 0;
--echo # DOUBLE addition, no warning
select '9999999999999999999999999999999999999999999' + 0;
--echo # BIGINT UNSIGNED addition, warning
select 0x9999999999999999999999999999999999999999999 + 0;

--echo # Test truncation inside error message
select 0x9999999999999999999999999999999999999999999888888888888888888888888888888888888888888888888888888888888888888888888877777777777777777777777777777777777777777777777777777777777776666666666666666666666666666666666666666 + 0;

# Hex literal is like BIGINT UNSIGNED in additions:
create table t1 select 0x9999 + 0;
desc t1;
select * from t1;
drop table t1;

--echo #
--echo # Bug#11757477 ARITHMETIC ERROR WHEN DEALING WITH BIGINT
--echo #

SELECT HEX(0xfffffffffffff+1);
SELECT HEX(0xfffffffffffff+2);
SELECT 0x20000000000000+0;
SELECT 0x20000000000000+1;
SELECT 0x20000000000000+2;
SELECT 0x20000000000000+3;
SELECT 0xfffffffffffff+2;
SELECT 0xfffffffffffff+1;

--echo #
--echo # Bug#22268110 VIEW USING HEXADECIMAL OR BIT LITERAL GIVES WRONG RESULTS
--echo #

--echo # Literals with more than 8 bytes.

let $query=
SELECT
x'7f9d04ae61b34468ac798ffcc984ab68'=x'7f9d04ae61b34468ac798ffcc984ab68'
AS a,
x'7f9d04ae61b34468ac798ffcc984ab68'=x'7f9d04ae61b34468ac798ffcc984ab60'
AS b,
x'7f9d04ae61b34468ac798ffcc984ab68'=x'0f9d04ae61b34468ac798ffcc984ab68'
AS c,
b'111111111111111111111111111111111111111111111111111111111111111111'=
b'111111111111111111111111111111111111111111111111111111111111111111'
AS d,
b'111111111111111111111111111111111111111111111111111111111111111111'=
b'111111111111111111111111111111111111111111111111111111111111111110'
AS e,
b'111111111111111111111111111111111111111111111111111111111111111111'=
b'011111111111111111111111111111111111111111111111111111111111111111'
AS f;

eval CREATE VIEW v1 AS $query;

--echo # Both should give same result:
eval $query;
SELECT * FROM v1;
--echo # Literals should be entirely printed:
SHOW CREATE VIEW v1;
DROP VIEW v1;

--echo #
--echo # Bug#35467555 No matching rows when joining multiple tables
--echo #              on binary/varbinary key
--echo #

CREATE TABLE parents (
  id binary(16) NOT NULL,
  PRIMARY KEY (id)
);

CREATE TABLE foos (
  id bigint NOT NULL AUTO_INCREMENT,
  parent_id varbinary(16) NOT NULL,
  text varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  PRIMARY KEY (id),
  KEY index_foos_on_parent_id (parent_id)
);

CREATE TABLE bars (
  id binary(16) NOT NULL,
  parent_id varbinary(16) NOT NULL,
  PRIMARY KEY (id),
  KEY index_bars_on_parent_id (parent_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

INSERT INTO parents VALUES (X'93222796caba43ca979f5c96eb6898b7');
INSERT INTO foos (parent_id, text) SELECT parents.id, 'correct output' FROM
parents;
INSERT INTO bars (id, parent_id) SELECT X'79cea9ab6fe14a8ebdfed711a7727763',
parents.id FROM parents;

SELECT foos.text
FROM foos
JOIN parents ON parents.id = foos.parent_id
JOIN bars ON bars.parent_id = parents.id
WHERE bars.parent_id = X'93222796caba43ca979f5c96eb6898b7';

DROP TABLE parents, foos, bars;

--echo #
--echo # Bug#35507763: Assertion `to->field_ptr() != from->field_ptr()' failed.
--echo #

CREATE TABLE t (x BINARY(0), y BINARY(0));
INSERT INTO t VALUES ('', x);
SELECT * FROM t;
DROP TABLE t;
