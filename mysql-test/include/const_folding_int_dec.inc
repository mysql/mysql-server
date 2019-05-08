--echo #
--echo # Tests with $signedness type $int_type $nullness against decimal
--echo #
eval CREATE TABLE $table_name (colA $int_type $signedness $nullness);
eval INSERT INTO $table_name VALUES ($minint), ($minint_plus_1),
                                    (0),(1),(3),
                                    ($maxint_minus_1), ($maxint),
                                    ($nullval);
eval ANALYZE TABLE $table_name;

eval SELECT * FROM $table_name;

eval SELECT * FROM $table_name WHERE colA < $dec_0_5;
eval SELECT * FROM $table_name WHERE colA <= $dec_0_5;
eval SELECT * FROM $table_name WHERE colA > $dec_0_5;
eval SELECT * FROM $table_name WHERE colA >= $dec_0_5;
eval SELECT * FROM $table_name WHERE colA = $dec_0_5;
eval SELECT * FROM $table_name WHERE colA <> $dec_0_5;

eval SELECT * FROM $table_name WHERE colA < $dec_minus_0_5;
eval SELECT * FROM $table_name WHERE colA <= $dec_minus_0_5;
eval SELECT * FROM $table_name WHERE colA > $dec_minus_0_5;
eval SELECT * FROM $table_name WHERE colA >= $dec_minus_0_5;
eval SELECT * FROM $table_name WHERE colA = $dec_minus_0_5;
eval SELECT * FROM $table_name WHERE colA <> $dec_minus_0_5;

eval SELECT * FROM $table_name WHERE colA < $dec_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA <= $dec_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA > $dec_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA >= $dec_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA = $dec_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA <> $dec_max_plus_delta;

eval SELECT * FROM $table_name WHERE colA < $dec_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA <= $dec_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA > $dec_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA >= $dec_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA = $dec_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA <> $dec_max_minus_delta;

eval SELECT * FROM $table_name WHERE colA < $dec_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA <= $dec_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA > $dec_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA >= $dec_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA = $dec_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA <> $dec_min_plus_delta;

eval SELECT * FROM $table_name WHERE colA < $dec_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA <= $dec_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA > $dec_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA >= $dec_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA = $dec_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA <> $dec_min_minus_delta;

eval SELECT * FROM $table_name WHERE colA < $dec_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA <= $dec_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA > $dec_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA >= $dec_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA = $dec_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA <> $dec_too_big_positive_for_any_int;

eval SELECT * FROM $table_name WHERE colA < $dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA <= $dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA > $dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA >= $dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA = $dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA <> $dec_too_big_negative_for_any_int;

eval PREPARE p_less    FROM 'SELECT * FROM $table_name WHERE colA < ?';
eval PREPARE p_less_eq FROM 'SELECT * FROM $table_name WHERE colA <= ?';
eval PREPARE p_greater FROM 'SELECT * FROM $table_name WHERE colA > ?';
eval PREPARE p_gret_eq FROM 'SELECT * FROM $table_name WHERE colA >= ?';
eval PREPARE p_equal   FROM 'SELECT * FROM $table_name WHERE colA = ?';
eval PREPARE p_n_equal FROM 'SELECT * FROM $table_name WHERE colA <> ?';

eval SET @dec_0_5=$dec_0_5;
eval SET @dec_minus_0_5=$dec_minus_0_5;
eval SET @dec_max_plus_delta=$dec_max_plus_delta;
eval SET @dec_max_minus_delta=$dec_max_minus_delta;
eval SET @dec_min_plus_delta=$dec_min_plus_delta;
eval SET @dec_min_minus_delta=$dec_min_minus_delta;
eval SET @dec_too_big_positive_for_any_int=$dec_too_big_positive_for_any_int;
eval SET @dec_too_big_negative_for_any_int=$dec_too_big_negative_for_any_int;

eval EXECUTE p_less    USING @dec_0_5;
eval EXECUTE p_less_eq USING @dec_0_5;
eval EXECUTE p_greater USING @dec_0_5;
eval EXECUTE p_gret_eq USING @dec_0_5;
eval EXECUTE p_equal   USING @dec_0_5;
eval EXECUTE p_n_equal USING @dec_0_5;

eval EXECUTE p_less    USING @dec_minus_0_5;
eval EXECUTE p_less_eq USING @dec_minus_0_5;
eval EXECUTE p_greater USING @dec_minus_0_5;
eval EXECUTE p_gret_eq USING @dec_minus_0_5;
eval EXECUTE p_equal   USING @dec_minus_0_5;
eval EXECUTE p_n_equal USING @dec_minus_0_5;

