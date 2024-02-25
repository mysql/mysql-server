--echo #
--echo # Integer column, equi-height histogram
--echo #
CREATE TABLE tbl_int (col1 INT);
INSERT INTO tbl_int VALUES (1), (2), (3), (4), (5), (6), (7), (8), (NULL), (NULL);
ANALYZE TABLE tbl_int;
ANALYZE TABLE tbl_int UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 > 0;
EXPLAIN SELECT * FROM tbl_int WHERE 0 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_int WHERE col1 > 8;
EXPLAIN SELECT * FROM tbl_int WHERE 8 < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 < 10;
EXPLAIN SELECT * FROM tbl_int WHERE 10 > col1;

--echo # Expect "36.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 >= 6;
EXPLAIN SELECT * FROM tbl_int WHERE 6 <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 >= -100;
EXPLAIN SELECT * FROM tbl_int WHERE -100 <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 != 8;
EXPLAIN SELECT * FROM tbl_int WHERE 8 != col1;
EXPLAIN SELECT * FROM tbl_int WHERE col1 <> 8;
EXPLAIN SELECT * FROM tbl_int WHERE 8 <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_int WHERE col1 = 100;
EXPLAIN SELECT * FROM tbl_int WHERE 100 = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IS NOT NULL;

--echo # Expect "36.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 BETWEEN 1 AND 3;

--echo # Expect "43.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 NOT BETWEEN 1 AND 3;

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IN (1, 3, 4, 5, 6, 7);

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 NOT IN (1, 3, 4, 5, 6, 7);

DROP TABLE tbl_int;

--echo #
--echo # String column, equi-height histogram
--echo #
CREATE TABLE tbl_varchar (col1 VARCHAR(255));
INSERT INTO tbl_varchar VALUES
  ("abcd"), ("🍣"), ("🍺"), ("eeeeeeeeee"), ("ef"), ("AG"),
  ("a very long string that is longer than 42 characters"),
  ("lorem ipsum"), (NULL), (NULL);
ANALYZE TABLE tbl_varchar UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_varchar;

--echo # Expect "36.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 > "b";
EXPLAIN SELECT * FROM tbl_varchar WHERE "b" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 > "lp";
EXPLAIN SELECT * FROM tbl_varchar WHERE "lp" < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 < "sierra";
EXPLAIN SELECT * FROM tbl_varchar WHERE "sierra" > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 >= "abcd";
EXPLAIN SELECT * FROM tbl_varchar WHERE "abcd" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 >= "";
EXPLAIN SELECT * FROM tbl_varchar WHERE "" <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 != "lorem ipsum";
EXPLAIN SELECT * FROM tbl_varchar WHERE "lorem ipsum" != col1;
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 <> "lorem ipsum";
EXPLAIN SELECT * FROM tbl_varchar WHERE "lorem ipsum" <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 = "sierra";
EXPLAIN SELECT * FROM tbl_varchar WHERE "sierra" = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 IS NOT NULL;

--echo # Expect "10.00" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 BETWEEN "a" AND "b";

--echo # Expect "76.72" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 NOT BETWEEN "a" AND "b";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 IN ("ag", "ef", "🍣");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 NOT IN ("ag", "ef", "🍣");

DROP TABLE tbl_varchar;

CREATE TABLE tbl_varchar (col1 VARCHAR(255));
INSERT INTO tbl_varchar VALUES
#   |------------ 42 characters -------------|
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnoq"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnor"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnos"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopp"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopq"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnoss"),
  ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnost");
ANALYZE TABLE tbl_varchar UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_varchar;

--echo # Expect "100.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 < "abcdefghijklmnopqrstuvwxyzabcdefghijklmnos";

--echo # Expect "12.50" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 < "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopr";

DROP TABLE tbl_varchar;

