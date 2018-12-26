###########################################################################
# $select_column             : Column to be fetched in SELECT             #
# $column_count              : Number of columns in the Index             #
# $col1, $col2 ...           : Columns' name in the Index                 #
# $col1_act, $col2_act ...   : Actual existing values of columns in table #
# $table                     : Performance Schema table name              #
###########################################################################

--echo

IF ($column_count == 1)
{
  --echo ====================================================================
  --echo Testing index for columns $col1
  --echo ====================================================================
  --disable_warnings
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column 
    from $table 
    where $col1 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act;
  
  --echo ############# Explain End #########################################
  --enable_warnings
  
  flush status;
  let $count1 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  --replace_column 1 #
  eval select $select_column
    from $table
    where $col1 = $col1_act;
  
  let $count2 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  if ($count1 < $count2)
  {
    --echo OK: handler_read_key incremented
  }
  if ($count1 >= $count2)
  {
    --echo ***ERROR*** handler_read_key: Before= $count1, After= $count2
  }
}

IF ($column_count == 2)
{
  --echo ====================================================================
  --echo Testing index for columns $col1, $col2
  --echo ====================================================================
  --disable_warnings
  
  ############################
  # For Col1                 #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act;
  
  ############################
  # For Col1 and col2 both   #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act;
  
  --echo ############# Explain End #########################################
  --enable_warnings
  
  flush status;
  let $count1 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  --replace_column 1 #
  eval select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act;

  let $count2 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  if ($count1 < $count2)
  {
    --echo OK: handler_read_key incremented
  }
  if ($count1 >= $count2)
  {
    --echo ***ERROR*** handler_read_key: Before= $count1, After= $count2
  }
}

IF ($column_count == 3)
{
  --echo ====================================================================
  --echo Testing index for columns $col1, $col2, $col3
  --echo ====================================================================
  
  --disable_warnings
  
  ############################
  # For Col1                 #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act;
  
  ############################
  # For Col1 col2            #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act;
  
  ############################
  # For Col1 col2 col3       #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible"
    and $col3 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act;
  
  --echo ############# Explain End #########################################
  
  flush status;
  let $count1 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  --replace_column 1 #
  eval select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act;

  let $count2 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  if ($count1 < $count2)
  {
    --echo OK: handler_read_key incremented
  }
  if ($count1 >= $count2)
  {
    --echo ***ERROR*** handler_read_key: Before= $count1, After= $count2
  }
}

IF ($column_count == 4)
{
  --echo ====================================================================
  --echo Testing index for columns $col1, $col2, $col3, $col4
  --echo ====================================================================
  --disable_warnings
  
  ############################
  # For Col1                 #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act;
  
  ############################
  # For Col1 col2            #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act;
  
  ############################
  # For Col1 col2 col3       #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible"
    and $col3 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act;
  
  ############################
  # For Col1 col2 col3 col4  #
  ############################
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = "impossible"
    and $col2 = "impossible"
    and $col3 = "impossible"
    and $col4 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act
    and $col4 = "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act
    and $col4 > "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act
    and $col4 < "impossible";
  
  --echo ############ Explain for Query ####################################
  --replace_column 10 #
  eval explain select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act
    and $col4 = $col4_act;
  
  --echo ############# Explain End #########################################
  
  flush status;
  let $count1 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  --replace_column 1 #
  eval select $select_column
    from $table
    where $col1 = $col1_act
    and $col2 = $col2_act
    and $col3 = $col3_act
    and $col4 = $col4_act;

  let $count2 = query_get_value(show session status like "handler_read_key", Value, 1);
  
  if ($count1 < $count2)
  {
    --echo OK: handler_read_key incremented
  }
  if ($count1 >= $count2)
  {
    --echo ***ERROR*** handler_read_key: Before= $count1, After= $count2
  }
}

IF ($column_count == 5)
{
--echo Testing index for columns $col1, $col2, $col3, $col4, $col5
}