eval EXECUTE p_less    USING @dec_max_plus_delta;
eval EXECUTE p_less_eq USING @dec_max_plus_delta;
eval EXECUTE p_greater USING @dec_max_plus_delta;
eval EXECUTE p_gret_eq USING @dec_max_plus_delta;
eval EXECUTE p_equal   USING @dec_max_plus_delta;
eval EXECUTE p_n_equal USING @dec_max_plus_delta;

eval EXECUTE p_less    USING @dec_max_minus_delta;
eval EXECUTE p_less_eq USING @dec_max_minus_delta;
eval EXECUTE p_greater USING @dec_max_minus_delta;
eval EXECUTE p_gret_eq USING @dec_max_minus_delta;
eval EXECUTE p_equal   USING @dec_max_minus_delta;
eval EXECUTE p_n_equal USING @dec_max_minus_delta;

eval EXECUTE p_less    USING @dec_min_plus_delta;
eval EXECUTE p_less_eq USING @dec_min_plus_delta;
eval EXECUTE p_greater USING @dec_min_plus_delta;
eval EXECUTE p_gret_eq USING @dec_min_plus_delta;
eval EXECUTE p_equal   USING @dec_min_plus_delta;
eval EXECUTE p_n_equal USING @dec_min_plus_delta;

eval EXECUTE p_less    USING @dec_min_minus_delta;
eval EXECUTE p_less_eq USING @dec_min_minus_delta;
eval EXECUTE p_greater USING @dec_min_minus_delta;
eval EXECUTE p_gret_eq USING @dec_min_minus_delta;
eval EXECUTE p_equal   USING @dec_min_minus_delta;
eval EXECUTE p_n_equal USING @dec_min_minus_delta;

eval EXECUTE p_less    USING @dec_too_big_positive_for_any_int;
eval EXECUTE p_less_eq USING @dec_too_big_positive_for_any_int;
eval EXECUTE p_greater USING @dec_too_big_positive_for_any_int;
eval EXECUTE p_gret_eq USING @dec_too_big_positive_for_any_int;
eval EXECUTE p_equal   USING @dec_too_big_positive_for_any_int;
eval EXECUTE p_n_equal USING @dec_too_big_positive_for_any_int;

eval EXECUTE p_less    USING @dec_too_big_negative_for_any_int;
eval EXECUTE p_less_eq USING @dec_too_big_negative_for_any_int;
eval EXECUTE p_greater USING @dec_too_big_negative_for_any_int;
eval EXECUTE p_gret_eq USING @dec_too_big_negative_for_any_int;
eval EXECUTE p_equal   USING @dec_too_big_negative_for_any_int;
eval EXECUTE p_n_equal USING @dec_too_big_negative_for_any_int;

DROP PREPARE p_less;
DROP PREPARE p_less_eq;
DROP PREPARE p_greater;
DROP PREPARE p_gret_eq;
DROP PREPARE p_equal;
DROP PREPARE p_n_equal;

eval DROP TABLE $table_name;

--echo Cruft inside string
CREATE TABLE t(i INT);
INSERT INTO t VALUES (-1), (0), (1);
ANALYZE TABLE t;
let $query=SELECT * FROM t WHERE i > 'A';
eval $query;
eval EXPLAIN $query;
DROP TABLE t;

--echo Detect decimal inside a string when compared to int
--echo Used to return one row; should be none.
CREATE TABLE t(i BIGINT);
ANALYZE TABLE t;
INSERT INTO t VALUES (1), (2);
let $query=SELECT * FROM t WHERE i = '1.1';
eval $query;
eval EXPLAIN $query;

DROP TABLE t;

--echo Detect out of range real inside a string when compared to int
--echo Used to return one row; should be none.
CREATE TABLE t(i TINYINT);
ANALYZE TABLE t;
INSERT INTO t VALUES (1), (2);
let $query=SELECT * FROM t WHERE i > '1E10';
eval $query;
eval EXPLAIN $query;

DROP TABLE t;

--echo Detect out of range real inside a string when compared to int
--echo Used to return one row; should be none.
CREATE TABLE t(i TINYINT);
ANALYZE TABLE t;
INSERT INTO t VALUES (0);
let $query=SELECT * FROM t WHERE i = '1E-308';
eval $query;
eval EXPLAIN $query;

--echo Above query should yield the same as this:
let $query=SELECT * FROM t WHERE i = 1E-308;
eval $query;
eval EXPLAIN $query;

DROP TABLE t;
