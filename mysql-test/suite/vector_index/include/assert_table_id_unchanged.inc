# Copyright 2026 Google LLC


--let $assert_text= Verify table_id is not changed
--let $assert_cond= [select table_id from information_schema.innodb_tables where name = "test/$table_name"] = $_table_id_
--source include/assert.inc

--let $assert_text= Verify space_id is not changed
--let $assert_cond= [select space from information_schema.innodb_tables where name = "test/$table_name"] = $_space_id_
--source include/assert.inc

