--echo #
--echo # Tests with $signedness type $int_type $nullness against reals
--echo #
eval CREATE TABLE $table_name (colA $int_type $signedness $nullness);
eval INSERT INTO $table_name VALUES ($minint), ($minint_plus_1),
                                    (0),(1),(3),
                                    ($maxint_minus_1), ($maxint),
                                    ($nullval);
eval ANALYZE TABLE $table_name;

eval SELECT * FROM $table_name;

eval SELECT * FROM $table_name WHERE colA < $real_0_5;
eval SELECT * FROM $table_name WHERE colA <= $real_0_5;
eval SELECT * FROM $table_name WHERE colA > $real_0_5;
eval SELECT * FROM $table_name WHERE colA >= $real_0_5;
eval SELECT * FROM $table_name WHERE colA = $real_0_5;
eval SELECT * FROM $table_name WHERE colA <> $real_0_5;

eval SELECT * FROM $table_name WHERE colA < $real_minus_0_5;
eval SELECT * FROM $table_name WHERE colA <= $real_minus_0_5;
eval SELECT * FROM $table_name WHERE colA > $real_minus_0_5;
eval SELECT * FROM $table_name WHERE colA >= $real_minus_0_5;
eval SELECT * FROM $table_name WHERE colA = $real_minus_0_5;
eval SELECT * FROM $table_name WHERE colA <> $real_minus_0_5;

eval SELECT * FROM $table_name WHERE colA < $real_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA <= $real_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA > $real_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA >= $real_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA = $real_max_plus_delta;
eval SELECT * FROM $table_name WHERE colA <> $real_max_plus_delta;

eval SELECT * FROM $table_name WHERE colA < $real_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA <= $real_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA > $real_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA >= $real_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA = $real_max_minus_delta;
eval SELECT * FROM $table_name WHERE colA <> $real_max_minus_delta;

eval SELECT * FROM $table_name WHERE colA < $real_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA <= $real_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA > $real_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA >= $real_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA = $real_min_plus_delta;
eval SELECT * FROM $table_name WHERE colA <> $real_min_plus_delta;

eval SELECT * FROM $table_name WHERE colA < $real_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA <= $real_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA > $real_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA >= $real_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA = $real_min_minus_delta;
eval SELECT * FROM $table_name WHERE colA <> $real_min_minus_delta;

eval SELECT * FROM $table_name WHERE colA < $real_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA <= $real_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA > $real_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA >= $real_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA = $real_too_big_positive_for_any_int;
eval SELECT * FROM $table_name WHERE colA <> $real_too_big_positive_for_any_int;

eval SELECT * FROM $table_name WHERE colA < $real_dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA <= $real_dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA > $real_dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA >= $real_dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA = $real_dec_too_big_negative_for_any_int;
eval SELECT * FROM $table_name WHERE colA <> $real_dec_too_big_negative_for_any_int;

eval SELECT * FROM $table_name WHERE colA < $real_too_big_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <= $real_too_big_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA > $real_too_big_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA >= $real_too_big_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA = $real_too_big_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <> $real_too_big_positive_for_any_decimal;

eval SELECT * FROM $table_name WHERE colA < $real_too_big_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <= $real_too_big_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA > $real_too_big_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA >= $real_too_big_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA = $real_too_big_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <> $real_too_big_negative_for_any_decimal;

eval SELECT * FROM $table_name WHERE colA < $real_too_small_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <= $real_too_small_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA > $real_too_small_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA >= $real_too_small_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA = $real_too_small_positive_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <> $real_too_small_positive_for_any_decimal;

eval SELECT * FROM $table_name WHERE colA < $real_too_small_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <= $real_too_small_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA > $real_too_small_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA >= $real_too_small_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA = $real_too_small_negative_for_any_decimal;
eval SELECT * FROM $table_name WHERE colA <> $real_too_small_negative_for_any_decimal;

eval PREPARE p_less    FROM 'SELECT * FROM $table_name WHERE colA < ?';
eval PREPARE p_less_eq FROM 'SELECT * FROM $table_name WHERE colA <= ?';
eval PREPARE p_greater FROM 'SELECT * FROM $table_name WHERE colA > ?';
eval PREPARE p_gret_eq FROM 'SELECT * FROM $table_name WHERE colA >= ?';
eval PREPARE p_equal   FROM 'SELECT * FROM $table_name WHERE colA = ?';
eval PREPARE p_n_equal FROM 'SELECT * FROM $table_name WHERE colA <> ?';

eval SET @real_0_5=$real_0_5;
eval SET @real_minus_0_5=$real_minus_0_5;
eval SET @real_max_plus_delta=$real_max_plus_delta;
eval SET @real_max_minus_delta=$real_max_minus_delta;
eval SET @real_min_plus_delta=$real_min_plus_delta;
eval SET @real_min_minus_delta=$real_min_minus_delta;
eval SET @real_too_big_positive_for_any_int=$real_too_big_positive_for_any_int;
eval SET @real_dec_too_big_negative_for_any_int=$real_dec_too_big_negative_for_any_int;
eval SET @real_too_big_positive_for_any_decimal=$real_too_big_positive_for_any_decimal;
eval SET @real_too_big_negative_for_any_decimal=$real_too_big_negative_for_any_decimal;
eval SET @dec_too_small_positive_for_any_int_as_real=$real_too_small_positive_for_any_decimal;
eval SET @dec_too_small_negative_for_any_int_as_real=$real_too_small_negative_for_any_decimal;
eval SET @real_too_small_positive_for_any_decimal=$real_too_small_positive_for_any_decimal;
eval SET @real_too_small_negative_for_any_decimal=$real_too_small_negative_for_any_decimal;

