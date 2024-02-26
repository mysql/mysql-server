# II.a decimal field vs decimal constant of different precision, range
eval CREATE TABLE t(d DECIMAL (3,1) $nullness);
eval INSERT INTO t VALUES (-10.5), (-10.4), (-10.3), ($nullval), (10.3), (10.4), (10.5);
ANALYZE TABLE t;
SELECT d FROM t;

--echo II.a.1   field cmp constant
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE d > 100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 100.0;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE d > -100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -100.0;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -100.0;
eval $query;
eval EXPLAIN $query;

--echo Truncated positive constant
let $query=SELECT d FROM t WHERE d > 10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 10.31;
eval $query;
eval EXPLAIN $query;

--echo Truncated negative constant

let $query=SELECT d FROM t WHERE d > -10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -10.31;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -10.31;
eval $query;
eval EXPLAIN $query;

--echo II.a.2 constant cmp field
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE 100.0 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0 <> d;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE -100.0 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0 <> d;
eval $query;
eval EXPLAIN $query;

--echo Truncated positive constant
let $query=SELECT d FROM t WHERE 10.31 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31 <> d;
eval $query;
eval EXPLAIN $query;

--echo Truncated negative constant

let $query=SELECT d FROM t WHERE -10.31 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31 <> d;
eval $query;
eval EXPLAIN $query;

--echo Check that leading and trailing zeros are handled
let $query=SELECT d FROM t WHERE d > 10.30;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d > 010.3;
eval $query;
eval EXPLAIN $query;

--echo Check decimals hidden inside strings
let $query=SELECT d FROM t WHERE d > '-10.31';
eval $query;
eval EXPLAIN $query;

--echo Cruft inside string, MySQL interprets as 0 with warning
let $query=SELECT d FROM t WHERE d > 'A';
eval $query;
eval EXPLAIN $query;


DROP TABLE t;

--echo Check that #digits in constant fraction is widenened as for fields
CREATE TABLE t(d DECIMAL(65,30));
INSERT INTO t VALUES (10.34999), (10.350001);
let $query=SELECT * FROM t WHERE d > 10.35;
eval $query;
eval EXPLAIN $query;
DROP TABLE t;

