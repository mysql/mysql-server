# Copyright 2026 Google LLC

--disable_query_log
--let $top10=`SELECT GROUP_CONCAT(i) FROM (SELECT i FROM $table_name ORDER BY APPROX_DISTANCE(j, string_to_vector('[1,2,3]'), 'distance_measure=l2_squared') LIMIT 10) x`
--assert($top10 == "35,628,8,201,284,559,798,152,651,269")
--eval INSERT INTO $table_name (i, j) VALUES (1005, string_to_vector("[1,2,3]"))
--let $top10=`SELECT GROUP_CONCAT(i) FROM (SELECT i FROM $table_name ORDER BY APPROX_DISTANCE(j, string_to_vector('[1,2,3]'), 'distance_measure=l2_squared') LIMIT 10) x`
--assert($top10 == "1005,35,628,8,201,284,559,798,152,651")
--eval DELETE FROM $table_name WHERE i = 1005
--let $top10=`SELECT GROUP_CONCAT(i) FROM (SELECT i FROM $table_name ORDER BY APPROX_DISTANCE(j, string_to_vector('[1,2,3]'), 'distance_measure=l2_squared') LIMIT 10) x`
--assert($top10 == "35,628,8,201,284,559,798,152,651,269")
--echo "ANN Verified"
--enable_query_log
