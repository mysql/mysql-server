# Copyright 2026 Google LLC

--eval SET SESSION debug='+d,skip_dd_table_access_check'

--let $_table_id_in_hex_= `select lpad(hex(table_id), 16, '0') from information_schema.innodb_tables where name = "test/$table_name"`

--let $assert_text= No trace in mysql.indexes should be present for the index
--let $assert_cond= [select count(*) FROM mysql.indexes WHERE name = "$index_name"] = 0
--source include/assert.inc

--let $assert_text= No trace in information_schema.innodb_vector_indexes
--let $assert_cond= [SELECT count(*) FROM information_schema.innodb_vector_indexes WHERE index_name = "$index_name"] = 0
--source include/assert.inc

--let $assert_text= No trace in information_schema.innodb_all_vector_indexes
--let $assert_cond= [SELECT count(*) FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = "test/$table_name"] = 0
--source include/assert.inc

--let $assert_text= No trace in information_schema.innodb_tables
--let $assert_cond= [SELECT count(*) FROM information_schema.innodb_tables where name like "test/vector_index_$_table_id_in_hex_%";] = 0  
--source include/assert.inc

--let $assert_text= No trace in information_schema.innodb_datafiles
--let $assert_cond= [SELECT count(*) FROM information_schema.innodb_datafiles where path like "%test/vector_index_$_table_id_in_hex_%";] = 0  
--source include/assert.inc