--echo #
--echo # Double column, equi-height histogram
--echo #
CREATE TABLE tbl_double (col1 DOUBLE);
INSERT INTO tbl_double VALUES (-1.1), (0.0), (1.1), (2.2), (3.3), (4.4), (5.5), (6.6), (NULL), (NULL);
ANALYZE TABLE tbl_double UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_double;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 > 0.0e0;
EXPLAIN SELECT * FROM tbl_double WHERE 0.0e0 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_double WHERE col1 > 6.6e0;
EXPLAIN SELECT * FROM tbl_double WHERE 6.6e0 < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 < 100.0;
EXPLAIN SELECT * FROM tbl_double WHERE 100.0 > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 >= 3.3e0;
EXPLAIN SELECT * FROM tbl_double WHERE 3.3e0 <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 >= -2.0e0;
EXPLAIN SELECT * FROM tbl_double WHERE -2.0e0 <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 != 0.0e0;
EXPLAIN SELECT * FROM tbl_double WHERE 0.0e0 != col1;
EXPLAIN SELECT * FROM tbl_double WHERE col1 <> 0.0e0;
EXPLAIN SELECT * FROM tbl_double WHERE 0.0e0 <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_double WHERE col1 = 100.0;
EXPLAIN SELECT * FROM tbl_double WHERE 100.0 = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 IS NOT NULL;

--echo # Expect "13.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 BETWEEN 1.1e0 AND 3.3e0;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 NOT BETWEEN 1.1e0 AND 3.3e0;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 IN (-1.1e0, 0.0e0, 1.1e0, 2.2e0);

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 NOT IN (-1.1e0, 0.0e0, 1.1e0, 2.2e0);

DROP TABLE tbl_double;


--echo #
--echo # Time column, equi-height histogram
--echo #
CREATE TABLE tbl_time (col1 TIME);
INSERT INTO tbl_time VALUES
  ("-01:00:00"), ("00:00:00"), ("00:00:01"), ("00:01:00"), ("01:00:00"),
  ("01:01:00"), ("02:00:00"), ("03:00:00"), (NULL), (NULL);
ANALYZE TABLE tbl_time UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_time;

--echo # Expect "40.66" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 > "00:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "00:00:00" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_time WHERE col1 > "03:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "03:00:00" < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 < "10:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "10:00:00" > col1;

--echo # Expect "40.64" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 >= "00:00:01";
EXPLAIN SELECT * FROM tbl_time WHERE "00:00:01" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 >= "-01:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "-01:00:00" <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 != "01:01:00";
EXPLAIN SELECT * FROM tbl_time WHERE "01:01:00" != col1;
EXPLAIN SELECT * FROM tbl_time WHERE col1 <> "01:01:00";
EXPLAIN SELECT * FROM tbl_time WHERE "01:01:00" <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_time WHERE col1 = "10:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "10:00:00" = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 IS NOT NULL;

--echo # Expect "20.64" in column "filtered"
--echo # This might seem low, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_time WHERE col1 BETWEEN "00:00:01" AND "02:00:00";

--echo # Expect "59.36" in column "filtered"
--echo # This might seem high, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_time WHERE col1 NOT BETWEEN "00:00:01" AND "02:00:00";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 IN ("-01:00:00", "00:00:00", "03:00:00");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 NOT IN ("-01:00:00", "00:00:00", "03:00:00");

DROP TABLE tbl_time;

CREATE TABLE tbl_time (col1 TIME(6));
INSERT INTO tbl_time VALUES
  ("00:00:00.000000"), ("00:00:00.000001"), ("00:00:00.000002"),
  ("00:00:00.000003"), ("00:00:00.000004"), ("00:00:00.000005");
ANALYZE TABLE tbl_time UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_time;

EXPLAIN SELECT * FROM tbl_time WHERE col1 < "00:00:00.000004";

DROP TABLE tbl_time;

--echo #
--echo # Date column, equi-height histogram
--echo #
CREATE TABLE tbl_date (col1 DATE);
INSERT INTO tbl_date VALUES
  ("1000-01-01"), ("9999-12-30"), ("2017-01-01"), ("2017-01-02"), ("2017-02-01"),
  ("2018-01-01"), ("2019-01-01"), ("3019-01-01"), (NULL), (NULL);
