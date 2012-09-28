# Adding the replace_column since the key length varies depending on the optimizer choice
--replace_column 9 #
eval EXPLAIN EXTENDED $query1;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3;
--replace_column 9 #
eval EXPLAIN EXTENDED $query4;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5;
--replace_column 9 #
eval EXPLAIN EXTENDED $query6;
--replace_column 9 #
eval EXPLAIN EXTENDED $query7;
--replace_column 9 #
eval EXPLAIN EXTENDED $query8;
--replace_column 9 #
eval EXPLAIN EXTENDED $query9;
--replace_column 9 #
eval EXPLAIN EXTENDED $query10;
--replace_column 9 #
eval EXPLAIN EXTENDED $query11;
--replace_column 9 #
eval EXPLAIN EXTENDED $query12;
--replace_column 9 #
eval EXPLAIN EXTENDED $query13;
--replace_column 9 #
eval EXPLAIN EXTENDED $query14;
--replace_column 9 #
eval EXPLAIN EXTENDED $query15;
--replace_column 9 #
eval EXPLAIN EXTENDED $query16;
--replace_column 9 #
eval EXPLAIN EXTENDED $query17;
--replace_column 9 #
eval EXPLAIN EXTENDED $query18;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2a;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2b;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2c;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2d;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2e;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2f;
--replace_column 9 #
eval EXPLAIN EXTENDED $query2g;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3a;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3b;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3c;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3d;
--replace_column 9 #
eval EXPLAIN EXTENDED $query3e;
--replace_column 9 #
eval EXPLAIN EXTENDED $query4a;
--replace_column 9 #
eval EXPLAIN EXTENDED $query4b;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5a;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5b;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5c;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5d;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5e;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5f;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5g;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5h;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5i;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5j;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5k;
--replace_column 9 #
eval EXPLAIN EXTENDED $query5l;
--replace_column 9 #
eval EXPLAIN EXTENDED $query6a;

eval $query1;
eval $query_s1;
ROLLBACK;
eval $query2;
eval $query_s1;
ROLLBACK;
eval $query3;
eval $query_s2;
ROLLBACK;
eval $query4;
eval $query_s2;
ROLLBACK;
eval $query5;
eval $query_s1;
ROLLBACK;
eval $query6;
eval $query_s1;
ROLLBACK;
eval $query7;
eval $query_s1;
ROLLBACK;
eval $query8;
eval $query_s2;
ROLLBACK;
eval $query9;
eval $query_s1;
ROLLBACK;
eval $query10;
eval $query_s3;
eval $query_s4;
ROLLBACK;
eval $query11;
eval $query_s3;
ROLLBACK;
eval $query12;
eval $query_s3;
eval $query_s4;
ROLLBACK;
eval $query13;
eval $query_s3;
eval $query_s4;
ROLLBACK;
eval $query14;
eval $query_s3;
eval $query_s4;
ROLLBACK;
eval $query15;
eval $query_s3;
ROLLBACK;
eval $query16;
eval $query_s1;
ROLLBACK;
eval $query17;
eval $query_s1;
ROLLBACK;
eval $query18;
eval $query_s1;
ROLLBACK;

eval $query2a;
eval $query2_s;
ROLLBACK;
eval $query2b;
eval $query2_s;
ROLLBACK;
eval $query2c;
eval $query2_s;
ROLLBACK;
eval $query2d;
eval $query2_s;
ROLLBACK;
eval $query2e;
eval $query2_s;
ROLLBACK;
eval $query2f;
eval $query2_s;
ROLLBACK;
eval $query2g;
eval $query2_s;
ROLLBACK;

eval $query3a;
eval $query2_s;
ROLLBACK;
eval $query3b;
eval $query2_s;
ROLLBACK;
eval $query3c;
eval $query2_s;
ROLLBACK;
eval $query3d;
eval $query2_s;
ROLLBACK;
eval $query3e;
eval $query2_s;
ROLLBACK;

eval $query4a;
eval $query4_s1;
eval $query4_s2;
ROLLBACK;
eval $query4b;
eval $query4_s1;
eval $query4_s2;
ROLLBACK;

eval $query5a;
eval $query5_s1;
ROLLBACK;
eval $query5b;
eval $query5_s1;
ROLLBACK;
eval $query5c;
eval $query5_s1;
ROLLBACK;
eval $query5d;
eval $query5_s1;
ROLLBACK;
eval $query5e;
eval $query5_s1;
ROLLBACK;
eval $query5f;
eval $query5_s2;
ROLLBACK;
eval $query5g;
eval $query5_s2;
ROLLBACK;
eval $query5h;
eval $query5_s1;
ROLLBACK;
eval $query5i;
eval $query5_s1;
ROLLBACK;
eval $query5j;
eval $query5_s1;
ROLLBACK;
eval $query5k;
eval $query5_s1;
ROLLBACK;
eval $query5l;
eval $query5_s1;
ROLLBACK;

eval $query6a;
SELECT * FROM tb1;
ROLLBACK;

