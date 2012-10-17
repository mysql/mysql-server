--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_1;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_2;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_3;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_4;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_6;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_7;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_8;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_9;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_10;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_11;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_12;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_13;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_14;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5a;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5b;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5c;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5d;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5e;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5f;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $queryd_5g;
--replace_column 9 # 10 #
eval EXPLAIN EXTENDED $query6b;

eval $queryd_1;
eval $query_s1;
ROLLBACK;
eval $queryd_2;
eval $query_s1;
ROLLBACK;
eval $queryd_3;
eval $query_s1;
ROLLBACK;
eval $queryd_4;
eval $query_s1;
ROLLBACK;
eval $queryd_5;
eval $query_s1;
ROLLBACK;
eval $queryd_6;
eval $query_s1;
ROLLBACK;
eval $queryd_7;
eval $query_s2;
ROLLBACK;
eval $queryd_8;
eval $query_s2;
ROLLBACK;
eval $queryd_9;
eval $query_s5;
ROLLBACK;
eval $queryd_10;
eval $query_s5;
ROLLBACK;
eval $queryd_11;
eval $query_s5;
ROLLBACK;
eval $queryd_12;
eval $query_s5;
ROLLBACK;
eval $queryd_13;
eval $query_s5;
ROLLBACK;
eval $queryd_14;
eval $query_s5;
ROLLBACK;

eval $queryd_5a;
eval $query5_s1;
ROLLBACK;
eval $queryd_5b;
eval $query5_s1;
ROLLBACK;
eval $queryd_5c;
eval $query5_s1;
ROLLBACK;
eval $queryd_5d;
eval $query5_s1;
ROLLBACK;
eval $queryd_5e;
eval $query5_s1;
ROLLBACK;
eval $queryd_5f;
eval $query5_s2;
ROLLBACK;
eval $queryd_5g;
eval $query5_s2;
ROLLBACK;

eval $query6b;
SELECT * FROM tb1;
ROLLBACK;