ANALYZE TABLE tbl_date UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_date;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 > "2017-01-02";
EXPLAIN SELECT * FROM tbl_date WHERE "2017-01-02" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_date WHERE col1 > "9999-12-31";
EXPLAIN SELECT * FROM tbl_date WHERE "9999-12-31" < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 < "9999-12-31";
EXPLAIN SELECT * FROM tbl_date WHERE "9999-12-31" > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 >= "2018-01-01";
EXPLAIN SELECT * FROM tbl_date WHERE "2018-01-01" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 >= "1000-01-01";
EXPLAIN SELECT * FROM tbl_date WHERE "1000-01-01" <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 != "2017-01-02";
EXPLAIN SELECT * FROM tbl_date WHERE "2017-01-02" != col1;
EXPLAIN SELECT * FROM tbl_date WHERE col1 <> "2017-01-02";
EXPLAIN SELECT * FROM tbl_date WHERE "2017-01-02" <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_date WHERE col1 = "9999-12-31";
EXPLAIN SELECT * FROM tbl_date WHERE "9999-12-31" = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 IS NOT NULL;

--echo # Expect "10.00" in column "filtered"
--echo # This might seem low, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_date WHERE col1 BETWEEN "2017-01-01" AND "3019-01-01";

--echo # Expect "74.98" in column "filtered"
--echo # This might seem high, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_date WHERE col1 NOT BETWEEN "2017-01-01" AND "3019-01-01";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 IN ("1000-01-01", "2017-01-02", "2018-01-01");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 NOT IN ("1000-01-01", "2017-01-02", "2018-01-01");

DROP TABLE tbl_date;


--echo #
--echo # Datetime column, equi-height histogram
--echo #
CREATE TABLE tbl_datetime (col1 DATETIME(6));
INSERT INTO tbl_datetime VALUES
  ("1000-01-01 00:00:00"), ("9999-12-31 23:59:59.999998"),
  ("2017-01-01 00:00:00"), ("2017-01-01 00:00:00.000001"),
  ("2017-02-01 00:00:00"), ("2018-01-01 00:00:00.999999"),
  ("2018-01-01 00:00:01"), ("3019-01-01 10:10:10.101010"), (NULL), (NULL);
ANALYZE TABLE tbl_datetime UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_datetime;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 > "2018-01-01 00:00:00";
EXPLAIN SELECT * FROM tbl_datetime WHERE "2018-01-01 00:00:00" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 > "9999-12-31 23:59:59.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "9999-12-31 23:59:59.999999" < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 < "9999-12-31 23:59:59.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "9999-12-31 23:59:59.999999" > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 >= "2018-01-01 00:00:00.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "2018-01-01 00:00:00.999999" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 >= "1000-01-01 00:00:00.000000";
EXPLAIN SELECT * FROM tbl_datetime WHERE "1000-01-01 00:00:00.000000" <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 != "3019-01-01 10:10:10.101010";
EXPLAIN SELECT * FROM tbl_datetime WHERE "3019-01-01 10:10:10.101010" != col1;
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 <> "3019-01-01 10:10:10.101010";
EXPLAIN SELECT * FROM tbl_datetime WHERE "3019-01-01 10:10:10.101010" <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 = "9999-12-31 23:59:59.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "9999-12-31 23:59:59.999999" = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 IS NOT NULL;

--echo # Expect "10.00" in column "filtered"
--echo # This might seem low, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 BETWEEN "2017-01-01 00:00:00.000001" AND "3019-01-01 10:10:10.101010";

--echo # Expect "74.98" in column "filtered"
--echo # This might seem high, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 NOT BETWEEN "2017-01-01 00:00:00.000001" AND "3019-01-01 10:10:10.101010";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 IN ("1000-01-01 00:00:00.000000", "2018-01-01 00:00:00.999999", "9999-12-31 23:59:59.999998");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 NOT IN ("1000-01-01 00:00:00.000000", "2018-01-01 00:00:00.999999", "9999-12-31 23:59:59.999998");

DROP TABLE tbl_datetime;


--echo #
--echo # Decimal column, equi-height histogram
--echo #
CREATE TABLE tbl_decimal (col1 DECIMAL(65, 30));
INSERT INTO tbl_decimal VALUES
  (00000000000000000000000000000000000.000000000000000000000000000000),
  (99999999999999999999999999999999999.999999999999999999999999999998),
  (-99999999999999999999999999999999999.999999999999999999999999999999),
  (1), (2), (3), (4), (-1), (NULL), (NULL);
