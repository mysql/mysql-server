--source suite/innodb/include/print_instant_part_metadata.inc
--eval SELECT * FROM $table_name;
--eval SHOW CREATE TABLE $table_name;
--eval CHECK TABLE $table_name;

connect (con1,localhost,root,,);

connection con1;
SET DEBUG_SYNC='innodb_inplace_alter_table_enter SIGNAL update_now WAIT_FOR update_done';
send;
eval ALTER TABLE $table_name ADD PRIMARY KEY ($pk_col);

connection default;
SET DEBUG_SYNC='now WAIT_FOR update_now';

--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;

eval UPDATE $table_name SET $upd_col = 1;

--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;
SET DEBUG_SYNC='now SIGNAL update_done';

connection con1;
reap;

connection default;
disconnect con1;

--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;

eval DROP TABLE $table_name;
