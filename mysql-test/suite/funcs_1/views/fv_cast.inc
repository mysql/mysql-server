--disable_query_log
eval SET @my_select = 'SELECT CAST($col_type AS $target_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
