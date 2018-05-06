--echo #
--echo # Integer column, singleton histogram
--echo #
CREATE TABLE tbl_int (col1 INT);
INSERT INTO tbl_int VALUES (1), (2), (3), (4), (5), (6), (7), (8), (NULL), (NULL);
ANALYZE TABLE tbl_int;
ANALYZE TABLE tbl_int UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 > 0;
EXPLAIN SELECT * FROM tbl_int WHERE 0 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_int WHERE col1 > 8;
EXPLAIN SELECT * FROM tbl_int WHERE 8 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_int WHERE col1 < 0;
EXPLAIN SELECT * FROM tbl_int WHERE 0 > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 < 10;
EXPLAIN SELECT * FROM tbl_int WHERE 10 > col1;

--echo # Expect "30.0" in column "filtered"
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
EXPLAIN SELECT * FROM tbl_int WHERE col1 = 10;
EXPLAIN SELECT * FROM tbl_int WHERE 10 = col1;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IS NULL;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IS NOT NULL;

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 BETWEEN 1 AND 3;

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 NOT BETWEEN 1 AND 3;

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 IN (1, 3, 4, 5, 6, 7);

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_int WHERE col1 NOT IN (1, 3, 4, 5, 6, 7);

DROP TABLE tbl_int;

--echo #
--echo # String column, singleton histogram
--echo #
CREATE TABLE tbl_varchar (col1 VARCHAR(255));
INSERT INTO tbl_varchar VALUES
  ("abcd"), ("🍣"), ("🍺"), ("eeeeeeeeee"), ("ef"), ("AG"),
  ("a very long string that is longer than 42 characters"),
  ("lorem ipsum"), (NULL), (NULL);
ANALYZE TABLE tbl_varchar UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_varchar;

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 > "b";
EXPLAIN SELECT * FROM tbl_varchar WHERE "b" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 > "lp";
EXPLAIN SELECT * FROM tbl_varchar WHERE "lp" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 < "🍡";
EXPLAIN SELECT * FROM tbl_varchar WHERE "🍡" > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 < "sierra";
EXPLAIN SELECT * FROM tbl_varchar WHERE "sierra" > col1;

--echo # Expect "50.0" in column "filtered"
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

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 BETWEEN "a" AND "b";

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 NOT BETWEEN "a" AND "b";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 IN ("ag", "ef", "🍣");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_varchar WHERE col1 NOT IN ("ag", "ef", "🍣");

DROP TABLE tbl_varchar;


--echo #
--echo # Double column, singleton histogram
--echo #
CREATE TABLE tbl_double (col1 DOUBLE);
INSERT INTO tbl_double VALUES (-1.1), (0.0), (1.1), (2.2), (3.3), (4.4), (5.5), (6.6), (NULL), (NULL);
ANALYZE TABLE tbl_double UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_double;

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 > 0.0e0;
EXPLAIN SELECT * FROM tbl_double WHERE 0.0e0 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_double WHERE col1 > 6.6e0;
EXPLAIN SELECT * FROM tbl_double WHERE 6.6e0 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_double WHERE col1 < -2.2;
EXPLAIN SELECT * FROM tbl_double WHERE -2.2 > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 < 10.0;
EXPLAIN SELECT * FROM tbl_double WHERE 10.0 > col1;

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

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 BETWEEN 1.1e0 AND 3.3e0;

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 NOT BETWEEN 1.1e0 AND 3.3e0;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 IN (-1.1e0, 0.0e0, 1.1e0, 2.2e0);

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_double WHERE col1 NOT IN (-1.1e0, 0.0e0, 1.1e0, 2.2e0);

DROP TABLE tbl_double;


--echo #
--echo # Time column, singleton histogram
--echo #
CREATE TABLE tbl_time (col1 TIME);
INSERT INTO tbl_time VALUES
  ("-01:00:00"), ("00:00:00"), ("00:00:01"), ("00:01:00"), ("01:00:00"),
  ("01:01:00"), ("02:00:00"), ("03:00:00"), (NULL), (NULL);
ANALYZE TABLE tbl_time UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_time;

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 > "00:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "00:00:00" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_time WHERE col1 > "03:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "03:00:00" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_time WHERE col1 < "-02:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "-02:00:00" > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 < "10:00:00";
EXPLAIN SELECT * FROM tbl_time WHERE "10:00:00" > col1;

--echo # Expect "60.0" in column "filtered"
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

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 BETWEEN "00:00:01" AND "02:00:00";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 NOT BETWEEN "00:00:01" AND "02:00:00";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 IN ("-01:00:00", "00:00:00", "03:00:00");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_time WHERE col1 NOT IN ("-01:00:00", "00:00:00", "03:00:00");

DROP TABLE tbl_time;


