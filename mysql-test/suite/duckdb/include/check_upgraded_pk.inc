SHOW CREATE TABLE t;
CALL dbms_duckdb.query("SELECT table_schema, table_name, column_name, column_default, is_nullable, data_type, ordinal_position FROM information_schema.columns WHERE table_name = 't'");
CALL dbms_duckdb.query("SELECT table_name, constraint_type, constraint_text FROM duckdb_constraints WHERE table_name = 't'");
--echo
