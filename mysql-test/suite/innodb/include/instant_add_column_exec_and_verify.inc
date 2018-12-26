
--let $old_table_id = `SELECT table_id FROM information_schema.innodb_tables WHERE name like '%t1%';`

--let $old_instant_cols = `SELECT instant_cols FROM information_schema.innodb_tables WHERE name like '%t1%';`

--eval $instant_add_column

--let $table_id = `SELECT table_id FROM information_schema.innodb_tables WHERE name like '%t1%';`

--let $instant_cols = `SELECT instant_cols FROM information_schema.innodb_tables WHERE name like '%t1%';`

if ($old_table_id != $table_id)
{
  --echo Table ID differed
}

if ($old_table_id == $table_id)
{
  --echo Table id did not change
}

if ($old_instant_cols == 0)
{
  --disable_query_log
  --eval SELECT count(*) = $new_cols FROM information_schema.innodb_columns WHERE table_id = $table_id AND has_default = 1;
  --enable_query_log
}

if ($old_instant_cols != 0)
{
--eval SELECT $old_instant_cols = instant_cols AS `Instant columns equal` FROM information_schema.innodb_tables WHERE name like '%t1%'
}

CHECK TABLE t1;
