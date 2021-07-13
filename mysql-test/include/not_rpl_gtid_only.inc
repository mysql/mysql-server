if ( $rpl_gtid_only != '' )
{
  --skip Test cannot run with replication option GTID_ONLY == 1;
}

if ( $GTID_ONLY != '' )
{
  --skip Test cannot run with replication option GTID_ONLY == 1;
}
