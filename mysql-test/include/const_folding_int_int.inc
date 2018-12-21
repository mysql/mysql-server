--echo #
--echo # Tests with $signedness type $int_type $nullness against int
--echo #
eval CREATE TABLE $table_name (colA $int_type $signedness $nullness);
eval INSERT INTO $table_name VALUES ($minint), ($minint_plus_1),
                                    (0),(1),(3),
                                    ($maxint_minus_1), ($maxint),
                                    ($nullval);
eval ANALYZE TABLE $table_name;

eval SELECT * FROM $table_name;

eval SELECT * FROM $table_name WHERE colA < $maxint_plus_1;
eval SELECT * FROM $table_name WHERE colA <= $maxint;
eval SELECT * FROM $table_name WHERE colA > $maxint_plus_1;
eval SELECT * FROM $table_name WHERE colA >= $maxint;

eval SELECT * FROM $table_name WHERE colA > $minint_minus_1;
eval SELECT * FROM $table_name WHERE colA <= $minint_minus_1;

eval SELECT * FROM $table_name WHERE colA <= $minint;
eval SELECT * FROM $table_name WHERE colA >= $minint;
eval SELECT * FROM $table_name WHERE colA > $minint;

eval SELECT * FROM $table_name WHERE colA = $maxint_plus_1;
eval SELECT * FROM $table_name WHERE colA <> $maxint_plus_1;
eval SELECT * FROM $table_name WHERE colA = $maxint;
eval SELECT * FROM $table_name WHERE colA <=> $maxint;
eval SELECT * FROM $table_name WHERE colA <> $maxint;
eval SELECT * FROM $table_name WHERE colA = $minint;
eval SELECT * FROM $table_name WHERE colA <=> $minint;
eval SELECT * FROM $table_name WHERE colA <> $minint;
eval SELECT * FROM $table_name WHERE colA = $minint_minus_1;
eval SELECT * FROM $table_name WHERE colA <=> $minint_minus_1;
eval SELECT * FROM $table_name WHERE colA <> $minint_minus_1;

eval PREPARE p_less    FROM 'SELECT * FROM $table_name WHERE colA < ?';
eval PREPARE p_less_eq FROM 'SELECT * FROM $table_name WHERE colA <= ?';
eval PREPARE p_greater FROM 'SELECT * FROM $table_name WHERE colA > ?';
eval PREPARE p_gret_eq FROM 'SELECT * FROM $table_name WHERE colA >= ?';
eval PREPARE p_eq      FROM 'SELECT * FROM $table_name WHERE colA = ?';
eval PREPARE p_equal   FROM 'SELECT * FROM $table_name WHERE colA <=> ?';
eval PREPARE p_n_equal FROM 'SELECT * FROM $table_name WHERE colA <> ?';

eval SET @maxint_plus_1=$maxint_plus_1;
eval SET @maxint=$maxint;
eval SET @minint_minus_1=$minint_minus_1;
eval SET @minint=$minint;

eval EXECUTE p_less    USING @maxint_plus_1;
eval EXECUTE p_less_eq USING @maxint;
eval EXECUTE p_greater USING @maxint_plus_1;
eval EXECUTE p_gret_eq USING @maxint;

eval EXECUTE p_greater USING @minint_minus_1;
eval EXECUTE p_less_eq USING @minint_minus_1;

eval EXECUTE p_less_eq USING @minint;
eval EXECUTE p_gret_eq USING @minint;
eval EXECUTE p_greater USING @minint;

eval EXECUTE p_eq      USING @maxint_plus_1;
eval EXECUTE p_equal   USING @maxint_plus_1;
eval EXECUTE p_n_equal USING @maxint_plus_1;
eval EXECUTE p_eq      USING @maxint;
eval EXECUTE p_equal   USING @maxint;
eval EXECUTE p_n_equal USING @maxint;
eval EXECUTE p_eq      USING @minint;
eval EXECUTE p_equal   USING @minint;
eval EXECUTE p_n_equal USING @minint;
eval EXECUTE p_eq      USING @minint_minus_1;
eval EXECUTE p_equal   USING @minint_minus_1;
eval EXECUTE p_n_equal USING @minint_minus_1;

DROP PREPARE p_less;
DROP PREPARE p_less_eq;
DROP PREPARE p_greater;
DROP PREPARE p_gret_eq;
DROP PREPARE p_equal;
DROP PREPARE p_n_equal;

eval EXPLAIN SELECT * FROM $table_name WHERE colA < $maxint_plus_1;   # always true
eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxint;  # always true
eval EXPLAIN SELECT * FROM $table_name WHERE colA > $maxint_plus_1;   # always false
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxint;  # cnvt to = $maxint

eval EXPLAIN SELECT * FROM $table_name WHERE colA > $minint_minus_1;    # always true
eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minint_minus_1;   # always false

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minint;    # convert to = $minint
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minint;    # always true
eval EXPLAIN SELECT * FROM $table_name WHERE colA > $minint;