ANALYZE TABLE tbl_decimal UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_decimal;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 > 1;
EXPLAIN SELECT * FROM tbl_decimal WHERE 1 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 > 100000000000000000000000000000000000;
EXPLAIN SELECT * FROM tbl_decimal WHERE 100000000000000000000000000000000000 < col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 < 99999999999999999999999999999999999.999999999999999999999999999999;
EXPLAIN SELECT * FROM tbl_decimal WHERE 99999999999999999999999999999999999.999999999999999999999999999999 > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 >= 4;
EXPLAIN SELECT * FROM tbl_decimal WHERE 4 <= col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 >= -1;
EXPLAIN SELECT * FROM tbl_decimal WHERE -1 <= col1;

--echo # Expect "70.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 != 2.0;
EXPLAIN SELECT * FROM tbl_decimal WHERE 2.0 != col1;
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 <> 2.0;
EXPLAIN SELECT * FROM tbl_decimal WHERE 2.0 <> col1;

--echo # Expect "10.0" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 = "99999999999999999999999999999999999.999999999999999999999999999999";
EXPLAIN SELECT * FROM tbl_decimal WHERE "99999999999999999999999999999999999.999999999999999999999999999999" = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 IS NOT NULL;

--echo # This might seem low, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
--echo # Expect "10.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 BETWEEN -1.0 AND 4.0;

--echo # This might seem high, but given the uniform distribution assuption in
--echo # each bucket this is actually correct.
--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 NOT BETWEEN -1.0 AND 4.0;

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 IN
  (-99999999999999999999999999999999999.999999999999999999999999999999, 1.0, 2.0);

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 NOT IN
  (-99999999999999999999999999999999999.999999999999999999999999999999, 1.0, 2.0);

DROP TABLE tbl_decimal;

--echo #
--echo # ENUM column, equi-height histogram
--echo # Note that we only support equality/inequality operators for ENUM
--echo # columns.
--echo #
CREATE TABLE tbl_enum (col1 ENUM('red', 'black', 'blue', 'green'));
INSERT INTO tbl_enum VALUES ('red'), ('red'), ('black'), ('blue'), ('green'),
                            ('green'), (NULL), (NULL), (NULL);
ANALYZE TABLE tbl_enum UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_enum;

--echo # Expect "16.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 'red';
EXPLAIN SELECT * FROM tbl_enum WHERE 'red' = col1;

--echo # Expect "50.00" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 'black';
EXPLAIN SELECT * FROM tbl_enum WHERE 'black' != col1;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 'black';
EXPLAIN SELECT * FROM tbl_enum WHERE 'black' <> col1;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != '';
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 0;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> '';
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 0;

--echo # Expect "50.00" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IN ('black', 'blue', 'green');

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 NOT IN ('green', 'blue');

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IS NULL;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IS NOT NULL;


--echo # Test that the numerical representation of enum values also gives the
--echo # correct result.

--echo # Expect "16.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 1;
EXPLAIN SELECT * FROM tbl_enum WHERE 1 = col1;

--echo # Expect "50.00" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 2;
EXPLAIN SELECT * FROM tbl_enum WHERE 2 != col1;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 2;
EXPLAIN SELECT * FROM tbl_enum WHERE 2 <> col1;

--echo # Expect "11.11" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 100;
EXPLAIN SELECT * FROM tbl_enum WHERE 100 = col1;

--echo # Expect "50.00" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IN (2, 3, 4);

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 NOT IN (4, 3);

DROP TABLE tbl_enum;


--echo #
--echo # SET column, equi-height histogram
--echo # Note that we only support equality/inequality operators for SET
--echo # columns.
--echo #
CREATE TABLE tbl_set (col1 SET('red', 'black', 'blue', 'green'));
INSERT INTO tbl_set VALUES ('red'), ('red,black'), ('black,green,blue'),
                           ('black,green,blue'), ('black,green,blue'),
                           ('green'), ('green,red'), ('red,green'), (NULL),
                           (NULL), (NULL);
