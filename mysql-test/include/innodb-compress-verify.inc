### verify tables are compressed as expected
eval FLUSH TABLES $db.$table_zlib, $db.$table_lz4 WITH READ LOCK;
UNLOCK TABLES;
--let $s1=`SELECT name,file_size,allocated_size FROM $i_s_table WHERE name LIKE '$db/$table_zlib'`
--let $s2=`SELECT name,file_size,allocated_size FROM $i_s_table WHERE name LIKE '$db/$table_lz4'`
--let COMPR_SIZES= $s1,$s2
perl;
  do 'include/innodb-compress-utils.inc';
  table_compression_verify();
EOF
