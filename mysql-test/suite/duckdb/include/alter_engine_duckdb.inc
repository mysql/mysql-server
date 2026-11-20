--echo #
--echo # 1. Test Alter Table Engine = duckdb for all data types
--echo #


CREATE TABLE t1 (
  id int PRIMARY KEY,
  col1 int,
  col2 bigint,
  col3 float,
  col4 double,
  col5 decimal(10,4),
  col6 tinyint,
  col7 smallint,
  col8 mediumint
) ENGINE = innodb;

insert into t1 values (1, 1, -1, 1.01, -1.001, 1.0001, 1, 1, 1), 
  (2, -2, 2, -2.02, 2.002, -2.0002, 2, 2, 2), 
  (-3, 3, 3, 3.03, 3.003, 3.0003, 3, 3, 3),
  (-2147483648, -2147483648, -9223372036854775808, -3.402823466E+38, -1.7976931348623157E+308, -999999.9999, -128, -32768, -8388608),
  (2147483647, 2147483647, 9223372036854775807, 3.402823466E+38, 1.7976931348623157E+308, 999999.9999, 127, 32767, 8388607);

--sorted_result
SELECT * FROM t1;

ALTER TABLE t1 ENGINE = duckdb;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (
  id int PRIMARY KEY,
  col1 int unsigned,
  col2 bigint unsigned,
  col3 float unsigned,
  col4 double unsigned,
  col5 decimal(10,4) unsigned,
  col6 bit(10),
  col7 tinyint unsigned,
  col8 smallint unsigned,
  col9 mediumint unsigned
) ENGINE = innodb;

insert into t1 values (1, 1, 1, 1.01, 1.001, 1.0001, B'111111111', 1, 1, 1),
  (2, 2, 2, 2.02, 2.002, 2.0002, B'0000000000', 1, 1, 1),
  (3, 3, 3, 3.03, 3.003, 3.0003, B'0001100110', 1, 1, 1),
  (-2147483648, 0, 0, 0, 0, 0, B'00000000000', 0, 0, 0),
  (2147483647, 255, 18446744073709551615, 3.402823466E+38, 1.7976931348623157E+308, 999999.9999, B'1111111111', 255, 65535, 8388607);

--sorted_result
SELECT id,col1,col2,col3,col4,col5,hex(col6),hex(col7),hex(col8),hex(col9) FROM t1;

ALTER TABLE t1 ENGINE = duckdb;
--sorted_result
SELECT id,col1,col2,col3,col4,col5,hex(col6),hex(col7),hex(col8),hex(col9) FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (
  id int PRIMARY KEY,
  col1 decimal(65, 30),
  col2 decimal(65, 15),
  col3 decimal(65, 0),
  col4 decimal(38,30),
  col5 decimal(38,18),
  col6 decimal(38,0),
  col7 decimal(9,9),
  col8 decimal(9,4),
  col9 decimal(9,0)
) ENGINE = innodb;

insert into t1 values (1,
                       99999999999999999999999999999999999.999999999999999999999999999999,
                       99999999999999999999999999999999999999999999999999.999999999999999,
                       99999999999999999999999999999999999999999999999999999999999999999,
                       99999999.999999999999999999999999999999,
                       99999999999999999999.999999999999999999,
                       99999999999999999999999999999999999999,
                       0.99999999,
                       99999.9999,
                       999999999);

--sorted_result
SELECT * FROM t1;

ALTER TABLE t1 ENGINE = duckdb;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (
  id int PRIMARY KEY,
  col1 date,
  col2 datetime,
  col3 timestamp,
  col4 datetime(6),
  col5 time,
  col6 time(6),
  col7 year
) ENGINE = innodb;

insert into t1 values (1, '2020-01-01', '2020-01-01 12:00:00', '2020-01-01 12:00:00', '2020-01-01 12:00:00.1', '12:00:00', '12:00:00.1', 2020),
  (2, '2020-12-31', '2020-12-31 00:00:00', '2020-12-31 00:00:00', '2020-12-31 00:00:00.123456789', '00:00:00', '00:00:00.123456789', 1970),
  (3, '1970-01-01', '1970-01-01 23:59:59', '1970-01-01 23:59:59', '2020-12-31 23:59:59.123456', '23:59:59', '23:59:59.123456', 2050),
  (4, '2000-12-31 23:59:59.123456', '2000-12-31 23:59:59.123456', '2000-12-31 23:59:59.123456', '2000-12-31 23:59:59.123456', '23:59:59.123456', '23:59:59.123456', 2100);

--sorted_result
SELECT * FROM t1;

ALTER TABLE t1 ENGINE = duckdb;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
