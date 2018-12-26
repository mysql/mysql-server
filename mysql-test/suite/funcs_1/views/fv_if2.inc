--disable_query_log
eval SET @my_select = 'SELECT IF($col_type IS NULL, ''IS     NULL'',
''IS NOT NULL''), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
