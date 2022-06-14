###########################################################################
# $select_column             : Column to be fetched in SELECT             #
# $column_count              : Number of columns in the Index             #
# $col1, $col2 ...           : Columns' name in the Index                 #
# $col1_act, $col2_act ...   : Actual existing values of columns in table #
# $table                     : Performance Schema table name              #
###########################################################################

--echo

--echo ====================================================================
--echo Testing index for columns $col1
--echo ====================================================================
--disable_warnings

--echo ############ Explain for Query ####################################
--replace_column 10 #
eval EXPLAIN SELECT $select_column 
  FROM $table 
  WHERE $col1 = $col1_act;

--echo ############ Explain for Query ####################################
--replace_column 10 #
eval EXPLAIN SELECT $select_column
  FROM $table
  WHERE $col1 > $col1_act;

--echo ############ Explain for Query ####################################
--replace_column 10 #
eval EXPLAIN SELECT $select_column
  FROM $table
  WHERE $col1 < $col1_act;

--echo ############ Explain for Query ####################################
--replace_column 10 #
eval EXPLAIN SELECT $select_column
  FROM $table
  WHERE $col1 != $col1_act;

--echo ############# Explain End #########################################
--enable_warnings