--echo #
--echo # Date column, singleton histogram
--echo #
CREATE TABLE tbl_date (col1 DATE);
INSERT INTO tbl_date VALUES
  ("1000-01-02"), ("9999-12-30"), ("2017-01-01"), ("2017-01-02"), ("2017-02-01"),
  ("2018-01-01"), ("2019-01-01"), ("3019-01-01"), (NULL), (NULL);
ANALYZE TABLE tbl_date UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_date;

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 > "2017-01-02";
EXPLAIN SELECT * FROM tbl_date WHERE "2017-01-02" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_date WHERE col1 > "9999-12-31";
EXPLAIN SELECT * FROM tbl_date WHERE "9999-12-31" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_date WHERE col1 < "1000-01-01";
EXPLAIN SELECT * FROM tbl_date WHERE "1000-01-01" > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 < "9999-12-31";
EXPLAIN SELECT * FROM tbl_date WHERE "9999-12-31" > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 >= "2018-01-01";
EXPLAIN SELECT * FROM tbl_date WHERE "2018-01-01" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 >= "1000-01-02";
EXPLAIN SELECT * FROM tbl_date WHERE "1000-01-02" <= col1;

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

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 BETWEEN "2017-01-01" AND "3019-01-01";

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 NOT BETWEEN "2017-01-01" AND "3019-01-01";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 IN ("1000-01-02", "2017-01-02", "2018-01-01");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_date WHERE col1 NOT IN ("1000-01-02", "2017-01-02", "2018-01-01");

DROP TABLE tbl_date;


--echo #
--echo # Datetime column, singleton histogram
--echo #
CREATE TABLE tbl_datetime (col1 DATETIME(6));
INSERT INTO tbl_datetime VALUES
  ("1000-01-01 00:00:01"), ("9999-12-31 23:59:59.999998"),
  ("2017-01-01 00:00:00"), ("2017-01-01 00:00:00.000001"),
  ("2017-02-01 00:00:00"), ("2018-01-01 00:00:00.999999"),
  ("2018-01-01 00:00:01"), ("3019-01-01 10:10:10.101010"), (NULL), (NULL);
ANALYZE TABLE tbl_datetime UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_datetime;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 > "2018-01-01 00:00:00";
EXPLAIN SELECT * FROM tbl_datetime WHERE "2018-01-01 00:00:00" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 > "9999-12-31 23:59:59.999998";
EXPLAIN SELECT * FROM tbl_datetime WHERE "9999-12-31 23:59:59.999998" < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 < "1000-01-01 00:00:00";
EXPLAIN SELECT * FROM tbl_datetime WHERE "1000-01-01 00:00:00" > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 < "9999-12-31 23:59:59.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "9999-12-31 23:59:59.999999" > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 >= "2018-01-01 00:00:00.999999";
EXPLAIN SELECT * FROM tbl_datetime WHERE "2018-01-01 00:00:00.999999" <= col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 >= "1000-01-01 00:00:01.000000";
EXPLAIN SELECT * FROM tbl_datetime WHERE "1000-01-01 00:00:01.000000" <= col1;

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

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 BETWEEN "2017-01-01 00:00:00.000001" AND "3019-01-01 10:10:10.101010";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 NOT BETWEEN "2017-01-01 00:00:00.000001" AND "3019-01-01 10:10:10.101010";

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 IN ("1000-01-01 00:00:01.000000", "2018-01-01 00:00:00.999999", "9999-12-31 23:59:59.999998");

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_datetime WHERE col1 NOT IN ("1000-01-01 00:00:01.000000", "2018-01-01 00:00:00.999999", "9999-12-31 23:59:59.999998");

DROP TABLE tbl_datetime;


--echo #
--echo # Decimal column, singleton histogram
--echo #
CREATE TABLE tbl_decimal (col1 DECIMAL(65, 30));
INSERT INTO tbl_decimal VALUES
  (00000000000000000000000000000000000.000000000000000000000000000000),
  (99999999999999999999999999999999999.999999999999999999999999999998),
  (-99999999999999999999999999999999999.999999999999999999999999999998),
  (1), (2), (3), (4), (-1), (NULL), (NULL);
ANALYZE TABLE tbl_decimal UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_decimal;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 > 1;
EXPLAIN SELECT * FROM tbl_decimal WHERE 1 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 > 100000000000000000000000000000000000;
EXPLAIN SELECT * FROM tbl_decimal WHERE 100000000000000000000000000000000000 < col1;

--echo # Expect "10.0" in column "filtered" (we never estimate 0%)
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 < -99999999999999999999999999999999999.999999999999999999999999999999;
EXPLAIN SELECT * FROM tbl_decimal WHERE -99999999999999999999999999999999999.999999999999999999999999999999 > col1;

--echo # Expect "80.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 < 99999999999999999999999999999999999.999999999999999999999999999999;
EXPLAIN SELECT * FROM tbl_decimal WHERE 99999999999999999999999999999999999.999999999999999999999999999999 > col1;

