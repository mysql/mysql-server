# CREATE FUNCTION USING func_or_trig_body PROVIDED BY MAIN TEST SCRIPT
--let $header=CREATE FUNCTION func1 () RETURNS varchar(30) CHARSET utf8
--let $begin=BEGIN
--let $return=RETURN 0;
--let $end=END
--let $cmd=`select CONCAT("$header", "\n","$begin","\n", "$func_or_trig_body","\n", "$return", "\n","$end")`
--eval $cmd

# USE THAT FUNCTION IN SELECT QUERY
--error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION
SELECT func1();

# USE THAT FUNCTION IN SELECT QUERY IN MULTI TRANSACTION STATEMENT
START TRANSACTION;
--error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION
SELECT func1();
COMMIT;

# iter 1 : INSERT/UPDATE/DELETE QUERY ON transaction table USING THAT FUNCTION
# iter 2 : INSERT/UPDATE/DELETE QUERY ON non transaction table USING THAT FUNCTION
--let $iter=1
while ($iter <=2)
{
  if ($iter == 1)
  {
    --let $table=trans_table2
  }
  if ($iter == 2)
  {
    --let $table=non_trans_table2
  }
  --let $insert_cmd= INSERT INTO $table VALUES (func1())
  --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
  --eval $insert_cmd

  --let $update_cmd= UPDATE $table SET i=func1()
  --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
  --eval $update_cmd

  --let $delete_cmd= DELETE FROM $table WHERE i=func1()
  --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
  --eval $delete_cmd

  --inc $iter
}

# DELETE THAT FUNCTION
DROP FUNCTION func1;

# CREATE A TRIGGER USING func_or_trig_body PROVIDED BY MAIN TEST SCRIPT
# iter1:  BEFORE INSERT ON     transaction table
# iter2:  AFTER  INSERT ON     transaction table
# iter3:  BEFORE UPDATE ON     transaction table
# iter4:  AFTER  UPDATE ON     transaction table
# iter5:  BEFORE DELETE ON     transaction table
# iter6:  AFTER  DELETE ON     transaction table
# iter7:  BEFORE INSERT ON non transaction table
# iter8:  AFTER  INSERT ON non transaction table
# iter9:  BEFORE UPDATE ON non transaction table
# iter10: AFTER  UPDATE ON non transaction table
# iter11: BEFORE DELETE ON non transaction table
# iter12: AFTER  DELETE ON non transaction table

# And all 12 iterations should throw an error
--let $iter=1
while ($iter <= 12)
{
  --let $header=CREATE TRIGGER trigger1
  --let $for_each_row= FOR EACH ROW
  --let $begin=BEGIN
  --let $end=END

  --let $table=trans_table2
  if ($iter >= 7)
  {
   --let $table=non_trans_table2
  }

  --let $before_or_after=BEFORE
  if (`SELECT $iter IN (2, 4, 6, 8, 10, 12)`)
  {
   --let $before_or_after=AFTER
  }

  --let $insert_or_update_or_delete=INSERT
  if (`SELECT $iter IN (3, 4, 9, 10)`)
  {
   --let $insert_or_update_or_delete=UPDATE
  }
  if (`SELECT $iter IN (5, 6, 11, 12)`)
  {
   --let $insert_or_update_or_delete=DELETE
  }

  --let $cmd=`select CONCAT("$header", "\n", "$before_or_after", " ", "$insert_or_update_or_delete", " ON ", "$table", " ", "$for_each_row", "\n", "$begin","\n", "$func_or_trig_body","\n", "$end")`
  --eval $cmd

  if (`SELECT $iter IN (1, 2, 7, 8)`)
  {
    --let $insert_cmd= INSERT INTO $table VALUES (10)
    --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
    --eval $insert_cmd
  }

  if (`SELECT $iter IN (3, 4, 9, 10)`)
  {
    --let $update_cmd= UPDATE $table SET i=12
    --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
    --eval $update_cmd
  }

  if (`SELECT $iter IN (5, 6, 11, 12)`)
  {
    --let $delete_cmd= DELETE FROM $table
    --error ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE
    --eval $delete_cmd
  }
   DROP TRIGGER trigger1;
  --inc $iter
}
