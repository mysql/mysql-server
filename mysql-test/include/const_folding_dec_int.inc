# II.a decimal field vs decimal constant of different precision, range
eval CREATE TABLE t(d DECIMAL (3,1) $nullness);
eval INSERT INTO t VALUES (-10.5), (-10.4), (-10.3), ($nullval), (10.3), (10.4), (10.5);
ANALYZE TABLE t;
SELECT d FROM t;

--echo II.a.1   field cmp constant
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE d > 100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 100;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE d > -100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -100;
eval $query;
eval EXPLAIN $query;

--echo Positive constant
let $query=SELECT d FROM t WHERE d > 10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 10;
eval $query;
eval EXPLAIN $query;

--echo Negative constant

let $query=SELECT d FROM t WHERE d > -10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -10;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -10;
eval $query;
eval EXPLAIN $query;

--echo II.a.2 constant cmp field
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE 100 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100 <> d;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE -100 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100 <> d;
eval $query;
eval EXPLAIN $query;

--echo Ints inside strings
let $query=SELECT d FROM t WHERE d > '10';
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d > '100';
eval $query;
eval EXPLAIN $query;

--echo 0x notation

let $query=SELECT d FROM t WHERE d > 0x100;
eval $query;
eval EXPLAIN $query;

DROP TABLE t;

eval CREATE TABLE t(d DECIMAL(65, 30) $nullness);

INSERT INTO t VALUES (184467440737095516160000);
INSERT INTO t VALUES (18446744073709551.3);
ANALYZE TABLE t;

--echo 0x constant too large; gets truncated to zero
let $query=SELECT * FROM t WHERE d <= 0x1FFFFFFFFFFFFFFFF;
eval $query;
eval EXPLAIN $query;

--echo 0x constant within bounds, expect to warnings
let $query=SELECT * FROM t WHERE d > 0x7FFFFFFFFFFFFFFF;
eval $query;
eval EXPLAIN $query;

--echo 0x constant within bounds, expect to warnings
let $query=SELECT * FROM t WHERE d > 0xFFFFFFFFFFFFFFFF;
eval $query;
eval EXPLAIN $query;

--echo 0x constant within bounds, expect to warnings
let $query=SELECT * FROM t WHERE d < 0x7FFFFFFFFFFFFFFF;
eval $query;
eval EXPLAIN $query;

--echo 0x constant within bounds, expect to warnings
let $query=SELECT * FROM t WHERE d < 0xFFFFFFFFFFFFFFFF;
eval $query;
eval EXPLAIN $query;

DROP TABLE t;
