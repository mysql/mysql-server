eval CREATE TABLE $table_name (y YEAR $nullness);
eval INSERT INTO $table_name VALUES (0), (1901), (2018), (2155);
eval ANALYZE TABLE $table_name;
eval SELECT * FROM $table_name;

let $constant=$zero;
let $colname=y;
--source include/const_folding_cmp_ops.inc
let $constant=$zero_minus_delta;
--source include/const_folding_cmp_ops.inc
let $constant=$zero_plus_delta;
--source include/const_folding_cmp_ops.inc
let $constant=$in_hole;
--source include/const_folding_cmp_ops.inc
let $constant=$min_minus_delta;
--source include/const_folding_cmp_ops.inc
let $constant=$min;
--source include/const_folding_cmp_ops.inc
let $constant=$min_plus_delta;
--source include/const_folding_cmp_ops.inc
let $constant=$inside;
--source include/const_folding_cmp_ops.inc
let $constant=$max_minus_delta;
--source include/const_folding_cmp_ops.inc
let $constant=$max;
--source include/const_folding_cmp_ops.inc
let $constant=$max_plus_delta;
--source include/const_folding_cmp_ops.inc

eval DROP TABLE $table_name;
