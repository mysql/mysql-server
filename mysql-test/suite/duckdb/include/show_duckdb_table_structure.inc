--disable_query_log
--let $check_constraint = 1
--let $check_index = 1
--let $check_sequence = 1

--echo ------------------------------------------------------------------------
--echo ## ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ Print $TABLE_NAME begin ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ ##
--echo # ① Print columns of $TABLE_NAME ##
--let $sql=call dbms_duckdb.query("SELECT column_name, column_index, data_type, numeric_precision FROM duckdb_columns() WHERE schema_name = '$db_name' AND table_name = '$table_name'")
--eval $sql

-- if ($check_constraint) {
--echo # ② Print CONSTRAINTs of $TABLE_NAME ##
--let $sql=call dbms_duckdb.query("SELECT table_name, constraint_type, constraint_text FROM duckdb_constraints() WHERE schema_name = '$db_name' AND table_name = '$table_name'")
--eval $sql
--}

-- if ($check_index) {
--echo # ③ Print INDEXs of $TABLE_NAME ##
--let $sql=call dbms_duckdb.query("SELECT index_name, comment, tags, is_unique, is_primary, expressions FROM duckdb_indexes()  WHERE schema_name = '$db_name' AND table_name = '$table_name'")
--eval $sql
--}

-- if ($check_sequence) {
--echo # ④ Print SEQUENCE of $TABLE_NAME ##
--let $sql=call dbms_duckdb.query("SELECT sequence_name, min_value, increment_by, last_value FROM duckdb_sequences() WHERE schema_name='$db_name' AND sequence_name LIKE 'seq_$table_name%'")
--eval $sql
--}

--echo ## ↑↑↑↑↑↑↑↑↑↑↑↑↑↑ Print table $TABLE_NAME end ↑↑↑↑↑↑↑↑↑↑↑↑↑↑ ##
--echo ------------------------------------------------------------------------

--enable_query_log
