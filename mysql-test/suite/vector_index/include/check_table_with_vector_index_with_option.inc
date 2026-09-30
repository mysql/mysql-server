# Copyright 2026 Google LLC

# check table with no vector index
--eval CHECK TABLE $table_name QUICK FAST

--eval SELECT T.NAME, I.STATE, I.CORRUPTED  FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = 'test/$table_name'

# Create vector index with all options
--eval CREATE VECTOR INDEX $index_name ON $table_name(j) USING TREE QUANTIZER=SQ8 DISTANCE_MEASURE=l2_squared NUM_LEAVES=10

--eval SELECT T.NAME, I.STATE, I.CORRUPTED  FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = 'test/$table_name'

# check table with vector index
--eval CHECK TABLE $table_name QUICK FAST

--eval SELECT T.NAME, I.STATE, I.CORRUPTED  FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = 'test/$table_name'

# add new rows to table
--eval INSERT INTO $table_name (j) VALUES (string_to_vector('[2,3,4]'))
--eval INSERT INTO $table_name (j) VALUES (string_to_vector('[4,5,6]'))

# check table with vector index after adding new rows
--eval CHECK TABLE $table_name QUICK FAST

--eval SELECT T.NAME, I.STATE, I.CORRUPTED  FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = 'test/$table_name'

# clean up the index
--eval DROP INDEX $index_name ON $table_name

# check table after dropping vector index
--eval CHECK TABLE $table_name QUICK FAST

--eval SELECT T.NAME, I.STATE, I.CORRUPTED  FROM information_schema.innodb_all_vector_indexes I JOIN information_schema.innodb_tables T ON I.table_id = T.table_id WHERE T.name = 'test/$table_name'

# clean up the table
--eval DROP TABLE if exists $table_name
