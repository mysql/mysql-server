# Copyright 2026 Google LLC

--let $_table_id_= `select table_id from information_schema.innodb_tables where name = "test/$table_name"`
--let $_space_id_= `select space from information_schema.innodb_tables where name = "test/$table_name"`
