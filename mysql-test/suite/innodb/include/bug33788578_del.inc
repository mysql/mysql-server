--eval SELECT * FROM $table_name;
--eval SHOW CREATE TABLE $table_name;
--eval CHECK TABLE $table_name;

--eval UPDATE $table_name SET $pk_col = 1 WHERE $sel_col = '$sel_val';

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

eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;

eval DELETE FROM $table_name WHERE $del_col = '$del_val';

eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;

SET DEBUG_SYNC='now SIGNAL update_done';

connection con1;
reap;

connection default;
disconnect con1;

eval SELECT * FROM $table_name;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name;

eval DROP TABLE $table_name;
