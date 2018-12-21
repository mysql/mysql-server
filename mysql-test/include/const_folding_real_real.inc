--echo #
--echo # Tests with $signedness type $real_type $nullness against real constants
--echo #
eval CREATE TABLE $table_name (colA $real_type $signedness $nullness);
eval INSERT INTO $table_name VALUES ($minreal), ($minreal_plus_delta_round_up),
                                    ($minreal_plus_delta_round_down),
                                    ($minus_sign 3.3), (0.0), (3.3),
                                    ($maxreal_minus_delta_round_up),
                                    ($maxreal_minus_delta_round_down), ($maxreal),
                                    ($nullval);
eval ANALYZE TABLE $table_name;

eval SELECT * FROM $table_name;

eval SELECT * FROM $table_name WHERE colA <= $minreal;
eval SELECT * FROM $table_name WHERE colA <  $minreal;
eval SELECT * FROM $table_name WHERE colA >= $minreal;
eval SELECT * FROM $table_name WHERE colA >  $minreal;
eval SELECT * FROM $table_name WHERE colA =  $minreal;
eval SELECT * FROM $table_name WHERE colA <> $minreal;

eval SELECT * FROM $table_name WHERE colA <= $minreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <  $minreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >= $minreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >  $minreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA =  $minreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <> $minreal_plus_delta_round_down;

eval SELECT * FROM $table_name WHERE colA <= $minreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <  $minreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >= $minreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >  $minreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA =  $minreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <> $minreal_plus_delta_round_up;

eval SELECT * FROM $table_name WHERE colA <= $minreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <  $minreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >= $minreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >  $minreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA =  $minreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <> $minreal_minus_delta_round_down;

eval SELECT * FROM $table_name WHERE colA <= $minreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <  $minreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >= $minreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >  $minreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA =  $minreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <> $minreal_minus_delta_round_up;

eval SELECT * FROM $table_name WHERE colA <= $maxreal;
eval SELECT * FROM $table_name WHERE colA <  $maxreal;
eval SELECT * FROM $table_name WHERE colA >= $maxreal;
eval SELECT * FROM $table_name WHERE colA >  $maxreal;
eval SELECT * FROM $table_name WHERE colA =  $maxreal;
eval SELECT * FROM $table_name WHERE colA <> $maxreal;

eval SELECT * FROM $table_name WHERE colA <= $maxreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <  $maxreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >= $maxreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >  $maxreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA =  $maxreal_minus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <> $maxreal_minus_delta_round_down;

eval SELECT * FROM $table_name WHERE colA <= $maxreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <  $maxreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >= $maxreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >  $maxreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA =  $maxreal_minus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <> $maxreal_minus_delta_round_up;

eval SELECT * FROM $table_name WHERE colA <= $maxreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <  $maxreal_plus_delta_round_down;
--echo Result here can be empty set, because MySQL performs operation in
--echo double, e.g. colA >= 999.994  for FLOAT(5,2):
--echo executes as     nnn.nn0 >= 999.994, i.e. the .994 isn't truncated
--echo to .99 before comparison
eval SELECT * FROM $table_name WHERE colA >= $maxreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA >  $maxreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA =  $maxreal_plus_delta_round_down;
eval SELECT * FROM $table_name WHERE colA <> $maxreal_plus_delta_round_down;

eval SELECT * FROM $table_name WHERE colA <= $maxreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <  $maxreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >= $maxreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA >  $maxreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA =  $maxreal_plus_delta_round_up;
eval SELECT * FROM $table_name WHERE colA <> $maxreal_plus_delta_round_up;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $minreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $minreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $minreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minreal;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $minreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $minreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $minreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minreal_plus_delta_round_down;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $minreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $minreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $minreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minreal_plus_delta_round_up;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $minreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $minreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $minreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minreal_minus_delta_round_down;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $minreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $minreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $minreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $minreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $minreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $minreal_minus_delta_round_up;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $maxreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $maxreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $maxreal;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxreal;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $maxreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $maxreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $maxreal_minus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxreal_minus_delta_round_down;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $maxreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $maxreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $maxreal_minus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxreal_minus_delta_round_up;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $maxreal_plus_delta_round_down;
--echo Cf explanation of semantics here under the SELECT above. The operator
--echo is modified here to get the same semantics: we need > instead of >=
--echo since we truncate the constant to the allowed range
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $maxreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $maxreal_plus_delta_round_down;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxreal_plus_delta_round_down;

eval EXPLAIN SELECT * FROM $table_name WHERE colA <= $maxreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <  $maxreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >= $maxreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA >  $maxreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA =  $maxreal_plus_delta_round_up;
eval EXPLAIN SELECT * FROM $table_name WHERE colA <> $maxreal_plus_delta_round_up;

eval DROP TABLE $table_name;

# We test string constants by changing the variables used by this script, but test
# garbage here:
CREATE TABLE f(f FLOAT);
INSERT INTO f VALUES (-1), (2);
SELECT * FROM f WHERE f >= '2 garbage';
DROP TABLE f;