eval EXPLAIN SELECT * FROM $table_name WHERE colA = $maxint_plus_1;   # always false
eval EXPLAIN SELECT * FROM $table_name WHERE colA <=> $maxint_plus_1;   # always false
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxint_plus_1;  # always true
eval EXPLAIN SELECT * FROM $table_name WHERE colA = $maxint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <=> $maxint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA = $minint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <=> $minint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minint;
eval EXPLAIN SELECT * FROM $table_name WHERE colA = $minint_minus_1;    # always false
eval EXPLAIN SELECT * FROM $table_name WHERE colA <=> $minint_minus_1;    # always false
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minint_minus_1;   # always true

--echo same queries but constant first
eval SELECT * FROM $table_name WHERE $maxint_plus_1 > colA;
eval SELECT * FROM $table_name WHERE $maxint >= colA;
eval SELECT * FROM $table_name WHERE $maxint_plus_1 < colA;
eval SELECT * FROM $table_name WHERE $maxint <= colA;
eval SELECT * FROM $table_name WHERE $maxint < colA;

eval SELECT * FROM $table_name WHERE $minint_minus_1 < colA;
eval SELECT * FROM $table_name WHERE $minint_minus_1 >= colA;

eval SELECT * FROM $table_name WHERE $minint >= colA;
eval SELECT * FROM $table_name WHERE $minint <= colA;
eval SELECT * FROM $table_name WHERE $minint < colA;

eval SELECT * FROM $table_name WHERE $maxint_plus_1 = colA;
eval SELECT * FROM $table_name WHERE $maxint_plus_1 <=> colA;
eval SELECT * FROM $table_name WHERE $maxint_plus_1 <> colA;
eval SELECT * FROM $table_name WHERE $maxint = colA;
eval SELECT * FROM $table_name WHERE $maxint <=> colA;
eval SELECT * FROM $table_name WHERE $maxint <> colA;
eval SELECT * FROM $table_name WHERE $minint = colA;
eval SELECT * FROM $table_name WHERE $minint <=> colA;
eval SELECT * FROM $table_name WHERE $minint <> colA;
eval SELECT * FROM $table_name WHERE $minint_minus_1 = colA;
eval SELECT * FROM $table_name WHERE $minint_minus_1 <=> colA;
eval SELECT * FROM $table_name WHERE $minint_minus_1 <> colA;

eval PREPARE p_less    FROM 'SELECT * FROM $table_name WHERE ? < colA';
eval PREPARE p_less_eq FROM 'SELECT * FROM $table_name WHERE ? <= colA';
eval PREPARE p_greater FROM 'SELECT * FROM $table_name WHERE ? > colA';
eval PREPARE p_gret_eq FROM 'SELECT * FROM $table_name WHERE ? >= colA';
eval PREPARE p_eq      FROM 'SELECT * FROM $table_name WHERE ? = colA';
eval PREPARE p_equal   FROM 'SELECT * FROM $table_name WHERE ? <=> colA';
eval PREPARE p_n_equal FROM 'SELECT * FROM $table_name WHERE ? <> colA';

eval EXECUTE p_greater USING @maxint_plus_1;
eval EXECUTE p_gret_eq USING @maxint;
eval EXECUTE p_less    USING @maxint_plus_1;
eval EXECUTE p_less_eq USING @maxint;

eval EXECUTE p_less    USING @minint_minus_1;
eval EXECUTE p_gret_eq USING @minint_minus_1;

eval EXECUTE p_gret_eq USING @minint;
eval EXECUTE p_less_eq USING @minint;
eval EXECUTE p_less    USING @minint;

eval EXECUTE p_eq      USING @maxint_plus_1;
eval EXECUTE p_equal   USING @maxint_plus_1;
eval EXECUTE p_n_equal USING @maxint_plus_1;
eval EXECUTE p_eq      USING @maxint;
eval EXECUTE p_equal   USING @maxint;
eval EXECUTE p_n_equal USING @maxint;
eval EXECUTE p_eq      USING @minint;
eval EXECUTE p_equal   USING @minint;
eval EXECUTE p_n_equal USING @minint;
eval EXECUTE p_eq      USING @minint_minus_1;
eval EXECUTE p_equal   USING @minint_minus_1;
eval EXECUTE p_n_equal USING @minint_minus_1;

DROP PREPARE p_less;
DROP PREPARE p_less_eq;
DROP PREPARE p_greater;
DROP PREPARE p_gret_eq;
DROP PREPARE p_equal;
DROP PREPARE p_n_equal;

eval EXPLAIN SELECT * FROM $table_name WHERE $maxint_plus_1 > colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint >= colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint_plus_1 < colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint <= colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint < colA;

eval EXPLAIN SELECT * FROM $table_name WHERE $minint_minus_1 < colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint_minus_1 >= colA;

eval EXPLAIN SELECT * FROM $table_name WHERE $minint >= colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint <= colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint < colA;

eval EXPLAIN SELECT * FROM $table_name WHERE $maxint_plus_1 = colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint_plus_1 <=> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint_plus_1 <> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint = colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint <=> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $maxint <> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint = colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint <=> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint <> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint_minus_1 = colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint_minus_1 <=> colA;
eval EXPLAIN SELECT * FROM $table_name WHERE $minint_minus_1 <> colA;

eval DROP TABLE $table_name;
