--echo ===============
--echo Check the stats of the table
--echo Check the size of the ibd file
--echo ===============

-- echo # testcase : pass = 1 fail = 0
SELECT count(*) > 0 as "compress_stat"
FROM information_schema.innodb_cmp_per_index
WHERE
compress_ops_ok BETWEEN @inl_val AND 1000
AND compress_ops BETWEEN @inl_val AND 1000
AND table_name='tab5' AND database_name='test'
AND index_name like 'idx%' ;

perl;
 my $dir = $ENV{'MYSQLD_DATADIR'}."test";
 my $size;
 opendir(DIR, $dir) or die $!;
 while (my $file = readdir(DIR))
 {

   next unless ($file =~ m/\.ibd$/);
   $size = -s "$dir/$file";
   print "The size of the tab5.ibd file: $size\n";
 }
 close(DIR);
 exit(0)
EOF
