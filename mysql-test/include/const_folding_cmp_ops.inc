eval SELECT * FROM $table_name WHERE $colname <  $constant;
eval SELECT * FROM $table_name WHERE $colname <= $constant;
eval SELECT * FROM $table_name WHERE $colname >  $constant;
eval SELECT * FROM $table_name WHERE $colname >= $constant;
eval SELECT * FROM $table_name WHERE $colname =  $constant;
eval SELECT * FROM $table_name WHERE $colname <> $constant;

eval EXPLAIN  SELECT * FROM $table_name WHERE $colname <  $constant;
eval EXPLAIN  SELECT * FROM $table_name WHERE $colname <= $constant;
eval EXPLAIN  SELECT * FROM $table_name WHERE $colname >  $constant;
eval EXPLAIN  SELECT * FROM $table_name WHERE $colname >= $constant;
eval EXPLAIN  SELECT * FROM $table_name WHERE $colname =  $constant;
eval EXPLAIN  SELECT * FROM $table_name WHERE $colname <> $constant;

