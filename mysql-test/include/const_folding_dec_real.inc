# II.a decimal field vs real constant of different precision, range
eval create table t(d decimal (3,1) $nullness);
eval insert into t values (-10.5), (-10.4), (-10.3), ($nullval), (10.3), (10.4), (10.5);
analyze table t;
SELECT d FROM t;

--echo II.a.1   field cmp constant
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE d > 100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 100.0E00;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE d > -100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -100.0E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -100.0E00;
eval $query;
eval EXPLAIN $query;

--echo Truncated positive constant
let $query=SELECT d FROM t WHERE d > 10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 10.31E00;
eval $query;
eval EXPLAIN $query;

--echo Truncated negative constant

let $query=SELECT d FROM t WHERE d > -10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -10.31E00;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -10.31E00;
eval $query;
eval EXPLAIN $query;

--echo II.a.2 constant cmp field
--echo Out of range, positive constant
let $query=SELECT d FROM t WHERE 100.0E00 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0E00 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0E00 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0E00 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0E00 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 100.0E00 <> d;
eval $query;
eval EXPLAIN $query;

--echo Out of range, negative constant

let $query=SELECT d FROM t WHERE -100.0E00 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0E00 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0E00 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0E00 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0E00 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -100.0E00 <> d;
eval $query;
eval EXPLAIN $query;

--echo Truncated positive constant
let $query=SELECT d FROM t WHERE 10.31E00 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31E00 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31E00 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31E00 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31E00 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE 10.31E00 <> d;
eval $query;
eval EXPLAIN $query;

--echo Truncated negative constant

let $query=SELECT d FROM t WHERE -10.31E00 < d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31E00 <= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31E00 > d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31E00 >= d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31E00 = d;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE -10.31E00 <> d;
eval $query;
eval EXPLAIN $query;

--echo Check real underflow
let $query=SELECT d FROM t WHERE d > 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> 1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d > -1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d >= -1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d < -1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <= -1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d = -1.0E-100;
eval $query;
eval EXPLAIN $query;

let $query=SELECT d FROM t WHERE d <> -1.0E-100;
eval $query;
eval EXPLAIN $query;

--echo Test that reals inside strings are handled ok
let $query=SELECT d FROM t WHERE d < '1.0E0';
eval $query;
eval EXPLAIN $query;


DROP TABLE t;

--echo Check that #digits in constant fraction is widenened as for fields
CREATE TABLE t(d DECIMAL(65,30));
INSERT INTO t VALUES (10.34999), (10.350001);
let $query=SELECT * FROM t WHERE d > 0.1035E02;
eval $query;
eval EXPLAIN $query;
DROP TABLE t;
