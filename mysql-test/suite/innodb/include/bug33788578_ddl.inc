--source suite/innodb/include/print_instant_part_metadata.inc
--eval SHOW CREATE TABLE $table_name;
--eval SELECT * FROM $table_name;
--eval CHECK TABLE $table_name;

--eval ALTER TABLE $table_name ADD PRIMARY KEY ($pk_col);

--source suite/innodb/include/print_instant_part_metadata.inc
--eval SHOW CREATE TABLE $table_name;
--eval SELECT * FROM $table_name;
--eval CHECK TABLE $table_name;