eval EXECUTE p_less    USING @real_0_5;
eval EXECUTE p_less_eq USING @real_0_5;
eval EXECUTE p_greater USING @real_0_5;
eval EXECUTE p_gret_eq USING @real_0_5;
eval EXECUTE p_equal   USING @real_0_5;
eval EXECUTE p_n_equal USING @real_0_5;

eval EXECUTE p_less    USING @real_minus_0_5;
eval EXECUTE p_less_eq USING @real_minus_0_5;
eval EXECUTE p_greater USING @real_minus_0_5;
eval EXECUTE p_gret_eq USING @real_minus_0_5;
eval EXECUTE p_equal   USING @real_minus_0_5;
eval EXECUTE p_n_equal USING @real_minus_0_5;

eval EXECUTE p_less    USING @real_max_plus_delta;
eval EXECUTE p_less_eq USING @real_max_plus_delta;
eval EXECUTE p_greater USING @real_max_plus_delta;
eval EXECUTE p_gret_eq USING @real_max_plus_delta;
eval EXECUTE p_equal   USING @real_max_plus_delta;
eval EXECUTE p_n_equal USING @real_max_plus_delta;

eval EXECUTE p_less    USING @real_max_minus_delta;
eval EXECUTE p_less_eq USING @real_max_minus_delta;
eval EXECUTE p_greater USING @real_max_minus_delta;
eval EXECUTE p_gret_eq USING @real_max_minus_delta;
eval EXECUTE p_equal   USING @real_max_minus_delta;
eval EXECUTE p_n_equal USING @real_max_minus_delta;

eval EXECUTE p_less    USING @real_min_plus_delta;
eval EXECUTE p_less_eq USING @real_min_plus_delta;
eval EXECUTE p_greater USING @real_min_plus_delta;
eval EXECUTE p_gret_eq USING @real_min_plus_delta;
eval EXECUTE p_equal   USING @real_min_plus_delta;
eval EXECUTE p_n_equal USING @real_min_plus_delta;

eval EXECUTE p_less    USING @real_min_minus_delta;
eval EXECUTE p_less_eq USING @real_min_minus_delta;
eval EXECUTE p_greater USING @real_min_minus_delta;
eval EXECUTE p_gret_eq USING @real_min_minus_delta;
eval EXECUTE p_equal   USING @real_min_minus_delta;
eval EXECUTE p_n_equal USING @real_min_minus_delta;

eval EXECUTE p_less    USING @real_too_big_positive_for_any_int;
eval EXECUTE p_less_eq USING @real_too_big_positive_for_any_int;
eval EXECUTE p_greater USING @real_too_big_positive_for_any_int;
eval EXECUTE p_gret_eq USING @real_too_big_positive_for_any_int;
eval EXECUTE p_equal   USING @real_too_big_positive_for_any_int;
eval EXECUTE p_n_equal USING @real_too_big_positive_for_any_int;

eval EXECUTE p_less    USING @real_dec_too_big_negative_for_any_int;
eval EXECUTE p_less_eq USING @real_dec_too_big_negative_for_any_int;
eval EXECUTE p_greater USING @real_dec_too_big_negative_for_any_int;
eval EXECUTE p_gret_eq USING @real_dec_too_big_negative_for_any_int;
eval EXECUTE p_equal   USING @real_dec_too_big_negative_for_any_int;
eval EXECUTE p_n_equal USING @real_dec_too_big_negative_for_any_int;

eval EXECUTE p_less    USING @real_too_big_positive_for_any_decimal;
eval EXECUTE p_less_eq USING @real_too_big_positive_for_any_decimal;
eval EXECUTE p_greater USING @real_too_big_positive_for_any_decimal;
eval EXECUTE p_gret_eq USING @real_too_big_positive_for_any_decimal;
eval EXECUTE p_equal   USING @real_too_big_positive_for_any_decimal;
eval EXECUTE p_n_equal USING @real_too_big_positive_for_any_decimal;

eval EXECUTE p_less    USING @real_too_big_negative_for_any_decimal;
eval EXECUTE p_less_eq USING @real_too_big_negative_for_any_decimal;
eval EXECUTE p_greater USING @real_too_big_negative_for_any_decimal;
eval EXECUTE p_gret_eq USING @real_too_big_negative_for_any_decimal;
eval EXECUTE p_equal   USING @real_too_big_negative_for_any_decimal;
eval EXECUTE p_n_equal USING @real_too_big_negative_for_any_decimal;

eval EXECUTE p_less    USING @real_too_small_positive_for_any_decimal;
eval EXECUTE p_less_eq USING @real_too_small_positive_for_any_decimal;
eval EXECUTE p_greater USING @real_too_small_positive_for_any_decimal;
eval EXECUTE p_gret_eq USING @real_too_small_positive_for_any_decimal;
eval EXECUTE p_equal   USING @real_too_small_positive_for_any_decimal;
eval EXECUTE p_n_equal USING @real_too_small_positive_for_any_decimal;

eval EXECUTE p_less    USING @real_too_small_negative_for_any_decimal;
eval EXECUTE p_less_eq USING @real_too_small_negative_for_any_decimal;
eval EXECUTE p_greater USING @real_too_small_negative_for_any_decimal;
eval EXECUTE p_gret_eq USING @real_too_small_negative_for_any_decimal;
eval EXECUTE p_equal   USING @real_too_small_negative_for_any_decimal;
eval EXECUTE p_n_equal USING @real_too_small_negative_for_any_decimal;

DROP PREPARE p_less;
DROP PREPARE p_less_eq;
DROP PREPARE p_greater;
DROP PREPARE p_gret_eq;
DROP PREPARE p_equal;
DROP PREPARE p_n_equal;

eval DROP TABLE $table_name;