ANALYZE TABLE tbl_set UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE tbl_set;

--echo # Expect "11.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 = 'red,green';
EXPLAIN SELECT * FROM tbl_set WHERE 'red,green' = col1;

--echo # Expect "61.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 'red';
EXPLAIN SELECT * FROM tbl_set WHERE 'red' != col1;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 'red';
EXPLAIN SELECT * FROM tbl_set WHERE 'red' <> col1;

--echo # Expect "72.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 0;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 0;

--echo # Expect "38.64" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IN ('green', 'black,blue,green');

--echo # Expect "34.09" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 NOT IN ('green', 'black,blue,green');

--echo # Expect "27.27" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IS NULL;

--echo # Expect "72.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IS NOT NULL;


--echo # Test that the numerical representation of set values also gives the
--echo # correct result.

--echo # Expect "11.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 = 9;
EXPLAIN SELECT * FROM tbl_set WHERE 9 = col1;

--echo # Expect "61.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 1;
EXPLAIN SELECT * FROM tbl_set WHERE 1 != col1;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 1;
EXPLAIN SELECT * FROM tbl_set WHERE 1 <> col1;

--echo # Expect "72.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 0;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 0;

--echo # Expect "9.09" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_set WHERE col1 = 100;
EXPLAIN SELECT * FROM tbl_set WHERE 100 = col1;

--echo # Expect "38.64" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IN (8, 14);

--echo # Expect "34.09" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 NOT IN (8, 14);

DROP TABLE tbl_set;

--echo #
--echo # Tests for covering various corner cases that hasn't already been
--echo # covered.
--echo #
CREATE TABLE t1 (col1 VARCHAR(255));
INSERT INTO t1 VALUES ("a"), ("a"), ("a"), ("a"), ("a"), ("a"), ("a"), ("b"),
                      ("c"), ("d");
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE col1 < "a";
DROP TABLE t1;

CREATE TABLE t1 (col1 DECIMAL);
INSERT INTO t1 VALUES (1.0), (1.0), (1.0), (1.0), (1.0), (1.0), (1.0), (2.0),
                      (3.0), (4.0);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE col1 < 0.0;
EXPLAIN SELECT * FROM t1 WHERE col1 < 1.0;
DROP TABLE t1;

CREATE TABLE t1 (col1 BIGINT UNSIGNED);
INSERT INTO t1 VALUES (100), (100), (100), (100), (100), (100), (100), (200),
                      (300), (400);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE col1 <= 100;
EXPLAIN SELECT * FROM t1 WHERE col1 <= 150;
EXPLAIN SELECT * FROM t1 WHERE 150 >= col1;
EXPLAIN SELECT * FROM t1 WHERE col1 BETWEEN 0 AND RAND();
EXPLAIN SELECT * FROM t1 WHERE col1 BETWEEN 1 AND NULL;
EXPLAIN SELECT * FROM t1 WHERE col1 IN (1, RAND());
EXPLAIN SELECT * FROM t1 WHERE col1 IN (1, NULL);
EXPLAIN SELECT * FROM t1 WHERE col1 IN (100, 100, 100, 100, 100, 100);
EXPLAIN SELECT * FROM t1 WHERE col1 NOT IN (1, NULL);
EXPLAIN SELECT * FROM t1 WHERE col1 NOT IN (100, 100, 100, 100, 100, 100);
EXPLAIN SELECT * FROM t1 WHERE col1 <= NULL;
EXPLAIN SELECT * FROM t1 WHERE col1 >= NULL;
EXPLAIN SELECT * FROM t1 WHERE col1 != NULL;
DROP TABLE t1;


CREATE TABLE t1 (col1 TIME);
INSERT INTO t1 VALUES ("00:00:00"), ("00:00:00"), ("00:00:00"), ("00:00:00"),
                      ("00:00:00"), ("00:00:00"), ("00:00:00"), ("00:01:00"),
                      ("00:02:00"), ("00:03:00");
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE col1 < "00:00:00";
EXPLAIN SELECT * FROM t1 WHERE col1 NOT BETWEEN "00:00:00" AND "";
DROP TABLE t1;

