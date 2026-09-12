# Copyright 2026 Google LLC

--let $knn=`SELECT id, vector_to_string(emb) FROM t1 ORDER BY vector_distance(emb, string_to_vector('$target_vector')) LIMIT 1`
--let $ann=`SELECT id, vector_to_string(emb) FROM t1 ORDER BY APPROX_DISTANCE(emb, string_to_vector('$target_vector'), 'DISTANCE_MEASURE=L2_SQUARED') LIMIT 1`
--echo Closest KNN Vector: $knn
--echo Closest ANN Vector: $ann
