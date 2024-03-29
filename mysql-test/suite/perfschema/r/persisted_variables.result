# Test persisted variables
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
SET PERSIST foreign_key_checks=0;
SET PERSIST flush_time=2;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
flush_time	2
foreign_key_checks	OFF
SET PERSIST sort_buffer_size=256000;
SET PERSIST max_heap_table_size=999424, replica_net_timeout=124;
SET PERSIST innodb_default_row_format=COMPACT;
SET @@persist.max_execution_time=44000, @@persist.max_user_connections=30;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
flush_time	2
foreign_key_checks	OFF
innodb_default_row_format	compact
max_execution_time	44000
max_heap_table_size	999424
max_user_connections	30
replica_net_timeout	124
slave_net_timeout	124
sort_buffer_size	256000
SET PERSIST foreign_key_checks=DEFAULT;
SET PERSIST flush_time=DEFAULT;
SET PERSIST sort_buffer_size=DEFAULT;
SET PERSIST max_heap_table_size=DEFAULT, replica_net_timeout=DEFAULT;
SET PERSIST innodb_default_row_format=DEFAULT;
SET @@persist.max_execution_time=DEFAULT, @@persist.max_user_connections=DEFAULT;
RESET PERSIST;
SELECT 'END OF TEST';
END OF TEST
END OF TEST