--echo #
--echo # Bug# 33302021 - Equi-height histogram is not equi-height
--echo #

# Histogram construction bug example:
#
# A data distribution with a few highly frequent largest values can cause
# two distinct values that together account for almost the entire distribution
# to end up in the same bucket, causing large potential errors in selectivity
# estimates.

# Explanation of the bug:
#
# If during histogram construction, we end up in a state with at least as many
# buckets remaining as there are there are distinct values remaining, we begin
# adding buckets indiscriminately. If many "light" buckets are added this way,
# the target threshold is moved far ahead of the actual cumulative frequency of
# the buckets. It might then happen that placing two "heavy" values in the same
# bucket will bring us closer to the target than just adding one.

delimiter //;

CREATE PROCEDURE insert_repeat(IN value INT, IN num_insertions INT)
BEGIN
  DECLARE i INT DEFAULT 0;
  SET i = 1;
  WHILE i <= num_insertions DO
    INSERT INTO t1 VALUES (value);
    SET i = i + 1;
  END WHILE;
END//

CREATE PROCEDURE insert_range(IN first INT, IN last INT)
BEGIN
  DECLARE i INT DEFAULT 0;
  SET i = first;
  WHILE i <= last DO
    INSERT INTO t1 VALUES (i);
    SET i = i + 1;
  END WHILE;
END//

delimiter ;//

CREATE TABLE t1(x INT);
CALL insert_range(1, 10);
CALL insert_repeat(11, 10);
CALL insert_repeat(12, 20);

# With the bug the value 11 (25% of values) and 12 (50% of values) end up in the
# same bucket. They should be placed in singleton buckets.
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x WITH 10 BUCKETS;

# It seems that we have to call ANALYZE TABLE for the histogram to be used.
# TODO(christiani): Investigate if this is a bug.
ANALYZE TABLE t1;

# Test that we get selectivity estimates that correspond to singleton buckets.
EXPLAIN SELECT * FROM t1 WHERE x = 11;
EXPLAIN SELECT * FROM t1 WHERE x = 12;

TRUNCATE TABLE t1;

# Create histograms for a couple of different data sets and verify manually that
# the output looks reasonable.

# Test case: Uniform data set.
# Buckets should be evenly distributed and contain close to 10 values each.
CALL insert_range(1, 100);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x WITH 10 BUCKETS;
SELECT JSON_PRETTY(JSON_REMOVE(histogram, '$."last-updated"'))
FROM INFORMATION_SCHEMA.column_statistics
WHERE table_name = 't1' AND column_name = 'x';
TRUNCATE TABLE t1;

# Test case: A few heavy hitters at the end.
# The heavy values should be placed in singleton buckets and the remaining buckets
# should be used efficiently, i.e, they should contain roughly 100/7 = 14.3
# values each.
CALL insert_range(1, 100);
CALL insert_repeat(101, 100);
CALL insert_repeat(102, 100);
CALL insert_repeat(103, 100);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x WITH 10 BUCKETS;
SELECT JSON_PRETTY(JSON_REMOVE(histogram, '$."last-updated"'))
FROM INFORMATION_SCHEMA.column_statistics
WHERE table_name = 't1' AND column_name = 'x';
TRUNCATE TABLE t1;

# Test case: Heavy hitters with a few values in between.
# The heavy values should be placed in singleton buckets. This will force the
# values between them, e.g. 1, 3-4, and 6-9, to be placed in their own buckets.
# The remaining 4 buckets should contain approximately 23 values each.
CALL insert_range(1, 100);
CALL insert_repeat(2, 100);
CALL insert_repeat(5, 100);
CALL insert_repeat(10, 100);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x WITH 10 BUCKETS;
SELECT JSON_PRETTY(JSON_REMOVE(histogram, '$."last-updated"'))
FROM INFORMATION_SCHEMA.column_statistics
WHERE table_name = 't1' AND column_name = 'x';
TRUNCATE TABLE t1;

DROP TABLE t1;
DROP PROCEDURE insert_repeat;
DROP PROCEDURE insert_range;
