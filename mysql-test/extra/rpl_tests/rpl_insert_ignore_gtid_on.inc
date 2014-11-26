if (!`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'GTID_MODE' AND VARIABLE_VALUE = 'ON'`)
{
  --let $i=1
  --let $j=1
  while ($i)
  {
    --let $temp= query_get_value("SHOW BINLOG EVENTS", Pos, $j)
    --let $temp2=`select $binlog_start - $temp`
    if($temp2==0)
    {
      --let $binlog_offset= query_get_value(SHOW BINLOG EVENTS, End_log_pos, $j)
      dec $i;
    }
   inc $j;
  }
  --let $binlog_start= $binlog_offset
}