--echo # Expect "40.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 >= 4;
EXPLAIN SELECT * FROM tbl_decimal WHERE 4 <= col1;

--echo # Expect "70.0" in column "filtered"
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

--echo # Expect "60.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 BETWEEN -1.0 AND 4.0;

--echo # Expect "20.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 NOT BETWEEN -1.0 AND 4.0;

--echo # Expect "30.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 IN
  (-99999999999999999999999999999999999.999999999999999999999999999998, 1.0, 2.0);

--echo # Expect "50.0" in column "filtered"
EXPLAIN SELECT * FROM tbl_decimal WHERE col1 NOT IN
  (-99999999999999999999999999999999999.999999999999999999999999999998, 1.0, 2.0);

DROP TABLE tbl_decimal;

--echo #
--echo # ENUM column, singleton histogram
--echo # Note that we only support equality/inequality operators for ENUM
--echo # columns.
--echo #
CREATE TABLE tbl_enum (col1 ENUM('red', 'black', 'blue', 'green'));
INSERT INTO tbl_enum VALUES ('red'), ('red'), ('black'), ('blue'), ('green'),
                            ('green'), (NULL), (NULL), (NULL);
ANALYZE TABLE tbl_enum UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_enum;

--echo # Expect "22.22" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 'red';
EXPLAIN SELECT * FROM tbl_enum WHERE 'red' = col1;

--echo # Expect "55.56" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 'black';
EXPLAIN SELECT * FROM tbl_enum WHERE 'black' != col1;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 'black';
EXPLAIN SELECT * FROM tbl_enum WHERE 'black' <> col1;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != '';
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 0;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> '';
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 0;

--echo # Expect "44.44" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IN ('black', 'blue', 'green');

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 NOT IN ('green', 'blue');

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IS NULL;

--echo # Expect "66.67" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IS NOT NULL;


--echo # Test that the numerical representation of enum values also gives the
--echo # correct result.

--echo # Expect "22.22" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 1;
EXPLAIN SELECT * FROM tbl_enum WHERE 1 = col1;

--echo # Expect "55.56" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 != 2;
EXPLAIN SELECT * FROM tbl_enum WHERE 2 != col1;
EXPLAIN SELECT * FROM tbl_enum WHERE col1 <> 2;
EXPLAIN SELECT * FROM tbl_enum WHERE 2 <> col1;

--echo # Expect "11.11" in column "filtered", due to the fact that the optimizer
--echo # always assumes that at least one row will match.
EXPLAIN SELECT * FROM tbl_enum WHERE col1 = 100;
EXPLAIN SELECT * FROM tbl_enum WHERE 100 = col1;

--echo # Expect "44.44" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 IN (2, 3, 4);

--echo # Expect "33.33" in column "filtered"
EXPLAIN SELECT * FROM tbl_enum WHERE col1 NOT IN (4, 3);

DROP TABLE tbl_enum;


--echo #
--echo # SET column, singleton histogram
--echo # Note that we only support equality/inequality operators for SET
--echo # columns.
--echo #
CREATE TABLE tbl_set (col1 SET('red', 'black', 'blue', 'green'));
INSERT INTO tbl_set VALUES ('red'), ('red,black'), ('black,green,blue'),
                           ('black,green,blue'), ('black,green,blue'),
                           ('green'), ('green,red'), ('red,green'), (NULL),
                           (NULL), (NULL);
ANALYZE TABLE tbl_set UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
ANALYZE TABLE tbl_set;

--echo # Expect "18.18" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 = 'red,green';
EXPLAIN SELECT * FROM tbl_set WHERE 'red,green' = col1;

--echo # Expect "63.64" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 'red';
EXPLAIN SELECT * FROM tbl_set WHERE 'red' != col1;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 'red';
EXPLAIN SELECT * FROM tbl_set WHERE 'red' <> col1;

--echo # Expect "72.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 != '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 != 0;
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> '';
EXPLAIN SELECT * FROM tbl_set WHERE col1 <> 0;

--echo # Expect "36.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IN ('green', 'black,blue,green');

--echo # Expect "36.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 NOT IN ('green', 'black,blue,green');

--echo # Expect "27.27" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IS NULL;

--echo # Expect "72.73" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IS NOT NULL;


--echo # Test that the numerical representation of enum values also gives the
--echo # correct result.

--echo # Expect "18.18" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 = 9;
EXPLAIN SELECT * FROM tbl_set WHERE 9 = col1;

--echo # Expect "63.64" in column "filtered"
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

--echo # Expect "36.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 IN (8, 14);

--echo # Expect "36.36" in column "filtered"
EXPLAIN SELECT * FROM tbl_set WHERE col1 NOT IN (8, 14);

DROP TABLE tbl_set;
