eval ALTER TABLE $table_name ADD $new_col INT DEFAULT 1, ALGORITHM=INSTANT;
eval SELECT * FROM $table_name LIMIT 15;

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con1;
--echo # Prepare the new instant column
send;
eval ALTER TABLE $table_name DROP PRIMARY KEY, ADD PRIMARY KEY ($pk_col, $new_col);

connection con2;
--echo # Run the DML in online
send;
eval DELETE FROM $table_name WHERE $pk_col=1;
reap;

connection con1;
reap;

connection default;
disconnect con1;
disconnect con2;

eval SELECT * FROM $table_name LIMIT 15;
eval SHOW CREATE TABLE $table_name;
eval CHECK TABLE $table_name EXTENDED;
eval DROP TABLE $table_name;
